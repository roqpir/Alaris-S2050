/* Orchestration tests: transport/page job mocked; real page jobs covered by test-scan. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int IOUSBInterfaceInterface300;
typedef unsigned char UInt8;
typedef struct { IOUSBInterfaceInterface300 **intf; UInt8 in,out; int synchronized; } ScanTransport;
static const char *scan_directory;
static int scan_cancelled,mode,remaining,jobs,sensors,exports;
enum { TEN,ONE,EMPTY,PAGE_FAIL,CLEANUP_FAIL,CANCEL,INVALID_SENSOR,SENSOR_FAIL,LIMIT,EXISTS,VERSION };
static int scan_exchange(ScanTransport *s,unsigned tag,unsigned opcode,unsigned char *data,unsigned length,int receive,const unsigned char *params,const char *name) {
    (void)s;(void)name; assert(tag==0x0d && opcode==0x17 && length==1 && receive && !params);sensors++;
    if (mode==SENSOR_FAIL) return -1;
    data[0]=mode==INVALID_SENSOR?2:(remaining?1:0);return 0;
}
static int scan_one(IOUSBInterfaceInterface300 **intf,UInt8 in,UInt8 out,unsigned version) {
    (void)intf;assert(in==3 && out==4 && version==14);jobs++;
    if (mode==PAGE_FAIL && jobs==3) return 1;
    char path[PATH_MAX];snprintf(path,sizeof(path),"%s/scan.tiff",scan_directory);
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0);
    unsigned char page=(unsigned char)jobs;assert(write(fd,&page,1)==1 && !close(fd));remaining--;
    if (mode==CANCEL && jobs==2) scan_cancelled=1;
    return mode==CLEANUP_FAIL && jobs==3;
}
static int pdf_from_tiffs(const char *destination,const char *const *paths,unsigned count) {
    assert(count>0);exports++;
    for (unsigned i=0;i<count;i++) {
        int fd=open(paths[i],O_RDONLY);unsigned char page=0;
        assert(fd>=0 && read(fd,&page,1)==1 && page==i+1 && !close(fd));
    }
    assert(strstr(destination,(mode==TEN || mode==ONE)?"/scan.pdf":"/scan-partial.pdf"));
    return 0;
}
#include "stack.h"
int main(void) {
    for (mode=TEN;mode<=VERSION;mode++) {
        char root[]="/tmp/s2050-stack-test-XXXXXX";assert(mkdtemp(root));scan_directory=root;
        scan_cancelled=jobs=sensors=exports=0;remaining=mode==ONE?1:(mode==EMPTY?0:(mode==LIMIT?101:10));
        if (mode==EXISTS) {char p[PATH_MAX];snprintf(p,sizeof(p),"%s/page-001",root);assert(!mkdir(p,0700));}
        int failed=scan_stack(NULL,3,4,mode==VERSION?15:14);
        assert(scan_directory==root);
        unsigned expected=mode==TEN?10:(mode==ONE?1:(mode==PAGE_FAIL || mode==CANCEL?2:(mode==CLEANUP_FAIL?3:(mode==LIMIT?100:0))));
        assert(scan_pages_saved==expected);
        assert((failed==0)==(mode==TEN || mode==ONE));
        if (mode==TEN || mode==ONE) assert(sensors==jobs+1 && remaining==0);
        if (mode==EMPTY) assert(sensors==1 && jobs==0);
        if (mode==CANCEL) assert(sensors==2 && jobs==2);
        if (mode==LIMIT) assert(jobs==100 && remaining==1 && sensors==101);
        if (mode==EXISTS || mode==VERSION || mode==INVALID_SENSOR || mode==SENSOR_FAIL) assert(!jobs);
        assert(!scan_export_pdf(1,failed));assert(exports==(expected?1:0));
        for (unsigned i=1;i<=101;i++) {
            char file[PATH_MAX],dir[PATH_MAX];
            snprintf(file,sizeof(file),"%s/page-%03u/scan.tiff",root,i);unlink(file);
            snprintf(dir,sizeof(dir),"%s/page-%03u",root,i);rmdir(dir);
        }
        assert(!rmdir(root));printf("STACK TEST %d PASSED\n",mode);
    }
    puts("All stack orchestration tests passed.");return 0;
}
