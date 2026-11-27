/* Experimental single-page path, derived from the sage command resource and
 * CDevMgrProcessDI::ReadImage in Kodak's Linux driver 08.02.00.00.
 * Only interface 14 is enabled for this hardware test. */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "config-fields.h"

#define SCAN_LIMIT (16u*1024u*1024u)
/* Transfer negotiation stays at the proven limit. A full A4 RGB600 image
 * needs about100MiB, received in the scanner's negotiated blocks. */
#define SCAN_IMAGE_LIMIT (128u*1024u*1024u)
enum { SCAN_GRAY=1, SCAN_COLOR=2, SCAN_BW=3 };
static unsigned scan_dpi=150,scan_mode=SCAN_GRAY;
static int scan_paper_cleanup=1,scan_edge_cleanup=1;
#include "cleanup.h"
static int scan_dpi_valid(unsigned dpi) { return dpi==150 || dpi==300 || dpi==600; }
static int scan_options(int argc,char **argv,int start) {
    int dpi_seen=0,mode_seen=0,paper_seen=0,edge_seen=0;
    scan_dpi=150; scan_mode=SCAN_GRAY;
    scan_paper_cleanup=1;scan_edge_cleanup=1;
    for (int i=start;i<argc;i+=2) {
        if (i+1>=argc) return 1;
        if (!strcmp(argv[i],"--dpi") && !dpi_seen++) {
            if (!strcmp(argv[i+1],"150")) scan_dpi=150;
            else if (!strcmp(argv[i+1],"300")) scan_dpi=300;
            else if (!strcmp(argv[i+1],"600")) scan_dpi=600;
            else return 1;
        } else if (!strcmp(argv[i],"--mode") && !mode_seen++) {
            if (!strcmp(argv[i+1],"gray")) scan_mode=SCAN_GRAY;
            else if (!strcmp(argv[i+1],"color")) scan_mode=SCAN_COLOR;
            else if (!strcmp(argv[i+1],"bw")) scan_mode=SCAN_BW;
            else return 1;
        } else if ((!strcmp(argv[i],"--paper-cleanup") && !paper_seen++) ||
                   (!strcmp(argv[i],"--edge-cleanup") && !edge_seen++)) {
            int value;
            if (!strcmp(argv[i+1],"on")) value=1;
            else if (!strcmp(argv[i+1],"off")) value=0;
            else return 1;
            if (!strcmp(argv[i],"--paper-cleanup")) scan_paper_cleanup=value;
            else scan_edge_cleanup=value;
        } else return 1;
    }
    return 0;
}
/* GetWindowOrder (0x97e53) assigns 2 for front, 3 for rear.
 * ScanSetup (0xda60d..0xda626) copies that value into dmcameraid.
 * This ID is distinct from cameratype, where front is encoded as 0. */
enum { SCAN_FRONT_CAMERA_ID=2 };
static const char *scan_directory;
static volatile sig_atomic_t scan_cancelled;
static void scan_signal(int sig) { (void)sig; scan_cancelled=1; }
static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static void put16(unsigned char *p, unsigned n) { p[0]=(unsigned char)(n>>8); p[1]=(unsigned char)n; }
static void put32(unsigned char *p, uint32_t n) {
    p[0]=(unsigned char)(n>>24); p[1]=(unsigned char)(n>>16);
    p[2]=(unsigned char)(n>>8); p[3]=(unsigned char)n;
}
static double scan_clock(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;
}
typedef struct {
    IOUSBInterfaceInterface300 **intf;
    UInt8 in,out;
    int synchronized;
} ScanTransport;

/* A timeout or invalid frame makes subsequent commands unsafe: a delayed
 * response might belong to the previous command. Never reset or retry it. */
static int scan_status(ScanTransport *s, const unsigned char *p, unsigned n, unsigned tag) {
    if (!status_packet(p,n) || be32(p+4)!=tag || p[9] || p[10] || p[11]) {
        puts("Invalid or mismatched scan status; command channel is no longer synchronized.");
        return -1;
    }
    s->synchronized=1;
    printf("Command tag 0x%02x status: 0x%02x\n",tag,p[8]);
    return p[8];
}
/* Returns 0, device status 1..255, or -1 for transport/framing failure.
 * A complete early error STS is consumed, so it remains safe to clean up. */
static int scan_exchange(ScanTransport *s, unsigned tag, unsigned opcode,
                         unsigned char *data, unsigned length, int receive,
                         const unsigned char parameters[6], const char *name) {
    if (!s->synchronized || length>SCAN_LIMIT || (length && !data)) return -1;
    unsigned char command[20]={'C','M','D',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    put32(command+4,tag); put32(command+8,length);
    command[12]=receive?0x80:0; command[13]=(unsigned char)opcode;
    if (parameters) memcpy(command+14,parameters,6);
    printf("%s: %s %u bytes\n",name,receive?"receive":"send",length);
    dump("TX command endpoint 0x02",command,20);
    s->synchronized=0;
    IOReturn r=(*s->intf)->WritePipeTO(s->intf,s->out,command,20,3000,5000);
    if (r) { result("Scan command write",r); return -1; }
    if (receive && length) {
        unsigned used=0;
        unsigned char small[12];
        while (used<length) {
            unsigned want=length-used;
            /* Submit the whole remaining command payload to IOKit. Do not
             * impose a second1MiB split on the device's negotiated block. */
            UInt32 n=want<12?12:want;
            unsigned char *target=want<12?small:data+used;
            r=(*s->intf)->ReadPipeTO(s->intf,s->in,target,&n,8000,10000);
            if (r) { result("Scan data read",r); return -1; }
            if (!used && status_packet(target,n)) {
                printf("%s: status arrived before any payload.\n",name);
                int status=scan_status(s,target,n,tag);
                if (status==0) { puts("Success status without required data."); return -1; }
                return status;
            }
            if (!n || n>want) { puts("Invalid scan payload length."); return -1; }
            if (want<12) memcpy(data+used,small,n);
            used+=n;
        }
        printf("%s: received %u payload bytes; awaiting completion status.\n",name,used);
    } else if (length) {
        dump("TX settings endpoint 0x02",data,length);
        r=(*s->intf)->WritePipeTO(s->intf,s->out,data,length,3000,5000);
        if (r) { result("Scan settings write",r); return -1; }
    }
    unsigned char status[12]={0}; UInt32 n=12;
    r=(*s->intf)->ReadPipeTO(s->intf,s->in,status,&n,8000,10000);
    if (r) { result("Scan completion read",r); return -1; }
    return scan_status(s,status,n,tag);
}

static int scan_save(const char *name, const void *bytes, size_t size) {
    char path[PATH_MAX];
    int n=snprintf(path,sizeof(path),"%s/%s",scan_directory,name);
    if (n<0 || (size_t)n>=sizeof(path)) return 1;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if (fd<0) { perror(path); return 1; }
    const unsigned char *p=bytes; size_t used=0; int failed=0;
    while (used<size) {
        ssize_t count=write(fd,p+used,size-used);
        if (count<0 && errno==EINTR) continue;
        if (count<=0) { perror("Saving scan data"); failed=1; break; }
        used+=(size_t)count;
    }
    if (fsync(fd)) failed=1;
    if (close(fd)) failed=1;
    if (!failed) printf("Saved: %s (%zu bytes)\n",path,size);
    else fprintf(stderr,"Incomplete file: %s\n",path);
    return failed;
}

static void scan_settings(const unsigned char original[757], unsigned char config[757],
                          unsigned char camera[36], unsigned char window[87]) {
    memcpy(config,original,757);
    config[0]=1; config[1]=0; config[2]=1; /* autostart, normal handling, timeout */
    put16(config+3,10); put16(config+5,10); config[7]=0; /* end job on timeout */
    /* Image count and sheet count below bound this job. Do not enable a second
     * batching mechanism or change its dormant count/level/start/end actions.
     * v7 hardware retained batchendfunction=1 after restoring its original 0. */
    config[8]=0;
    config[85]=0; put16(config+86,1); /* one image, no paper-on-demand */
    config[93]=1; config[104]=0; /* multifeed ends job; ADF */
    config[105]=0; config[106]=0;
    memset(config+108,0,11); /* patch processing and physical printer off */
    config[536]=0; config[545]=0; config[564]=0; /* merge/sorter/indexing off */
    config[698]=0; config[706]=0; config[707]=0;
    memset(config+716,0,6); /* patch job controls off */
    config[738]=0; config[739]=1; config[740]=2; /* output tray, separation, normal feed */
    memset(config+741,0,4); /* no flatbed, sleeve or splitting */
    put16(config+755,1); /* one sheet */
    memset(camera,0,36); camera[0]=SCAN_FRONT_CAMERA_ID;
    /* Vendor Default profile: autocoloramount default/minimum=1 (range1..200),
     * autocolorthreshold default=0x14. Detection remains off. Even inactive
     * parameters must not be populated with out-of-range zero placeholders. */
    camera[3]=1; camera[4]=20;
    memset(window,0,87);
    window[0]=0; window[1]=1; window[2]=SCAN_FRONT_CAMERA_ID;
    /* BW uses the proven gray acquisition, then local1-bit thresholding. */
    window[3]=scan_mode==SCAN_COLOR?2:1;
    put16(window+4,scan_dpi); put16(window+6,scan_dpi);
    put32(window+16,9920); put32(window+20,14032); /* A4 approx, units 1/1200 inch */
    window[24]=3; /* fixed crop; no deskew or compression */
    const unsigned neutral[]={26,31,32,33,34,35,41,74,79,81,82};
    for (unsigned i=0;i<sizeof(neutral)/sizeof(neutral[0]);i++) window[neutral[i]]=100;
    /* convert=13 encodes signed adjustment as value+100, NOT two's complement. */
    window[30]=128; window[38]=2; window[39]=255; window[65]=3; window[86]=255;
}

static void le16(unsigned char *p, unsigned n) { p[0]=(unsigned char)n; p[1]=(unsigned char)(n>>8); }
static void le32(unsigned char *p, uint32_t n) { le16(p,n); le16(p+2,n>>16); }
static int scan_tiff(const unsigned char header[213], const unsigned char *pixels, unsigned size) {
    /* ImageHeader dimensions are in 1/1200 inch (also used by the vendor's
     * JPEG header construction). Reject any ambiguous stride or chunk layout. */
    unsigned x=be16(header+9),y=be16(header+11);
    uint32_t wu=be32(header+21),hu=be32(header+25);
    unsigned samples=scan_mode==SCAN_COLOR?3:1;
    if (header[0]!=SCAN_FRONT_CAMERA_ID || header[1]!=0 || header[2]!=0 || header[3]!=0 || header[8]!=(samples==3?2:1) ||
        header[179]!=0 || header[184]!=0 || header[31]!=1 || header[32]!=0 ||
        !scan_dpi_valid(x) || x!=scan_dpi || y!=x || !wu || !hu || wu>11000 || hu>15000 ||
        ((uint64_t)wu*x)%1200 || ((uint64_t)hu*y)%1200 || size>SCAN_IMAGE_LIMIT) {
        puts("Image layout not supported by this first test; raw data and header retained."); return 1;
    }
    unsigned width=wu*x/1200,height=hu*y/1200;
    if ((uint64_t)width*height*samples!=size || be32(header+4)!=size) {
        printf("Image size mismatch: %ux%u pixels, %u channels, %u bytes. Raw data retained.\n",width,height,samples,size);
        return 1;
    }
    unsigned bits=scan_mode==SCAN_BW?1:8;
    unsigned output_size=bits==1?((width+7)/8)*height:size;
    enum { TAGS=13, BASE=8+2+TAGS*12+4 };
    unsigned prefix=BASE+16+(samples==3?6:0);
    unsigned char *tiff=calloc(1,prefix+output_size);
    if (!tiff) return 1;
    tiff[0]='I'; tiff[1]='I'; le16(tiff+2,42); le32(tiff+4,8); le16(tiff+8,TAGS);
    const uint32_t tags[TAGS][4]={
        {256,4,1,width},{257,4,1,height},{258,3,samples,samples==3?BASE+16:bits},{259,3,1,1},
        {262,3,1,samples==3?2:1},{273,4,1,prefix},{274,3,1,1},{277,3,1,samples},
        {278,4,1,height},{279,4,1,output_size},{282,5,1,BASE},
        {283,5,1,BASE+8},{296,3,1,2}};
    for (unsigned i=0;i<TAGS;i++) {
        unsigned char *p=tiff+10+i*12;
        le16(p,tags[i][0]); le16(p+2,tags[i][1]); le32(p+4,tags[i][2]); le32(p+8,tags[i][3]);
    }
    le32(tiff+BASE,x); le32(tiff+BASE+4,1);
    le32(tiff+BASE+8,y); le32(tiff+BASE+12,1);
    if (samples==3) for (unsigned c=0;c<3;c++) le16(tiff+BASE+16+2*c,8);
    if (bits==1) {
        unsigned char *finished=malloc(size);
        if (!finished) { free(tiff);return 1; }
        memcpy(finished,pixels,size);
        cleanup_apply(finished,width,height,samples,x,scan_paper_cleanup,scan_edge_cleanup);
        /* TIFF defaults to MSB-first, BlackIsZero. Each row is byte-aligned. */
        unsigned stride=(width+7)/8;
        for (unsigned row=0;row<height;row++) for (unsigned col=0;col<width;col++)
            if (finished[row*width+col]>=128) tiff[prefix+row*stride+col/8]|=(unsigned char)(0x80u>>(col%8));
        free(finished);
    } else {
        memcpy(tiff+prefix,pixels,size);
        cleanup_apply(tiff+prefix,width,height,samples,x,scan_paper_cleanup,scan_edge_cleanup);
    }
    int failed=scan_save("scan.tiff",tiff,prefix+output_size); free(tiff); return failed;
}

static int scan_one(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out, unsigned version) {
    if (!scan_dpi_valid(scan_dpi) || scan_mode<SCAN_GRAY || scan_mode>SCAN_BW) return 1;
    if (version!=14) { puts("First scan test requires interface version 14; no settings written."); return 1; }
    ScanTransport s={intf,in,out,1};
    unsigned char original[757],config[757],camera[36],window[87],paper=0;
    unsigned char begin[8],header[213],end[12],order[4]={0,255,255,255};
    unsigned char parameters[6]={0}; put32(parameters,SCAN_LIMIT);
    unsigned char *pixels=NULL; unsigned image_size=0,image_received=0;
    int changed=0,started=0,ended=0,failed=1,cleanup_failed=0;
    if (scan_exchange(&s,0x0e,0x20,original,757,1,NULL,"READ CONFIG")) return 1;
    if (scan_save("config-before.bin",original,757)) return 1;
    if (scan_exchange(&s,0x0d,0x17,&paper,1,1,NULL,"PAPER SENSOR")) return 1;
    if (paper!=1) { puts("No paper confirmed. Load ONE A4 sheet; no settings written or scan started."); return 1; }
    if (scan_cancelled) return 1;
    scan_settings(original,config,camera,window);
    if (scan_save("config-test.bin",config,757)) return 1;
    printf("Starting single-sheet A4 front scan, %u dpi, %s.\n",scan_dpi,
           scan_mode==SCAN_COLOR?"RGB color":scan_mode==SCAN_BW?"black/white (local threshold128)":"grayscale");
    changed=1;
    if (scan_exchange(&s,0x0f,0x20,config,757,0,NULL,"SET CONFIG")) goto cleanup;
    if (scan_cancelled || scan_exchange(&s,0x10,0x21,camera,36,0,NULL,"CREATE CAMERA")) goto cleanup;
    if (scan_cancelled || scan_exchange(&s,0x11,0x22,window,87,0,NULL,"ADD WINDOW")) goto cleanup;
    if (scan_cancelled || scan_exchange(&s,0x12,0x23,order,4,0,NULL,"SET WINDOW ORDER")) goto cleanup;
    if (scan_cancelled || scan_exchange(&s,0x13,0x30,begin,8,1,parameters,"SCAN BEGIN")) goto cleanup;
    started=1;
    dump("SCAN BEGIN reply",begin,8);
    if (scan_save("scan-begin.bin",begin,8)) goto cleanup;
    unsigned chunk_size=be32(begin);
    if (!chunk_size || chunk_size>SCAN_LIMIT) { puts("Unexpected negotiated image chunk size."); goto cleanup; }
    printf("Negotiated image data block: %u bytes.\n",chunk_size);
    double deadline=scan_clock()+45;
    int response; unsigned pending=0;
    do {
        if (scan_cancelled || scan_clock()>deadline) { puts("Scan cancelled or deadline reached."); goto cleanup; }
        response=scan_exchange(&s,0x14,0x31,header,213,1,NULL,"IMAGE HEADER");
        /* statusdevice maps raw USB01 -> internal enum16 (0x10). The vendor
         * ReadImage compares that translated enum. We receive the RAW byte.
         * USB10 instead maps to enum23 and must NOT be retried. */
        if (response==0x01) {
            if (!pending++) puts("Image not ready (USB status 0x01); waiting for scanner, up to 45 seconds.");
            if (!scan_cancelled) { struct timespec delay={0,200000000}; nanosleep(&delay,NULL); }
        }
    } while (response==0x01);
    if (response) goto cleanup;
    if (pending) printf("Image ready after %u pending replies.\n",pending);
    dump("IMAGE HEADER",header,213);
    if (scan_save("image-header.bin",header,213)) goto cleanup;
    image_size=be32(header+4);
    printf("Image bytes: %u; physical size: %u x %u /1200 inch; chunk: %u; last: %u\n",
           image_size,be32(header+21),be32(header+25),be16(header+29),header[31]);
    /* dmimagechunksize limits each request, not the total imagesize from the
     * header. Vendor ReadImage (0xe0124..0xe0159) requests min(remaining,limit).
     * The real scanner negotiated2MiB; A4 gray150dpi is slightly larger. */
    if (!image_size || image_size>SCAN_IMAGE_LIMIT) {
        puts("Image exceeds the128 MiB memory bound."); goto cleanup;
    }
    deadline=scan_clock()+120; /* Larger color images; bounded data phase. */
    pixels=malloc(image_size);
    if (!pixels) goto cleanup;
    for (unsigned pos=0;pos<image_size;) {
        if (scan_cancelled || scan_clock()>deadline) goto cleanup;
        unsigned n=image_size-pos;
        if (n>chunk_size) n=chunk_size;
        /* Vendor ReadImage: EXACT min(remaining,dmimagechunksize) per opcode32,
         * tag000000ff. v9's extra1MiB command cap diverged from that sequence. */
        if (scan_exchange(&s,0xff,0x32,pixels+pos,n,1,NULL,"IMAGE DATA")) goto cleanup;
        pos+=n; image_received=pos;
        printf("Image received and confirmed: %u / %u bytes\n",pos,image_size);
    }
    if (scan_save("image.raw",pixels,image_size)) goto cleanup;
    failed=scan_tiff(header,pixels,image_size);
cleanup:
    if (failed && pixels && image_received && image_received<image_size) {
        printf("Incomplete image: %u / %u bytes confirmed. Saving diagnostic prefix, no TIFF.\n",
               image_received,image_size);
        if (scan_save("image.partial.raw",pixels,image_received)) cleanup_failed=1;
    }
    free(pixels);
    if (started && s.synchronized) {
        if (scan_exchange(&s,0x15,0x33,NULL,0,0,NULL,"STOP FEEDER")) cleanup_failed=1;
        if (s.synchronized && !scan_exchange(&s,0x16,0x34,end,12,1,NULL,"SCAN END")) ended=1;
        else cleanup_failed=1;
    }
    if (changed && s.synchronized && (!started || ended)) {
        if (scan_exchange(&s,0x0f,0x20,original,757,0,NULL,"RESTORE CONFIG")) cleanup_failed=1;
        else {
            unsigned char verify[757];
            if (scan_exchange(&s,0x0e,0x20,verify,757,1,NULL,"VERIFY RESTORED CONFIG")) {
                puts("Restored configuration could not be read back."); cleanup_failed=1;
            } else {
                if (scan_save("config-after.bin",verify,757)) cleanup_failed=1;
                unsigned differences=0;
                for (unsigned i=0;i<757;i++) if (verify[i]!=original[i]) {
                    printf("CONFIG DIFFERENCE offset=%u field=%s before=0x%02x after=0x%02x\n",
                           i,config_field_name(i),original[i],verify[i]);
                    differences++;
                }
                if (differences) {
                    printf("Restored configuration differs at %u byte positions; readback retained in config-after.bin if saving succeeded.\n",differences);
                    cleanup_failed=1;
                } else puts("Original configuration restored and verified.");
            }
        }
    }
    if (!s.synchronized) {
        puts("Command channel uncertain: no further commands sent. Power-cycle scanner before another test.");
        cleanup_failed=1;
    }
    if (started && !ended) puts("SCAN END not confirmed; configuration restore skipped.");
    if (cleanup_failed) puts("Scan cleanup incomplete. Power-cycle scanner before another test.");
    if (!failed) puts("Single-page image saved as scan.tiff.");
    return failed || cleanup_failed || scan_cancelled;
}
