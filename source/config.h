/* Read-only protocol commands from the vendor's sage resource. */
static unsigned config_size(unsigned version) {
    static const unsigned sizes[]={0,545,545,545,552,564,564,697,698,716,738,739,744,757,757,757};
    return version<sizeof(sizes)/sizeof(sizes[0])?sizes[version]:0;
}

static int receive_block(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out,
                         unsigned char tag, unsigned char opcode, const char *name,
                         unsigned char *data, unsigned expected) {
    if (!in || !out || !data || !expected || expected>757) return 1;
    unsigned char command[20]={'C','M','D',0,0,0,0,tag,0,0,0,0,0x80,opcode,0,0,0,0,0,0};
    command[10]=(unsigned char)(expected>>8); command[11]=(unsigned char)expected;
    printf("Reading %s (%u bytes); no settings write.\n",name,expected);
    dump("TX endpoint 0x02",command,sizeof(command));
    IOReturn r=(*intf)->WritePipeTO(intf,out,command,sizeof(command),3000,5000);
    result("Query write",r); if (r) return 1;
    /* Hold early STS errors even for a one-byte sensor result. */
    unsigned char response[757]={0},status[12]={0};
    UInt32 n=expected<12?12:expected;
    r=(*intf)->ReadPipeTO(intf,in,response,&n,3000,5000);
    result("Query read",r); if (r || n>sizeof(response)) return 1;
    dump(name,response,n);
    if (status_packet(response,n)) {
        printf("%s returned status 0x%02x without data.\n",name,response[8]); return 1;
    }
    if (n!=expected) { puts("Unexpected payload length; no decoded configuration."); return 1; }
    n=sizeof(status);
    r=(*intf)->ReadPipeTO(intf,in,status,&n,3000,5000);
    result("Query completion",r); if (r || n>sizeof(status)) return 1;
    dump("Query status",status,n);
    if (!status_packet(status,n) || memcmp(status+4,command+4,4) || status[8]) {
        puts("Query status failed or belongs to another command."); return 1;
    }
    memcpy(data,response,expected); return 0;
}

static unsigned be16(const unsigned char *p) { return ((unsigned)p[0]<<8)|p[1]; }
static int read_configuration(IOUSBInterfaceInterface300 **intf, UInt8 in, UInt8 out,
                              unsigned version) {
    unsigned size=config_size(version);
    if (!size) return 1;
    unsigned char config[757]={0},paper=0;
    if (receive_block(intf,in,out,0x0e,0x20,"CONFIG",config,size)) return 1;
    printf("CONFIG confirmed: %u bytes\n",size);
    printf("ADF autostart: %u; handling: %u; timeout enabled: %u\n",config[0],config[1],config[2]);
    printf("ADF timeout: %u; first page timeout: %u; timeout response: %u\n",
           be16(config+3),be16(config+5),config[7]);
    printf("Batch count enabled: %u; batch count: %u; image count: %u\n",
           config[8],be16(config+9),be16(config+86));
    printf("Paper source: %u; printer enabled: %u; multifeed response: %u\n",
           config[104],config[118],config[93]);
    if (version>=12) printf("ADF separation: %u; active feed: %u; sleeve mode: %u\n",
                           config[739],config[740],config[742]);
    if (receive_block(intf,in,out,0x0d,0x17,"PAPER DETECTED",&paper,1)) return 1;
    if (paper>1) { printf("Unrecognized paper sensor value: %u\n",paper); return 1; }
    printf("Paper detected: %s\n",paper?"yes":"no");
    puts("Configuration inspection complete. No scan started or configuration written.");
    return 0;
}
