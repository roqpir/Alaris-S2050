/* Local TIFF -> PDF conversion using macOS frameworks; no external service. */
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

typedef struct { int fd,failed; } PDFOutput;
static size_t pdf_write_bytes(void *info,const void *buffer,size_t count) {
    PDFOutput *out=info; const unsigned char *p=buffer; size_t done=0;
    while (done<count) {
        ssize_t n=write(out->fd,p+done,count-done);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) { out->failed=1; break; }
        done+=(size_t)n;
    }
    return done;
}
/* Writes atomically, never overwrites an existing destination. Paths are in
 * explicit page order. Empty/invalid/incomplete input cannot publish a PDF. */
static int pdf_from_tiffs(const char *destination,const char *const *paths,unsigned count) {
    if (!count || count>100) return 1;
    char temporary[PATH_MAX];
    int n=snprintf(temporary,sizeof(temporary),"%s.tmp-XXXXXX",destination);
    if (n<0 || (size_t)n>=sizeof(temporary)) return 1;
    PDFOutput out={.fd=mkstemp(temporary),.failed=0};
    if (out.fd<0) { perror("Create PDF"); return 1; }
    CGDataConsumerCallbacks callbacks={pdf_write_bytes,NULL};
    CGDataConsumerRef consumer=CGDataConsumerCreate(&out,&callbacks);
    CGContextRef context=consumer?CGPDFContextCreate(consumer,NULL,NULL):NULL;
    int failed=!context;
    for (unsigned i=0;!failed && i<count;i++) {
        CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8 *)paths[i],strlen(paths[i]),false);
        CGImageSourceRef source=url?CGImageSourceCreateWithURL(url,NULL):NULL;
        CGImageRef image=source?CGImageSourceCreateImageAtIndex(source,0,NULL):NULL;
        CFDictionaryRef props=source?CGImageSourceCopyPropertiesAtIndex(source,0,NULL):NULL;
        double dx=0,dy=0;
        CFTypeRef x=props?CFDictionaryGetValue(props,kCGImagePropertyDPIWidth):NULL;
        CFTypeRef y=props?CFDictionaryGetValue(props,kCGImagePropertyDPIHeight):NULL;
        if (!image || !x || !y || CFGetTypeID(x)!=CFNumberGetTypeID() || CFGetTypeID(y)!=CFNumberGetTypeID() ||
            !CFNumberGetValue(x,kCFNumberDoubleType,&dx) || !CFNumberGetValue(y,kCFNumberDoubleType,&dy) ||
            dx<1 || dx>9600 || dy<1 || dy>9600) failed=1;
        if (!failed) {
            CGRect box=CGRectMake(0,0,CGImageGetWidth(image)*72.0/dx,CGImageGetHeight(image)*72.0/dy);
            CFDataRef boxData=CFDataCreate(NULL,(const UInt8 *)&box,sizeof(box));
            const void *keys[]={kCGPDFContextMediaBox}, *values[]={boxData};
            CFDictionaryRef page=CFDictionaryCreate(NULL,keys,values,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
            CGPDFContextBeginPage(context,page);
            CGContextDrawImage(context,box,image);
            CGPDFContextEndPage(context);
            CFRelease(page); CFRelease(boxData);
            if (out.failed) failed=1;
        }
        if (props) CFRelease(props);
        if (image) CGImageRelease(image);
        if (source) CFRelease(source);
        if (url) CFRelease(url);
    }
    if (context) { CGPDFContextClose(context); CGContextRelease(context); }
    if (consumer) CGDataConsumerRelease(consumer);
    if (fsync(out.fd)) failed=1;
    if (close(out.fd)) failed=1;
    failed|=out.failed;
    if (!failed && link(temporary,destination)) { perror("Publish PDF"); failed=1; }
    unlink(temporary);
    if (!failed) printf("PDF_SAVED %u %s\n",count,destination);
    else fprintf(stderr,"PDF could not be completed; TIFF pages retained.\n");
    return failed;
}
