/* Run proven single-sheet jobs in one USB/resource session. No wait for reload. */
#define SCAN_STACK_LIMIT 100u
static unsigned scan_pages_saved;
static int scan_stack(IOUSBInterfaceInterface300 **intf,UInt8 in,UInt8 out,unsigned version) {
    const char *root=scan_directory;
    ScanTransport sensor={intf,in,out,1};
    scan_pages_saved=0;
    if (version!=14) { puts("Stack scanning requires interface version14."); return 1; }
    while (!scan_cancelled) {
        unsigned char paper=0;
        if (scan_exchange(&sensor,0x0d,0x17,&paper,1,1,NULL,"STACK PAPER SENSOR")) return 1;
        if (!paper) {
            if (!scan_pages_saved) { puts("STACK_EMPTY No paper loaded."); return 1; }
            printf("STACK_COMPLETE %u\n",scan_pages_saved); return 0;
        }
        if (paper!=1) { puts("Unexpected paper sensor value; stack stopped."); return 1; }
        if (scan_pages_saved==SCAN_STACK_LIMIT) { puts("Stack limit of100 pages reached; remaining paper not scanned."); return 1; }
        char page[PATH_MAX];
        int n=snprintf(page,sizeof(page),"%s/page-%03u",root,scan_pages_saved+1);
        if (n<0 || (size_t)n>=sizeof(page) || mkdir(page,0700)) { perror("Create page directory"); return 1; }
        printf("PAGE_START %u\n",scan_pages_saved+1);
        scan_directory=page;
        int failed=scan_one(intf,in,out,version);
        scan_directory=root;
        /* Retain a complete TIFF even if USB cleanup subsequently fails. */
        char tiff[PATH_MAX]; struct stat st;
        n=snprintf(tiff,sizeof(tiff),"%s/scan.tiff",page);
        if (n>0 && (size_t)n<sizeof(tiff) && !lstat(tiff,&st) && S_ISREG(st.st_mode) && st.st_size>0) {
            scan_pages_saved++;
            printf("PAGE_SAVED %u\n",scan_pages_saved);
        } else if (!failed) failed=1;
        if (failed) return 1;
    }
    puts("Stack cancelled; completed pages retained."); return 1;
}
static int scan_export_pdf(int stack,int incomplete) {
    unsigned count=stack?scan_pages_saved:1;
    if (!count) return 0;
    char paths[SCAN_STACK_LIMIT][PATH_MAX],output[PATH_MAX];
    const char *pages[SCAN_STACK_LIMIT];
    for (unsigned i=0;i<count;i++) {
        int n=stack?snprintf(paths[i],PATH_MAX,"%s/page-%03u/scan.tiff",scan_directory,i+1):
                    snprintf(paths[i],PATH_MAX,"%s/scan.tiff",scan_directory);
        if (n<0 || n>=PATH_MAX) return 1;
        pages[i]=paths[i];
    }
    /* A failed single-page scan may have no complete TIFF. */
    if (!stack && access(pages[0],R_OK)) return incomplete?0:1;
    int n=snprintf(output,sizeof(output),"%s/%s",scan_directory,incomplete?"scan-partial.pdf":"scan.pdf");
    if (n<0 || (size_t)n>=sizeof(output)) return 1;
    return pdf_from_tiffs(output,pages,count);
}
