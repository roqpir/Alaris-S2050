#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdio.h>
#include <string.h>
#include <mach/mach_error.h>
#include <stdint.h>
#include <ctype.h>

/* Query modes only read. --scan-one explicitly runs the experimental scan. */
static void result(const char *action, IOReturn r) {
    printf("%s: 0x%08x (%s)\n", action, r, mach_error_string(r));
}
static void match_number(CFMutableDictionaryRef d, CFStringRef key, int value) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &value);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}
static int descriptors(IOUSBDeviceInterface320 **dev) {
    UInt8 count = 0;
    IOReturn r = (*dev)->GetNumberOfConfigurations(dev, &count);
    if (r) { result("GetNumberOfConfigurations", r); return 1; }
    printf("Configurations: %u\n", count);
    for (UInt8 i=0; i<count; i++) {
        IOUSBConfigurationDescriptorPtr desc = NULL;
        r = (*dev)->GetConfigurationDescriptorPtr(dev, i, &desc);
        if (r || !desc) { result("GetConfigurationDescriptorPtr", r); return 1; }
        const unsigned char *b = (const unsigned char *)desc;
        unsigned total = b[2] | ((unsigned)b[3] << 8);
        if (total < 9) { fprintf(stderr,"Invalid configuration length\n"); return 1; }
        printf("Configuration index %u, value %u, length %u\n", i, b[5], total);
        for (unsigned p=0; p<total;) {
            if (p+2>total || b[p]<2 || p+b[p]>total) {
                fprintf(stderr,"Malformed descriptor at %u\n",p); return 1;
            }
            printf("  descriptor:");
            for (unsigned j=0; j<b[p]; j++) printf(" %02x",b[p+j]);
            putchar('\n');
            if (b[p+1]==4 && b[p]>=9)
                printf("  interface=%u alternate=%u endpoints=%u class=%02x/%02x/%02x\n",
                    b[p+2],b[p+3],b[p+4],b[p+5],b[p+6],b[p+7]);
            if (b[p+1]==5 && b[p]>=7)
                printf("  endpoint=0x%02x direction=%s type=%u packet=%u\n",
                    b[p+2],(b[p+2]&0x80)?"IN":"OUT",b[p+3]&3,
                    b[p+4]|((unsigned)b[p+5]<<8));
            p+=b[p];
        }
    }
    return 0;
}
static void dump(const char *label, const unsigned char *b, size_t n) {
    printf("%s (%zu bytes):\n",label,n);
    for (size_t i=0;i<n;i++) {
        printf("%02x%s",b[i],i%16==15?"\n":" ");
    }
    if (n%16) putchar('\n');
}
static int status_packet(const unsigned char *b, size_t n) {
    return n==12 && memcmp(b,"STS\0",4)==0;
}
static void print_field(const char *label, const unsigned char *b, size_t n) {
    printf("%s: ",label);
    for (size_t i=0;i<n && b[i];i++) putchar(b[i]>=32 && b[i]<=126?b[i]:'?');
    putchar('\n');
}
static int inquiry(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out, unsigned expected) {
    /* Kodak Linux 08.02.00.00: devicemanager.so, sage/create[name=inquiry].
     * Expected payload: up to 237 bytes, depending on negotiated version; completion: STS\0, 12 bytes, status at +8.
     * Endpoint selection: device.so/devosusbopen, BULKIN/BULKOUT = endpoint 2. */
    unsigned char command[20]={0x43,0x4d,0x44,0,0,0,0,2,0,0,0,0xed,0x80,0x10,0,0,0,0,0,0};
    unsigned char data[237]={0}, status[12]={0};
    if (expected<28 || expected>sizeof(data)) return 1;
    command[11]=(unsigned char)expected;
    if (!in || !out) { fputs("Required endpoint pair 02/82 not found.\n",stderr); return 1; }
    puts("Sending one INQUIRY command (no scan, no settings change).");
    dump("TX endpoint 0x02",command,sizeof(command));
    IOReturn r=(*intf)->WritePipeTO(intf,out,command,sizeof(command),3000,5000);
    result("INQUIRY write",r);
    if (r) return 1;
    UInt32 n=expected;
    r=(*intf)->ReadPipeTO(intf,in,data,&n,3000,5000);
    result("INQUIRY read",r);
    if (r) { puts("No complete response; no retry or reset performed."); return 1; }
    if (n>sizeof(data)) return 1;
    dump("RX endpoint 0x82",data,n);
    if (status_packet(data,n)) {
        printf("Device returned status without inquiry data: 0x%02x\n",data[8]);
        puts("The device returned no inquiry payload.");
        return 1;
    }
    UInt32 sn=sizeof(status);
    r=(*intf)->ReadPipeTO(intf,in,status,&sn,3000,5000);
    result("Completion read",r);
    if (r) return 1;
    if (sn>sizeof(status)) return 1;
    dump("Completion",status,sn);
    if (!status_packet(status,sn)) { puts("Unrecognized completion; inquiry not confirmed."); return 1; }
    printf("Device status: 0x%02x\n",status[8]);
    if (memcmp(status+4,command+4,4) || status[8]!=0 || n!=expected) {
        puts("Inquiry incomplete or rejected; no decoded device information."); return 1;
    }
    printf("Device interface version: %u\n",((unsigned)data[0]<<8)|data[1]);
    print_field("Firmware",data+2,10);
    print_field("Serial number",data+12,16);
    puts("INQUIRY completed successfully.");
    return 0;
}
#include "session.h"

static int interfaces(IOUSBDeviceInterface320 **dev, int open_interface, int query) {
    IOUSBFindInterfaceRequest request = {kIOUSBFindInterfaceDontCare,
        kIOUSBFindInterfaceDontCare,kIOUSBFindInterfaceDontCare,kIOUSBFindInterfaceDontCare};
    io_iterator_t iter = IO_OBJECT_NULL;
    IOReturn r = (*dev)->CreateInterfaceIterator(dev,&request,&iter);
    if (r) { result("CreateInterfaceIterator",r); return 1; }
    io_service_t service; unsigned found=0; int failed=0;
    while ((service=IOIteratorNext(iter))) {
        found++;
        IOCFPlugInInterface **plugin=NULL; SInt32 score=0;
        r=IOCreatePlugInInterfaceForService(service,kIOUSBInterfaceUserClientTypeID,
                                          kIOCFPlugInInterfaceID,&plugin,&score);
        IOObjectRelease(service);
        if (r || !plugin) { result("Interface plug-in",r); failed=1; continue; }
        IOUSBInterfaceInterface300 **intf=NULL;
        HRESULT q=(*plugin)->QueryInterface(plugin,
            CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID300),(LPVOID*)&intf);
        IODestroyPlugInInterface(plugin);
        if (q || !intf) { fprintf(stderr,"Interface QueryInterface failed: %d\n",q); failed=1; continue; }
        if (query) {
            UInt8 number=255, alternate=255;
            IOReturn nr=(*intf)->GetInterfaceNumber(intf,&number);
            IOReturn ar=(*intf)->GetAlternateSetting(intf,&alternate);
            if (nr || ar || number!=0 || alternate!=0) {
                puts("Skipping unexpected interface/alternate setting.");
                failed=1; (*intf)->Release(intf); continue;
            }
        }
        if (open_interface) {
            r=(*intf)->USBInterfaceOpen(intf);
            result("USBInterfaceOpen",r);
            if (r) { failed=1; (*intf)->Release(intf); continue; }
            UInt8 n=0;
            UInt8 input=0,output=0,lock_input=0,lock_output=0;
            int pipes_ok=1;
            r=(*intf)->GetNumEndpoints(intf,&n);
            if (r) { result("GetNumEndpoints",r); failed=1; pipes_ok=0; }
            else for (UInt8 p=1;p<=n;p++) {
                UInt8 direction,number,type,interval; UInt16 size;
                r=(*intf)->GetPipeProperties(intf,p,&direction,&number,&type,&size,&interval);
                if (r) { result("GetPipeProperties",r); failed=1; pipes_ok=0; }
                else {
                    printf("  pipe=%u endpoint=0x%02x type=%u packet=%u\n",p,
                        number|(direction==kUSBIn?0x80:0),type,size);
                    if (number==1 && type==kUSBBulk) {
                        if (direction==kUSBIn) lock_input=p;
                        if (direction==kUSBOut) lock_output=p;
                    }
                    if (number==2 && type==kUSBBulk) {
                        if (direction==kUSBIn) input=p;
                        if (direction==kUSBOut) output=p;
                    }
                }
            }
            if (query && pipes_ok) failed |= session_inquiry(intf,input,output,lock_input,lock_output,
                                                            query==5?3:(query==4?2:(query==3?1:0)));
            r=(*intf)->USBInterfaceClose(intf);
            result("USBInterfaceClose",r);
            if (r) failed=1;
        }
        (*intf)->Release(intf);
    }
    IOObjectRelease(iter);
    printf("Active interfaces found: %u\n",found);
    return failed || !found;
}
int main(int argc,char **argv) {
    setbuf(stdout,NULL);
    int query=argc==2 && strcmp(argv[1],"--inquiry")==0;
    if (argc==2 && strcmp(argv[1],"--session-inquiry")==0) query=2;
    if (argc==2 && strcmp(argv[1],"--session-config")==0) query=3;
    if (argc>=3 && strcmp(argv[1],"--scan-one")==0) { query=4; scan_directory=argv[2]; }
    if (argc>=3 && strcmp(argv[1],"--scan-stack")==0) { query=5; scan_directory=argv[2]; }
    if (query>=4 && scan_options(argc,argv,3)) {
        fputs("Invalid scan options. Use --dpi 150|300|600, --mode gray|color|bw, --paper-cleanup on|off, --edge-cleanup on|off.\n",stderr); return 2;
    }
    int open_interface=argc==2 && strcmp(argv[1],"--open")==0;
    if (argc==2 && strcmp(argv[1],"--help")==0) {
        printf("Usage: %s [--open | --inquiry | --session-inquiry | --session-config | --scan-one NEW_DIRECTORY | --scan-stack NEW_DIRECTORY]\n",argv[0]);
        puts("Scan options: --dpi 150|300|600 --mode gray|color|bw (defaults:150,gray).");
        puts("Document finishing: --paper-cleanup on|off --edge-cleanup on|off (defaults:on,on).");
        puts("--scan-one NEW_DIRECTORY: one A4 sheet, TIFF + PDF, front side.");
        puts("--scan-stack NEW_DIRECTORY: loaded stack to one PDF; ends on empty feeder, no reload wait. Maximum100 pages."); return 0;
    }
    if (argc>1 && !open_interface && !query) {
        fprintf(stderr,"Usage: %s [--open | --inquiry | --session-inquiry | --session-config | --scan-one NEW_DIRECTORY | --scan-stack NEW_DIRECTORY]\n",argv[0]); return 2;
    }
    puts("Alaris S2050 USB prototype v0.13 — experimental, not an ICA scanner driver");
    if (query>=4) {
        if (!*scan_directory || strlen(scan_directory)>PATH_MAX-64 || mkdir(scan_directory,0700)) {
            perror("Create new scan directory (must not already exist)"); return 1;
        }
        struct sigaction action={0}; action.sa_handler=scan_signal; sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL)) { perror("Signal handler"); return 1; }
        printf("Output directory: %s\n",scan_directory);
    }
    CFMutableDictionaryRef match=IOServiceMatching(kIOUSBDeviceClassName);
    if (!match) return 1;
    match_number(match,CFSTR(kUSBVendorID),0x29cc);
    match_number(match,CFSTR(kUSBProductID),0x1019);
    io_iterator_t iter=IO_OBJECT_NULL;
    IOReturn r=IOServiceGetMatchingServices(kIOMainPortDefault,match,&iter);
    if (r) { result("USB enumeration",r); return 1; }
    io_service_t service; unsigned found=0; int failed=0;
    if (query) {
        unsigned count=0;
        while ((service=IOIteratorNext(iter))) { count++; IOObjectRelease(service); }
        IOIteratorReset(iter);
        if (count!=1) {
            fprintf(stderr,"INQUIRY requires exactly one S2050; found %u.\n",count);
            IOObjectRelease(iter); return 1;
        }
    }
    while ((service=IOIteratorNext(iter))) {
        found++;
        printf("Found S2050 %u (29cc:1019)\n",found);
        IOCFPlugInInterface **plugin=NULL; SInt32 score=0;
        r=IOCreatePlugInInterfaceForService(service,kIOUSBDeviceUserClientTypeID,
                                          kIOCFPlugInInterfaceID,&plugin,&score);
        IOObjectRelease(service);
        if (r || !plugin) { result("Device plug-in",r); failed=1; continue; }
        IOUSBDeviceInterface320 **dev=NULL;
        HRESULT q=(*plugin)->QueryInterface(plugin,
            CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320),(LPVOID*)&dev);
        IODestroyPlugInInterface(plugin);
        if (q || !dev) { fprintf(stderr,"Device QueryInterface failed: %d\n",q); failed=1; continue; }
        if (query) {
            UInt8 config=0;
            r=(*dev)->GetConfiguration(dev,&config);
            if (r || config!=1) {
                result("GetConfiguration",r);
                printf("INQUIRY requires active configuration 1, found %u; not changed.\n",config);
                failed=1; (*dev)->Release(dev); continue;
            }
        } else failed |= descriptors(dev);
        failed |= interfaces(dev,open_interface || query,query);
        (*dev)->Release(dev);
    }
    IOObjectRelease(iter);
    if (!found) { fprintf(stderr,"S2050 not found.\n"); return 3; }
    if (query>=4 && scan_export_pdf(query==5,failed || scan_cancelled)) failed=1;
    if (scan_cancelled) return 130;
    return failed?1:0;
}
