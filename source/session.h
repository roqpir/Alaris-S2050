/* Resource handshake reconstructed from Kodak Linux 08.02.00.00.
 * No takeover or forced resource unlock. */
#include <libxml/parser.h>
#include <stdlib.h>
#include <limits.h>

static size_t task_xml(char *buf, size_t cap, unsigned id, const char *body) {
    int n=snprintf(buf,cap,"<task bytes='%020u' id='%020u' reply='%020u'>\n%s</task>\n",0u,id,0u,body);
    if (n<0 || (size_t)n>=cap) return 0;
    char length[21];
    snprintf(length,sizeof(length),"%020u",(unsigned)n);
    memcpy(buf+13,length,20);
    return (size_t)n;
}

static int decimal(const char *s, unsigned *value) {
    if (!s || !*s) return 0;
    unsigned v=0;
    for (;*s;s++) {
        if (*s<'0' || *s>'9' || v>(UINT_MAX-(unsigned)(*s-'0'))/10) return 0;
        v=v*10+(unsigned)(*s-'0');
    }
    *value=v; return 1;
}

/* Read only the bounded framing header, before parsing a complete XML task. */
static int frame_size(const char *s, unsigned *size) {
    if (strlen(s)<6) return !strncmp(s,"<task ",strlen(s))?0:-1;
    if (strncmp(s,"<task ",6)) return -1;
    const char *end=strchr(s,'>');
    if (!end) return 0;
    const char *p=strstr(s," bytes=");
    if (!p || p>end || (p[7]!='\'' && p[7]!='"')) return -1;
    char quote=p[7], digits[21]; p+=8;
    size_t n=0;
    while (p<end && *p!=quote && n<20) digits[n++]=*p++;
    digits[n]=0;
    if (p>=end || *p!=quote || !decimal(digits,size) || *size<=(unsigned)(end-s) || *size>4096) return -1;
    return 1;
}

static int xml_exchange(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out,
                        char *command, size_t len, char response[4097]) {
    printf("TX endpoint 0x01 (%zu bytes):\n%s",len,command);
    IOReturn r=(*intf)->WritePipeTO(intf,out,command,(UInt32)len,3000,5000);
    result("Resource write",r);
    if (r) return 1;
    unsigned used=0,expected=0;
    for (unsigned attempt=0;attempt<8;attempt++) {
        UInt32 n=expected?expected-used:1024;
        if (!n || n>4096-used) return 1;
        r=(*intf)->ReadPipeTO(intf,in,response+used,&n,3000,5000);
        result("Resource read",r);
        if (r || !n || n>4096-used) return 1;
        used+=n; response[used]=0;
        if (memchr(response,0,used)) { puts("Embedded NUL in resource response."); return 1; }
        int frame=frame_size(response,&expected);
        if (frame<0 || (frame>0 && used>expected)) {
            dump("Unrecognized resource response",(unsigned char *)response,used); return 1;
        }
        if (frame>0 && used==expected) {
            print_field("RX endpoint 0x81 XML",(unsigned char *)response,used);
            return 0;
        }
    }
    puts("Resource response incomplete; stopping."); return 1;
}

typedef struct { unsigned statuses, locks, modes; int success, normal, valid; unsigned lock; } Reply;
static void reply_fields(xmlNode *node, Reply *r) {
    for (;node;node=node->next) {
        if (node->type==XML_ELEMENT_NODE) {
            int status=!xmlStrcmp(node->name,BAD_CAST "status");
            int lock=!xmlStrcmp(node->name,BAD_CAST "lockid");
            int mode=!xmlStrcmp(node->name,BAD_CAST "lockmode");
            if (status || lock || mode) {
                for (xmlNode *c=node->children;c;c=c->next)
                    if (c->type!=XML_TEXT_NODE && c->type!=XML_CDATA_SECTION_NODE) r->valid=0;
                xmlChar *v=xmlNodeGetContent(node);
                if (status) { r->statuses++; r->success=v && !xmlStrcmp(v,BAD_CAST "success"); }
                if (lock) { r->locks++; if (!decimal((const char *)v,&r->lock) || !r->lock) r->valid=0; }
                if (mode) { r->modes++; r->normal=v && !xmlStrcmp(v,BAD_CAST "normal"); }
                xmlFree(v);
            }
        }
        reply_fields(node->children,r);
    }
}
static int parse_reply(const char *s, unsigned task, int require_lock, unsigned *lock) {
    xmlDoc *doc=xmlReadMemory(s,(int)strlen(s),"usb-response.xml",NULL,
        XML_PARSE_NONET|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    if (!doc) return 0;
    xmlNode *root=xmlDocGetRootElement(doc);
    int ok=root && !xmlStrcmp(root->name,BAD_CAST "task") && !doc->intSubset && !doc->extSubset;
    xmlChar *reply=root?xmlGetProp(root,BAD_CAST "reply"):NULL;
    xmlChar *task_id=root?xmlGetProp(root,BAD_CAST "id"):NULL;
    unsigned reply_id=0,id=0;
    /* Observed S2050 firmware echoes the request in id and sets reply=0.
     * Keep the explicit reply-ID form, but never let a matching id override
     * a nonzero reply that belongs to another request. */
    ok=ok && task!=0 && decimal((char *)reply,&reply_id) && decimal((char *)task_id,&id)
        && (reply_id==task || (reply_id==0 && id==task));
    xmlFree(reply); xmlFree(task_id);
    Reply fields={.valid=1};
    if (ok) reply_fields(root->children,&fields);
    ok=ok && fields.valid && fields.statuses==1 && fields.success && fields.modes<=1;
    if (require_lock) ok=ok && fields.locks==1 && (!fields.modes || fields.normal);
    if (ok && lock) *lock=fields.lock;
    xmlFreeDoc(doc); return ok;
}

static unsigned inquiry_size(unsigned version) {
    /* sage/inquiry fields; CDevMgrCreate::UseInterface adjusts bytes 8..11
     * to the last supported response field. Values verified against XML. */
    static const unsigned sizes[]={0,108,108,110,123,131,131,132,148,190,198,198,223,235,236,237};
    return version<sizeof(sizes)/sizeof(sizes[0])?sizes[version]:0;
}

static int negotiate_version(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out,
                             unsigned *version) {
    /* CheckInterfaceVersion at 0xe1238: 20-byte command, tag 1, opcode 1,
     * host interface 15 in big-endian at 14..15; receive two bytes then STS.
     * No retry. This changes only the active session's protocol version. */
    unsigned char command[20]={0x43,0x4d,0x44,0,0,0,0,1,0,0,0,2,0x80,1,0,15,0,0,0,0};
    unsigned char data[12]={0},status[12]={0};
    *version=0;
    dump("TX VERSION endpoint 0x02",command,sizeof(command));
    IOReturn r=(*intf)->WritePipeTO(intf,out,command,sizeof(command),3000,5000);
    result("VERSION write",r);
    if (r) return 1;
    /* At least 12 bytes of capacity so an early error STS fits as well. */
    UInt32 n=sizeof(data);
    r=(*intf)->ReadPipeTO(intf,in,data,&n,3000,5000);
    result("VERSION read",r);
    if (r || n>sizeof(data)) return 1;
    dump("RX VERSION endpoint 0x82",data,n);
    if (status_packet(data,n)) {
        printf("VERSION rejected: status 0x%02x; no INQUIRY sent.\n",data[8]);
        return 1;
    }
    if (n!=2) { puts("Unexpected VERSION payload; no INQUIRY sent."); return 1; }
    n=sizeof(status);
    r=(*intf)->ReadPipeTO(intf,in,status,&n,3000,5000);
    result("VERSION completion",r);
    if (r || n>sizeof(status)) return 1;
    dump("VERSION status",status,n);
    if (!status_packet(status,n) || memcmp(status+4,command+4,4) || status[8]) {
        puts("VERSION completion not successful or not matching; no INQUIRY sent."); return 1;
    }
    unsigned v=((unsigned)data[0]<<8)|data[1];
    printf("Negotiated interface version: %u\n",v);
    if (!inquiry_size(v)) { puts("Unsupported interface version; no INQUIRY sent."); return 1; }
    *version=v; return 0;
}

#include "config.h"
#include "scan.h"
#include "pdf.h"
#include "stack.h"

static int session_inquiry(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out,
                           UInt8 lock_in, UInt8 lock_out, int inspect_config) {
    if (!in || !out || !lock_in || !lock_out) {
        puts("Required bulk and resource endpoints not found."); return 1;
    }
    char command[1024],body[512],response[4097];
    snprintf(body,sizeof(body),"\t<resourcelock>\n\t\t<userid>s2050-probe</userid>\n"
        "\t\t<pcid>s2050-probe-%08x%08x</pcid>\n\t</resourcelock>\n",arc4random(),arc4random());
    size_t n=task_xml(command,sizeof(command),1,body);
    unsigned lock=0;
    if (!n || xml_exchange(intf,lock_in,lock_out,command,n,response)) {
        puts("Resource lock not confirmed. No INQUIRY sent. If a lock remains, power-cycle the scanner.");
        return 1;
    }
    if (!parse_reply(response,1,1,&lock)) {
        puts("No matching successful lock reply with a valid lock ID. No INQUIRY or forced unlock sent. Power-cycle the scanner before another test.");
        return 1;
    }
    printf("Resource lock confirmed: %u\n",lock);
    unsigned version=0;
    int failed=negotiate_version(intf,in,out,&version);
    if (!failed) failed=inquiry(intf,in,out,inquiry_size(version));
    if (!failed && inspect_config==1) failed=read_configuration(intf,in,out,version);
    if (!failed && inspect_config==2) failed=scan_one(intf,in,out,version);
    if (!failed && inspect_config==3) failed=scan_stack(intf,in,out,version);
    snprintf(body,sizeof(body),"\t<resourceunlock>\n\t\t<lockid>%u</lockid>\n\t</resourceunlock>\n",lock);
    n=task_xml(command,sizeof(command),2,body);
    if (!n || xml_exchange(intf,lock_in,lock_out,command,n,response) || !parse_reply(response,2,0,NULL)) {
        puts("Resource release not confirmed. Power-cycle the scanner before the next test."); return 1;
    }
    puts("Resource release confirmed.");
    return failed;
}
