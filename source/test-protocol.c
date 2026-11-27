/* Simulated transport tests; these do not communicate with hardware. */
#define main probe_program_main
#include "probe.c"
#undef main
#include <assert.h>

static int scenario, writes, reads;
static IOReturn fake_write(void *self, UInt8 pipe, void *buf, UInt32 n, UInt32 idle, UInt32 total) {
    (void)self;
    assert(pipe==4 && n==20 && idle>0 && total>0);
    assert(memcmp(buf,"CMD\0",4)==0);
    writes++;
    return scenario==6?kIOReturnNotResponding:kIOReturnSuccess;
}
static IOReturn fake_read(void *self, UInt8 pipe, void *buf, UInt32 *n, UInt32 idle, UInt32 total) {
    (void)self;
    assert(pipe==3 && idle>0 && total>0);
    reads++;
    if (scenario==5) return kIOReturnTimeout;
    unsigned char *b=buf;
    memset(b,0,*n);
    if (reads==1 && scenario!=1) {
        assert(*n>=237);
        b[1]=15; memcpy(b+2,"23.05.01",8); memcpy(b+12,"TESTONLY",8);
        *n=scenario==2?100:237;
    } else {
        assert(*n>=12);
        memcpy(b,"STS\0",4); b[7]=2; b[8]=(scenario==1 || scenario==4)?6:0;
        *n=scenario==3?11:12;
    }
    return kIOReturnSuccess;
}
int main(void) {
    IOUSBInterfaceInterface300 api={0};
    api.WritePipeTO=fake_write; api.ReadPipeTO=fake_read;
    IOUSBInterfaceInterface300 *p=&api;
    for (scenario=0;scenario<=6;scenario++) {
        writes=reads=0;
        int r=inquiry(&p,3,4,237);
        assert((r==0)==(scenario==0));
        assert(writes==1);
        assert(reads==(scenario==6?0:(scenario==1 || scenario==5?1:2)));
    }
    writes=reads=0;
    assert(inquiry(&p,0,4,237)!=0 && writes==0 && reads==0);
    puts("PASS: success, early rejection, short payload, malformed completion, device error, read/write failures, missing endpoint.");
    return 0;
}
