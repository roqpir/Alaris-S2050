/* Transport tests use synthetic replies, plus the user's observed lock reply
 * below with LF restored where v0.3 printed '?'. No raw USB capture exists. */
#define main probe_program_main
#include "probe.c"
#undef main
#include <assert.h>
#include "test-inquiry-v14.h"

enum { OK, BUSY, NO_ID, WRONG_REPLY, BAD_XML, LOCK_TIMEOUT, FRAGMENTED,
       INQUIRY_TIMEOUT, RELEASE_TIMEOUT, WRONG_MODE, LOCK_WRITE_ERROR, ECHO_ID,
       VERSION_REJECT, VERSION_TIMEOUT, VERSION_BAD_STATUS, VERSION_SHORT,
       VERSION_UNSUPPORTED, OLDER_VERSION, INQUIRY_REJECT, CONFIG_OK,
       CONFIG_SHORT, CONFIG_TIMEOUT, CONFIG_WRONG_TAG, CONFIG_REJECT,
       PAPER_BAD_VALUE, PAPER_REJECT };
static int scenario, resource_writes, bulk_writes, bulk_reads, version_writes, opcode;
static int config_writes,paper_writes;
static char rx[4097];
static size_t rx_pos, rx_size;
static const char observed_lock[]=
    "<task bytes='00000000000000000285' id='00000000000000000001' reply='00000000000000000000'>\n"
    "   <reportlock>\n"
    "       <lockid>174647827</lockid>\n"
    "   </reportlock>\n"
    "   <reportstatus>\n"
    "       <status>success</status>\n"
    "       <vid>0x29CC</vid>\n"
    "       <pid>0x1019</pid>\n"
    "   </reportstatus>\n"
    "</task>\n";

static void fixture(const char *body, unsigned reply) {
    rx_size=task_xml(rx,sizeof(rx),99,body);
    char digits[21]; snprintf(digits,sizeof(digits),"%020u",reply);
    memcpy(rx+68,digits,20); rx_pos=0;
}
static IOReturn write_mock(void *self, UInt8 pipe, void *buf, UInt32 n, UInt32 idle, UInt32 total) {
    (void)self; assert(idle && total);
    if (pipe==4) {
        const unsigned char *b=buf;
        assert(n==20 && !memcmp(buf,"CMD\0",4));
        opcode=b[13]; bulk_reads=0;
        if (opcode==1) {
            static const unsigned char expected[20]={0x43,0x4d,0x44,0,0,0,0,1,0,0,0,2,0x80,1,0,15,0,0,0,0};
            assert(!memcmp(buf,expected,20)); version_writes++;
        } else if (opcode==0x20) {
            assert(scenario>=CONFIG_OK && bulk_writes==1 && b[7]==0x0e && b[10]==2 && b[11]==0xf5 && b[12]==0x80);
            config_writes++;
        } else if (opcode==0x17) {
            assert(config_writes==1 && b[7]==0x0d && b[11]==1 && b[12]==0x80);
            paper_writes++;
        } else {
            assert(opcode==0x10 && version_writes==1 && b[7]==2);
            assert(b[11]==(scenario>=CONFIG_OK?236:(scenario==OLDER_VERSION?223:237)));
            bulk_writes++;
        }
        return 0;
    }
    assert(pipe==2 && n<1024 && strlen(buf)==n);
    unsigned length=0;
    assert(frame_size(buf,&length)==1 && length==n);
    resource_writes++;
    if (resource_writes==1) {
        assert(strstr(buf,"<resourcelock>") && !strstr(buf,"force"));
        if (scenario==LOCK_WRITE_ERROR) return kIOReturnNotResponding;
        const char *body="<status>success</status><lockid>42</lockid>";
        if (scenario==BUSY) body="<status>busy</status><lockid>77</lockid>";
        if (scenario==NO_ID) body="<status>success</status>";
        if (scenario==BAD_XML) body="<status>success</status><lockid>42";
        if (scenario==WRONG_MODE) body="<status>success</status><lockid>42</lockid><lockmode>other</lockmode>";
        fixture(body,scenario==WRONG_REPLY?8:1);
        if (scenario==ECHO_ID) {
            memcpy(rx,observed_lock,sizeof(observed_lock));
            rx_size=sizeof(observed_lock)-1; rx_pos=0;
        }
    } else {
        assert(resource_writes==2 && strstr(buf,"<resourceunlock>"));
        assert(strstr(buf,scenario==ECHO_ID?"<lockid>174647827</lockid>":"<lockid>42</lockid>"));
        fixture("<status>success</status>",2);
        if (scenario==ECHO_ID) {
            rx_size=task_xml(rx,sizeof(rx),2,"<reportstatus><status>success</status></reportstatus>");
        }
    }
    return 0;
}
static IOReturn read_mock(void *self, UInt8 pipe, void *buf, UInt32 *n, UInt32 idle, UInt32 total) {
    (void)self; assert(idle && total);
    if (pipe==3) {
        bulk_reads++;
        if (opcode==1) {
            assert(*n==12);
            if (scenario==VERSION_TIMEOUT) return kIOReturnTimeout;
            memset(buf,0,*n);
            if (scenario==VERSION_REJECT || bulk_reads==2) {
                memcpy(buf,"STS\0\0\0\0\1",8);
                if (scenario==VERSION_REJECT) ((unsigned char *)buf)[8]=6;
                if (scenario==VERSION_BAD_STATUS) ((unsigned char *)buf)[7]=2;
                *n=12;
            } else {
                ((unsigned char *)buf)[1]=scenario>=CONFIG_OK?14:(scenario==OLDER_VERSION?12:(scenario==VERSION_UNSUPPORTED?16:15));
                *n=scenario==VERSION_SHORT?1:2;
            }
            return 0;
        }
        if (opcode==0x20 || opcode==0x17) {
            unsigned char *b=buf;
            if (scenario==CONFIG_TIMEOUT && opcode==0x20) return kIOReturnTimeout;
            memset(buf,0,*n);
            if (bulk_reads==2 || (scenario==CONFIG_REJECT && opcode==0x20) || (scenario==PAPER_REJECT && opcode==0x17)) {
                memcpy(buf,"STS\0",4); b[7]=opcode==0x20?0x0e:0x0d;
                if (scenario==CONFIG_WRONG_TAG && opcode==0x20) b[7]=2;
                if ((scenario==CONFIG_REJECT && opcode==0x20) || (scenario==PAPER_REJECT && opcode==0x17)) b[8]=6;
                *n=12;
            } else if (opcode==0x20) {
                assert(*n==757); b[0]=1; b[4]=30; b[104]=0;
                *n=scenario==CONFIG_SHORT?500:757;
            } else {
                assert(*n==12); b[0]=scenario==PAPER_BAD_VALUE?2:1; *n=1;
            }
            return 0;
        }
        if (scenario==INQUIRY_TIMEOUT) return kIOReturnTimeout;
        memset(buf,0,*n);
        if (scenario==INQUIRY_REJECT) {
            memcpy(buf,"STS\0\0\0\0\2\6\0\0\0",12); *n=12;
        }
        else if (bulk_reads==1) {
            assert(*n==(scenario>=CONFIG_OK?236:(scenario==OLDER_VERSION?223:237)));
            if (scenario>=CONFIG_OK) memcpy(buf,observed_inquiry_v14,sizeof(observed_inquiry_v14));
            else ((char *)buf)[1]=scenario==OLDER_VERSION?12:15;
        }
        else { assert(*n==12); memcpy(buf,"STS\0\0\0\0\2",8); *n=12; }
        return 0;
    }
    assert(pipe==1);
    if (scenario==LOCK_TIMEOUT || (scenario==RELEASE_TIMEOUT && resource_writes==2)) return kIOReturnTimeout;
    size_t count=rx_size-rx_pos;
    if (scenario==FRAGMENTED && count>60) count=rx_pos==0?3:60;
    assert(count<=*n); *n=(UInt32)count;
    memcpy(buf,rx+rx_pos,count); rx_pos+=count; return 0;
}
int main(void) {
    IOUSBInterfaceInterface300 api={0};
    api.WritePipeTO=write_mock; api.ReadPipeTO=read_mock;
    IOUSBInterfaceInterface300 *p=&api;
    for (scenario=OK;scenario<=PAPER_REJECT;scenario++) {
        resource_writes=bulk_writes=bulk_reads=version_writes=config_writes=paper_writes=0;
        int r=session_inquiry(&p,3,4,1,2,scenario>=CONFIG_OK);
        assert((r==0)==(scenario==OK || scenario==FRAGMENTED || scenario==ECHO_ID || scenario==OLDER_VERSION || scenario==CONFIG_OK));
        int acquired=scenario==OK || scenario==FRAGMENTED || scenario==INQUIRY_TIMEOUT || scenario==RELEASE_TIMEOUT || scenario>=ECHO_ID;
        assert(resource_writes==(acquired?2:1));
        assert(version_writes==acquired);
        int negotiation_failed=scenario>=VERSION_REJECT && scenario<=VERSION_UNSUPPORTED;
        assert(bulk_writes==(acquired && !negotiation_failed));
        assert(config_writes==(scenario>=CONFIG_OK));
        assert(paper_writes==(scenario==CONFIG_OK || scenario>=PAPER_BAD_VALUE));
    }
    resource_writes=0;
    assert(session_inquiry(&p,3,4,0,2,0) && resource_writes==0);
    unsigned id=0;
    unsigned length=0;
    assert(strlen(observed_lock)==285 && frame_size(observed_lock,&length)==1 && length==285);
    assert(parse_reply(observed_lock,1,1,&id) && id==174647827);
    assert(!parse_reply(observed_lock,2,1,&id)); /* Old reply cannot match unlock. */
    assert(!parse_reply(observed_lock,0,1,&id));
    fixture("<status>success</status><lockid>42</lockid>",8);
    assert(!parse_reply(rx,99,1,&id)); /* Matching id must not mask wrong reply. */
    assert(!parse_reply("<task id='1'><status>success</status><lockid>42</lockid></task>",1,1,&id));
    assert(!parse_reply("<task reply='1'><status>success</status><lockid>42</lockid></task>",1,1,&id));
    assert(!parse_reply("<task id='1' reply='invalid'><status>success</status><lockid>42</lockid></task>",1,1,&id));
    assert(!parse_reply("<task id='4294967296' reply='0'><status>success</status><lockid>42</lockid></task>",1,1,&id));
    fixture("<status>success</status><lockid>4294967296</lockid>",1);
    assert(!parse_reply(rx,1,1,&id));
    fixture("<status>success</status><status>busy</status><lockid>42</lockid>",1);
    assert(!parse_reply(rx,1,1,&id));
    fixture("<status>success</status><lockid>0</lockid>",1);
    assert(!parse_reply(rx,1,1,&id));
    fixture("<status>success</status><lockid>42</lockid><lockid>43</lockid>",1);
    assert(!parse_reply(rx,1,1,&id));
    puts("PASS: observed lock reply, VERSION bytes/order, negotiation failures prevent INQUIRY, version-dependent length, STS 06 rejection, release after failure, resource parser and transport errors.");
    return 0;
}
