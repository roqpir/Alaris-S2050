/* Synthetic protocol fixtures; these tests do not claim hardware support. */
#include <time.h>
static int scan_test_clock_gettime(clockid_t clock, struct timespec *time);
#define clock_gettime scan_test_clock_gettime
#define main probe_program_main
#include "probe.c"
#undef main
#undef clock_gettime
#include <assert.h>
#include "test-inquiry-v14.h"
#include "test-image-header-v14.h"

enum { HAPPY, NO_PAPER, WINDOW_REJECT, BAD_TAG, DATA_TIMEOUT, BAD_LAYOUT,
       CANCEL, NOT_READY, RESTORE_FAIL, END_FAIL, CONFIG_FAIL, PARTIAL_READ,
       CAMERA_REJECT, CAMERA_REJECT_READBACK_DIFF, READBACK_SAVE_FAIL,
       HEADER_FATAL_10, HEADER_WAIT_CANCEL, HEADER_WAIT_DEADLINE, SMALL_TRANSFER_LIMIT,
       LAST_BLOCK_REJECT, LAST_BLOCK_BAD_COMPLETION, LAST_BLOCK_TIMEOUT };
static int scenario,command_count,read_phase,payload_pending,restored,end_seen,stop_seen;
static unsigned opcode,tag,length,data_pos,image_pos;
static unsigned char original[757],header_fixture[213];
static unsigned image_bytes=1240u*1754u;
static unsigned resource_writes,handshake,handshake_reads;
static unsigned header_queries,data_commands;
static int camera_rejected(void) {
    return scenario>=CAMERA_REJECT && scenario<=READBACK_SAVE_FAIL;
}
static int scan_test_clock_gettime(clockid_t clock, struct timespec *time) {
    int result=clock_gettime(clock,time);
    /* Simulate expiration without sleeping 45 seconds in the regression test. */
    if (!result && scenario==HEADER_WAIT_DEADLINE && header_queries) time->tv_sec+=60;
    return result;
}

static IOReturn scan_write_mock(void *self, UInt8 pipe, void *buffer, UInt32 size, UInt32 idle, UInt32 total) {
    (void)self; assert(idle && total);
    unsigned char *b=buffer;
    if (pipe==2) {
        assert(scenario>=CAMERA_REJECT);
        resource_writes++;
        if (resource_writes==1) assert(strstr(buffer,"<resourcelock>"));
        else {
            assert(resource_writes==2 && strstr(buffer,"<resourceunlock>"));
            assert(strstr(buffer,"<lockid>42</lockid>") && restored==(scenario==LAST_BLOCK_TIMEOUT?0:1));
        }
        return 0;
    }
    assert(pipe==4);
    if (!payload_pending && size==20 && (b[13]==1 || b[13]==0x10)) {
        assert(resource_writes==1 && command_count==0 && !handshake);
        if (b[13]==1) assert(b[7]==1 && b[11]==2 && b[15]==15);
        else assert(b[7]==2 && b[11]==236);
        handshake=b[13]; handshake_reads=0; return 0;
    }
    if (payload_pending) {
        assert(size==length); payload_pending=0;
        if (opcode==0x20) {
            if (command_count>3) { assert(!memcmp(b,original,757)); restored++; }
            else { assert(b[0]==1 && b[7]==0 && b[8]==0);
                   assert(!memcmp(b+9,original+9,5));
                   assert(b[86]==0 && b[87]==1 && b[104]==0 && b[118]==0 && b[755]==0 && b[756]==1); }
        }
        if (opcode==0x21) {
            /* Vendor Default profile + manager layout: ID2/type0/detect0,
             * amount1/threshold0x14. See evidence/camera-defaults.md. */
            static const unsigned char expected[36]={2,0,0,1,0x14};
            assert(size==36 && !memcmp(b,expected,36));
        }
        if (opcode==0x22) {
            assert(size==87 && b[0]==0 && b[1]==1 && b[2]==2);
            assert(b[3]==(scan_mode==SCAN_COLOR?2:1));
            assert(be16(b+4)==scan_dpi && be16(b+6)==scan_dpi);
            assert(b[26]==100 && b[31]==100 && b[35]==100 && b[39]==255 && b[86]==255);
        }
        if (opcode==0x23) { static const unsigned char order[]={0,255,255,255}; assert(!memcmp(b,order,4)); }
        return 0;
    }
    assert(size==20 && !memcmp(b,"CMD\0",4));
    assert(command_count==0 || read_phase==2);
    command_count++; tag=b[7]; opcode=b[13]; length=be32(b+8);
    assert(!b[4] && !b[5] && !b[6] && b[19]==0);
    read_phase=0; data_pos=0;
    payload_pending=b[12]==0 && length;
    if (opcode==0x20) { assert(length==757 && tag==(b[12]?0x0e:0x0f)); }
    else if (opcode==0x17) { assert(command_count==2 && tag==0x0d && length==1); }
    else if (opcode==0x21) { assert(command_count==4 && tag==0x10); }
    else if (opcode==0x22) { assert(command_count==5 && tag==0x11); }
    else if (opcode==0x23) { assert(command_count==6 && tag==0x12); }
    else if (opcode==0x30) {
        static const unsigned char expected[20]={'C','M','D',0,0,0,0,0x13,0,0,0,8,0x80,0x30,1,0,0,0,0,0};
        assert(!memcmp(b,expected,20));
    } else if (opcode==0x31) {
        assert(tag==0x14 && length==213); header_queries++;
        if (scenario==HEADER_FATAL_10) assert(header_queries==1);
    }
    else if (opcode==0x32) {
        assert(tag==255 && image_pos+length<=image_bytes);
        /* Vendor ReadImage: use the negotiated block, then the exact remainder.
         * With the observed2MiB limit the commands are2097152 and77808 bytes. */
        unsigned limit=scenario==SMALL_TRANSFER_LIMIT?65536:2097152;
        unsigned remaining=image_bytes-image_pos;
        assert(length==(remaining<limit?remaining:limit));
        if (scenario==NOT_READY) assert(header_queries==4 && !stop_seen);
        data_commands++;
    }
    else if (opcode==0x33) { assert(tag==0x15 && !length); stop_seen++; }
    else if (opcode==0x34) { assert(tag==0x16 && length==12); end_seen++; }
    else assert(0);
    if (!b[12] || !length) read_phase=1;
    if (scenario==CANCEL && opcode==0x21) scan_signal(SIGINT);
    return 0;
}
static IOReturn scan_read_mock(void *self, UInt8 pipe, void *buffer, UInt32 *size, UInt32 idle, UInt32 total) {
    (void)self; assert(idle && total && !payload_pending);
    unsigned char *b=buffer;
    if (pipe==1) {
        char xml[256];
        size_t count=task_xml(xml,sizeof(xml),resource_writes,resource_writes==1?
            "<reportlock><lockid>42</lockid></reportlock><reportstatus><status>success</status></reportstatus>":
            "<reportstatus><status>success</status></reportstatus>");
        assert(count && count<=*size); memcpy(b,xml,count); *size=(UInt32)count; return 0;
    }
    assert(pipe==3);
    if (handshake) {
        if (!handshake_reads++) {
            if (handshake==1) { assert(*size>=2); b[0]=0; b[1]=14; *size=2; }
            else { assert(*size>=236); memcpy(b,observed_inquiry_v14,236); *size=236; }
        } else {
            assert(*size>=12); memset(b,0,12); memcpy(b,"STS\0",4); b[7]=handshake==1?1:2;
            *size=12; handshake=0;
        }
        return 0;
    }
    if (scenario==DATA_TIMEOUT && opcode==0x32) return kIOReturnTimeout;
    if (scenario==LAST_BLOCK_TIMEOUT && opcode==0x32 && data_commands==2) return kIOReturnTimeout;
    int early=opcode==0x31 && ((scenario==NOT_READY && header_queries<=3) ||
        scenario==HEADER_FATAL_10 || scenario==HEADER_WAIT_CANCEL || scenario==HEADER_WAIT_DEADLINE);
    int reject_data=scenario==LAST_BLOCK_REJECT && opcode==0x32 && data_commands==2;
    if (read_phase==1 || early || reject_data || (scenario==END_FAIL && opcode==0x34)) {
        assert(*size>=12); memset(b,0,12); memcpy(b,"STS\0",4); b[7]=(unsigned char)tag; *size=12;
        if (scenario==WINDOW_REJECT && opcode==0x22) b[8]=6;
        if (camera_rejected() && opcode==0x21) b[8]=6;
        if (scenario==BAD_TAG && opcode==0x22) b[7]=0x99;
        if (scenario==RESTORE_FAIL && restored && opcode==0x20 && tag==0x0f) b[8]=6;
        if (scenario==CONFIG_FAIL && command_count==3) b[8]=6;
        if (scenario==END_FAIL && opcode==0x34) b[8]=6;
        if (reject_data || (scenario==LAST_BLOCK_BAD_COMPLETION && opcode==0x32 && data_commands==2)) b[8]=6;
        /* Raw USB 01 maps to vendor enum16; raw USB10 maps to enum23. */
        if (early) b[8]=scenario==HEADER_FATAL_10?0x10:0x01;
        if (early && scenario==HEADER_WAIT_CANCEL) scan_signal(SIGINT);
        read_phase=2; return 0;
    }
    assert(read_phase==0);
    unsigned n=length-data_pos; if (n>*size) n=*size;
    if (scenario==PARTIAL_READ && n>512) n=512;
    if (opcode==0x20) {
        memcpy(b,original+data_pos,n);
        if (scenario==CAMERA_REJECT_READBACK_DIFF && restored) {
            /* Exact v7 hardware mismatch: only batchendfunction remained 1. */
            if (data_pos<=13 && data_pos+n>13) b[13-data_pos]=1;
        }
    }
    else if (opcode==0x17) b[0]=scenario==NO_PAPER?0:1;
    else if (opcode==0x30) {
        /* Exact successful SCAN BEGIN payload observed on v8 hardware. */
        static const unsigned char reply[8]={0,0x20,0,0,0,8,0x5a,0x58};
        memcpy(b,reply,n);
        if (scenario==SMALL_TRANSFER_LIMIT) put32(b,65536);
    }
    else if (opcode==0x31) memcpy(b,header_fixture+data_pos,n);
    else if (opcode==0x32) {
        /* No artificial1MiB cap in the initial OS read for a2MiB command. */
        if (!data_pos) assert(*size==length);
        for (unsigned i=0;i<n;i++) b[i]=(unsigned char)((image_pos+i)%251);
        image_pos+=n;
    }
    else if (opcode==0x34) memset(b,0,n);
    else assert(0);
    *size=n; data_pos+=n; if (data_pos==length) read_phase=1;
    return 0;
}
int main(void) {
    IOUSBInterfaceInterface300 api={0}; api.WritePipeTO=scan_write_mock; api.ReadPipeTO=scan_read_mock;
    IOUSBInterfaceInterface300 *p=&api;
    /* Real header; synthetic pixel content only. Includes chunknumber1/last1. */
    memcpy(header_fixture,observed_image_header_v14,sizeof(header_fixture));
    for (unsigned i=0;i<757;i++) original[i]=(unsigned char)(i%251);
    original[13]=0;
    /* Both initially enabled and disabled batching must retain dormant fields.
     * Includes the real v7 action0 and action1 left after its failed restore. */
    for (unsigned enabled=0;enabled<=1;enabled++) for (unsigned action=0;action<=1;action++) {
        unsigned char before[757],config[757],camera[36],window[87];
        memcpy(before,original,757); before[8]=(unsigned char)enabled; before[13]=(unsigned char)action;
        scan_settings(before,config,camera,window);
        assert(config[8]==0 && !memcmp(config+9,before+9,5));
        assert(be16(config+86)==1 && be16(config+755)==1);
        assert(camera[2]==0 && camera[3]>=1 && camera[3]<=200 && camera[4]==20);
    }
    for (int trial=HAPPY;trial<=LAST_BLOCK_TIMEOUT+8;trial++) {
        const unsigned dpis[]={150,300,600,150,300,600,150,300,600};
        const unsigned modes[]={SCAN_GRAY,SCAN_GRAY,SCAN_GRAY,SCAN_COLOR,SCAN_COLOR,SCAN_COLOR,SCAN_BW,SCAN_BW,SCAN_BW};
        unsigned option=trial>LAST_BLOCK_TIMEOUT?(unsigned)(trial-LAST_BLOCK_TIMEOUT):0;
        scenario=option?HAPPY:trial; scan_dpi=dpis[option]; scan_mode=modes[option];
        image_bytes=(1240u*scan_dpi/150)*(1754u*scan_dpi/150)*(scan_mode==SCAN_COLOR?3:1);
        memcpy(header_fixture,observed_image_header_v14,sizeof(header_fixture));
        put16(header_fixture+9,scan_dpi);put16(header_fixture+11,scan_dpi);
        header_fixture[8]=scan_mode==SCAN_COLOR?2:1; put32(header_fixture+4,image_bytes);
        char directory[]="/tmp/s2050-scan-test-XXXXXX";
        assert(mkdtemp(directory)); scan_directory=directory;
        command_count=read_phase=payload_pending=restored=end_seen=stop_seen=0;
        resource_writes=handshake=handshake_reads=0;
        header_queries=data_commands=0;
        image_pos=0; scan_cancelled=0; header_fixture[179]=scenario==BAD_LAYOUT?2:0;
        char path[PATH_MAX];
        if (scenario==READBACK_SAVE_FAIL) {
            /* Existing output must not be overwritten, even during cleanup. */
            snprintf(path,sizeof(path),"%s/config-after.bin",directory);
            int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600); assert(fd>=0);
            assert(write(fd,"keep",4)==4); assert(!close(fd));
        }
        int failed=scenario>=CAMERA_REJECT?session_inquiry(&p,3,4,1,2,2):scan_one(&p,3,4,14);
        if (scenario>=CAMERA_REJECT) assert(resource_writes==2 && !handshake);
        assert((failed==0)==(scenario==HAPPY || scenario==NOT_READY || scenario==PARTIAL_READ || scenario==SMALL_TRANSFER_LIMIT));
        if (scenario==NO_PAPER) assert(command_count==2 && !restored && !stop_seen);
        else if (scenario==BAD_TAG) assert(command_count==5 && !restored && !stop_seen);
        else if (scenario==DATA_TIMEOUT) assert(command_count==9 && !restored && !stop_seen);
        else if (scenario==LAST_BLOCK_TIMEOUT) assert(!restored && !stop_seen && !end_seen && data_commands==2);
        else if (scenario==END_FAIL) assert(!restored && stop_seen==1 && end_seen==1);
        else assert(restored==1);
        if (camera_rejected()) assert(command_count==6 && !stop_seen && !end_seen && !image_pos);
        if (scenario==NOT_READY) assert(header_queries==4 && data_commands==2 && image_pos==image_bytes);
        if (scenario>=HEADER_FATAL_10 && scenario<=HEADER_WAIT_DEADLINE)
            assert(header_queries==1 && !data_commands && !image_pos && stop_seen==1 && end_seen==1);
        if (scenario==SMALL_TRANSFER_LIMIT) assert(data_commands==(image_bytes+65535)/65536 && image_pos==image_bytes);
        if (scenario==LAST_BLOCK_REJECT || scenario==LAST_BLOCK_BAD_COMPLETION)
            assert(data_commands==2 && stop_seen==1 && end_seen==1);
        snprintf(path,sizeof(path),"%s/config-after.bin",directory);
        if (restored && scenario!=RESTORE_FAIL) {
            FILE *file=fopen(path,"rb"); assert(file);
            unsigned char actual[758]; size_t count=fread(actual,1,sizeof(actual),file); assert(!fclose(file));
            if (scenario==READBACK_SAVE_FAIL) assert(count==4 && !memcmp(actual,"keep",4));
            else {
                assert(count==757);
                unsigned diffs=0;
                for (unsigned i=0;i<757;i++) if (actual[i]!=original[i]) {
                    assert(scenario==CAMERA_REJECT_READBACK_DIFF && i==13 && actual[i]==1); diffs++;
                }
                assert(diffs==(scenario==CAMERA_REJECT_READBACK_DIFF?1u:0u));
            }
        } else assert(access(path,F_OK)!=0);
        snprintf(path,sizeof(path),"%s/scan.tiff",directory);
        struct stat st;
        int image_saved=stat(path,&st)==0;
        assert(image_saved==(scenario==HAPPY || scenario==NOT_READY || scenario==RESTORE_FAIL || scenario==END_FAIL || scenario==PARTIAL_READ || scenario==SMALL_TRANSFER_LIMIT));
        if (image_saved) {
            unsigned width=1240u*scan_dpi/150,height=1754u*scan_dpi/150;
            unsigned prefix=scan_mode==SCAN_COLOR?192:186;
            unsigned saved_bytes=scan_mode==SCAN_BW?((width+7)/8)*height:image_bytes;
            assert(st.st_size==saved_bytes+prefix);
            /* Verify assembly across transfer boundaries, not just file size. */
            FILE *image=fopen(path,"rb"); assert(image && !fseek(image,prefix,SEEK_SET));
            unsigned char block[4096]; unsigned position=0;
            while (position<saved_bytes) {
                unsigned count=saved_bytes-position; if (count>sizeof(block)) count=sizeof(block);
                assert(fread(block,1,count,image)==count);
                for (unsigned i=0;i<count;i++) {
                    unsigned at=position+i;
                    if (scan_mode==SCAN_BW) {
                        unsigned stride=(width+7)/8,row=at/stride,col=(at%stride)*8;
                        unsigned char expected=0;
                        for (unsigned bit=0;bit<8 && col+bit<width;bit++)
                            if ((row*width+col+bit)%251>=128) expected|=(unsigned char)(0x80u>>bit);
                        assert(block[i]==expected);
                    } else assert(block[i]==(unsigned char)(at%251));
                }
                position+=count;
            }
            assert(fgetc(image)==EOF && !fclose(image));
        }
        snprintf(path,sizeof(path),"%s/image.partial.raw",directory);
        if (scenario>=LAST_BLOCK_REJECT) {
            /* Only the successful first block may be retained, even if the
             * second payload arrived but its completion status rejected it. */
            assert(!stat(path,&st) && st.st_size==2097152);
            FILE *partial=fopen(path,"rb"); assert(partial);
            unsigned char block[4096];
            for (unsigned pos=0;pos<2097152;pos+=sizeof(block)) {
                assert(fread(block,1,sizeof(block),partial)==sizeof(block));
                for (unsigned i=0;i<sizeof(block);i++) assert(block[i]==(unsigned char)((pos+i)%251));
            }
            assert(fgetc(partial)==EOF && !fclose(partial));
        } else assert(access(path,F_OK)!=0);
        const char *names[]={"config-before.bin","config-test.bin","scan-begin.bin","image-header.bin","image.raw","scan.tiff","config-after.bin","image.partial.raw"};
        for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) { snprintf(path,sizeof(path),"%s/%s",directory,names[i]); unlink(path); }
        assert(!rmdir(directory));
        printf("SCAN TEST %d PASSED dpi=%u mode=%u\n",trial,scan_dpi,scan_mode);
    }
    command_count=0; assert(scan_one(&p,3,4,15)); assert(command_count==0);
    assert(!strcmp(config_field_name(104),"papersource"));
    assert(!strcmp(config_field_name(756),"sheetcount"));
    puts("All scan protocol tests passed (synthetic transport; no hardware accessed).");
    return 0;
}
