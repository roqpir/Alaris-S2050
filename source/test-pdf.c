#define main probe_program_main
#include "probe.c"
#undef main
#include <assert.h>
#include <math.h>
#include "test-image-header-v14.h"
int main(void) {
    char root[]="/tmp/s2050-pdf-test-XXXXXX";assert(mkdtemp(root));
    char paths[10][PATH_MAX],pdf[PATH_MAX];const char *pages[10];
    unsigned size=1240u*1754u;unsigned char *pixels=malloc(size);assert(pixels);
    for (unsigned i=0;i<10;i++) {
        char dir[PATH_MAX];snprintf(dir,sizeof(dir),"%s/page-%03u",root,i+1);assert(!mkdir(dir,0700));scan_directory=dir;
        memset(pixels,20+i*20,size);assert(!scan_tiff(observed_image_header_v14,pixels,size));
        snprintf(paths[i],PATH_MAX,"%s/page-%03u/scan.tiff",root,i+1);pages[i]=paths[i];
    }
    free(pixels);snprintf(pdf,sizeof(pdf),"%s/scan.pdf",root);
    assert(!pdf_from_tiffs(pdf,pages,10));
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(UInt8 *)pdf,strlen(pdf),false);
    CGPDFDocumentRef doc=CGPDFDocumentCreateWithURL(url);assert(doc && CGPDFDocumentGetNumberOfPages(doc)==10);
    unsigned previous=0;
    for (unsigned i=1;i<=10;i++) {
        CGPDFPageRef page=CGPDFDocumentGetPage(doc,i);CGRect box=CGPDFPageGetBoxRect(page,kCGPDFMediaBox);
        assert(fabs(box.size.width-595.2)<0.01 && fabs(box.size.height-841.92)<0.01);
        unsigned char bitmap[100]={0};CGColorSpaceRef gray=CGColorSpaceCreateDeviceGray();
        CGContextRef ctx=CGBitmapContextCreate(bitmap,10,10,8,10,gray,kCGImageAlphaNone);assert(ctx);
        CGContextScaleCTM(ctx,10.0/box.size.width,10.0/box.size.height);CGContextDrawPDFPage(ctx,page);
        /* Ordered gray pages: catches reversed/skipped/duplicated PDF pages. */
        assert(bitmap[55]>previous+5);previous=bitmap[55];CGContextRelease(ctx);CGColorSpaceRelease(gray);
    }
    CGPDFDocumentRelease(doc);CFRelease(url);
    /* A destination that already exists is preserved. */
    assert(pdf_from_tiffs(pdf,pages,1));
    snprintf(pdf,sizeof(pdf),"%s/invalid.pdf",root);const char *missing[]={"/no/such/s2050-input.tiff"};
    assert(pdf_from_tiffs(pdf,missing,1) && access(pdf,F_OK));
    assert(pdf_from_tiffs(pdf,pages,0) && access(pdf,F_OK));
    char single[PATH_MAX];snprintf(single,sizeof(single),"%s/page-001",root);scan_directory=single;
    assert(!scan_export_pdf(0,0));
    snprintf(pdf,sizeof(pdf),"%s/page-001/scan.pdf",root);
    url=CFURLCreateFromFileSystemRepresentation(NULL,(UInt8 *)pdf,strlen(pdf),false);doc=CGPDFDocumentCreateWithURL(url);
    assert(doc && CGPDFDocumentGetNumberOfPages(doc)==1);CGPDFDocumentRelease(doc);CFRelease(url);unlink(pdf);
    scan_directory=root;scan_pages_saved=10;assert(!scan_export_pdf(1,1));
    snprintf(pdf,sizeof(pdf),"%s/scan-partial.pdf",root);
    url=CFURLCreateFromFileSystemRepresentation(NULL,(UInt8 *)pdf,strlen(pdf),false);doc=CGPDFDocumentCreateWithURL(url);
    assert(doc && CGPDFDocumentGetNumberOfPages(doc)==10);CGPDFDocumentRelease(doc);CFRelease(url);unlink(pdf);
    snprintf(pdf,sizeof(pdf),"%s/scan.pdf",root);unlink(pdf);
    for (unsigned i=0;i<10;i++) {unlink(paths[i]);char dir[PATH_MAX];snprintf(dir,sizeof(dir),"%s/page-%03u",root,i+1);assert(!rmdir(dir));}
    assert(!rmdir(root));puts("PDF tests passed:10 ordered pages,physical dimensions,partial export,no overwrite,invalid input.");return 0;
}
