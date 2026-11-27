#define main probe_program_main
#include "probe.c"
#undef main
#include <assert.h>
#include <math.h>
#include "test-image-header-v14.h"

static void options_test(void) {
    char *valid[]={"--mode","color","--dpi","600"};
    assert(!scan_options(4,valid,0) && scan_mode==SCAN_COLOR && scan_dpi==600);
    assert(!scan_options(0,valid,0) && scan_mode==SCAN_GRAY && scan_dpi==150);
    char *finish[]={"--paper-cleanup","off","--edge-cleanup","on"};
    assert(!scan_options(4,finish,0) && !scan_paper_cleanup && scan_edge_cleanup);
    char *duplicate[]={"--paper-cleanup","on","--paper-cleanup","off"};
    assert(scan_options(4,duplicate,0));
    char *badfinish[]={"--edge-cleanup","yes"};assert(scan_options(2,badfinish,0));
    assert(!scan_options(0,valid,0) && scan_paper_cleanup && scan_edge_cleanup);
    char *invalid[][4]={{"--dpi","301"},{"--dpi","600x"},{"--dpi","0"},
        {"--mode","rgb"},{"--unknown","gray"},{"--dpi","150","--dpi","300"},
        {"--mode","gray","--mode","color"},{"--dpi"}};
    for (unsigned i=0;i<8;i++) assert(scan_options(i==7?1:(i==5 || i==6)?4:2,invalid[i],0));
}
int main(void) {
    options_test();
    char dir[]="/tmp/s2050-options-test-XXXXXX"; assert(mkdtemp(dir));scan_directory=dir;
    char tiff[PATH_MAX],pdf[PATH_MAX];snprintf(tiff,sizeof(tiff),"%s/scan.tiff",dir);
    snprintf(pdf,sizeof(pdf),"%s/scan.pdf",dir);
    unsigned dpis[]={150,300,600};
    for (unsigned d=0;d<3;d++) for (unsigned mode=SCAN_GRAY;mode<=SCAN_BW;mode++) {
        scan_dpi=dpis[d];scan_mode=mode;
        unsigned width=104*scan_dpi/1200,height=80*scan_dpi/1200,samples=mode==SCAN_COLOR?3:1;
        unsigned size=width*height*samples;unsigned char *pixels=malloc(size);assert(pixels);
        for (unsigned row=0;row<height;row++) for (unsigned col=0;col<width;col++) {
            unsigned at=(row*width+col)*samples;
            if (samples==3) {pixels[at]=col<width/2?255:0;pixels[at+1]=0;pixels[at+2]=col<width/2?0:255;}
            else pixels[at]=col<width/2?127:128;
        }
        unsigned char header[213];memcpy(header,observed_image_header_v14,213);
        header[8]=samples==3?2:1;put16(header+9,scan_dpi);put16(header+11,scan_dpi);
        put32(header+21,104);put32(header+25,80);put32(header+4,size);
        assert(!scan_tiff(header,pixels,size));
        CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(UInt8 *)tiff,strlen(tiff),false);
        CGImageSourceRef source=CGImageSourceCreateWithURL(url,NULL);assert(source);
        CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,NULL);assert(image);
        assert(CGImageGetWidth(image)==width && CGImageGetHeight(image)==height);
        /* ImageIO may expand1-bit TIFF to8-bit for display. Verify the stored
         * BitsPerSample tag directly, then test the decoded pixels below. */
        FILE *file=fopen(tiff,"rb");assert(file);unsigned char stored[46];
        assert(fread(stored,1,sizeof(stored),file)==sizeof(stored) && !fclose(file));
        assert(stored[34]==2 && stored[35]==1); /* tag258 */
        if (mode!=SCAN_COLOR) assert(stored[42]==(mode==SCAN_BW?1:8));
        unsigned char bitmap[8*4*4]={0};CGColorSpaceRef rgb=CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx=CGBitmapContextCreate(bitmap,8,4,8,8*4,rgb,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);assert(ctx);
        CGContextSetInterpolationQuality(ctx,kCGInterpolationNone);
        CGContextDrawImage(ctx,CGRectMake(0,0,8,4),image);
        unsigned char *left=bitmap+4*(8+1),*right=bitmap+4*(8+6);
        if (mode==SCAN_COLOR) {assert(left[0]>240 && left[1]<10 && left[2]<10);assert(right[0]<10 && right[1]<10 && right[2]>240);}
        else if (mode==SCAN_BW) {assert(left[0]<5 && right[0]>250);}
        else {assert(left[0]>100 && left[0]<150 && right[0]>100 && right[0]<150);}
        CGImageRelease(image);CFRelease(source);CFRelease(url);
        const char *pages[]={tiff};assert(!pdf_from_tiffs(pdf,pages,1));
        url=CFURLCreateFromFileSystemRepresentation(NULL,(UInt8 *)pdf,strlen(pdf),false);
        CGPDFDocumentRef doc=CGPDFDocumentCreateWithURL(url);assert(doc && CGPDFDocumentGetNumberOfPages(doc)==1);
        CGPDFPageRef page=CGPDFDocumentGetPage(doc,1);CGRect box=CGPDFPageGetBoxRect(page,kCGPDFMediaBox);
        assert(fabs(box.size.width-6.24)<0.01 && fabs(box.size.height-4.8)<0.01);
        memset(bitmap,0,sizeof(bitmap));CGContextScaleCTM(ctx,8/box.size.width,4/box.size.height);
        CGContextDrawPDFPage(ctx,page);
        if (mode==SCAN_COLOR) {assert(left[0]>240 && left[2]<10);assert(right[0]<10 && right[2]>240);}
        else if (mode==SCAN_BW) {assert(left[0]<5 && right[0]>250);}
        CGContextRelease(ctx);CGColorSpaceRelease(rgb);CGPDFDocumentRelease(doc);CFRelease(url);
        assert(!unlink(tiff) && !unlink(pdf));
        /* Fail closed for mismatched DPI, unsupported formats, compression,
         * inversion, truncated data and ambiguous row dimensions. */
        header[8]=7;assert(scan_tiff(header,pixels,size));header[8]=samples==3?2:1;
        put16(header+9,1200);assert(scan_tiff(header,pixels,size));put16(header+9,scan_dpi);
        header[179]=2;assert(scan_tiff(header,pixels,size));header[179]=0;
        header[184]=1;assert(scan_tiff(header,pixels,size));header[184]=0;
        assert(scan_tiff(header,pixels,size-1));
        put32(header+21,105);assert(scan_tiff(header,pixels,size));
        assert(access(tiff,F_OK));free(pixels);
        printf("OUTPUT TEST dpi=%u mode=%u PASSED\n",scan_dpi,mode);
    }
    assert(!rmdir(dir));puts("Options tests passed: parsing,9 TIFF/PDF modes,color order,bitonal polarity,physical size,invalid layouts.");
    return 0;
}
