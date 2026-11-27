/* Conservative document-only finishing. No resampling/cropping; raw USB data
 * remains untouched. Both operations are independently optional. */
static unsigned cleanup_light(const unsigned char *p,unsigned channels) {
    return channels==1?p[0]:((unsigned)p[0]+p[1]+p[2])/3;
}
static unsigned cleanup_span(const unsigned char *p,unsigned channels) {
    if (channels==1) return 0;
    unsigned lo=p[0],hi=p[0];
    for (unsigned c=1;c<3;c++) { if (p[c]<lo) lo=p[c]; if (p[c]>hi) hi=p[c]; }
    return hi-lo;
}
/* Detect a concentrated, neutral, bright paper population. Broad gradients,
 * dark backgrounds and saturated colors do not establish a white point. */
static unsigned cleanup_whitepoint(const unsigned char *pixels,unsigned width,unsigned height,unsigned channels,unsigned dpi) {
    unsigned hist[256]={0},total=0,neutral=0;
    unsigned margin=(dpi*5+24)/25; /* approx5mm; no edge glass in histogram */
    if (width<=2*margin || height<=2*margin) return 0;
    unsigned step=(width>height?width:height)/512; if (!step) step=1;
    for (unsigned y=margin;y<height-margin;y+=step) for (unsigned x=margin;x<width-margin;x+=step) {
        const unsigned char *p=pixels+((size_t)y*width+x)*channels;total++;
        unsigned light=cleanup_light(p,channels);
        if (light>=180 && cleanup_span(p,channels)<=20) {hist[light]++;neutral++;}
    }
    if (!total || neutral<total/4) return 0;
    unsigned peak=0,best=0;
    for (unsigned v=200;v<=255;v++) {
        unsigned count=0;
        for (unsigned j=v-2;j<=v+2 && j<256;j++) count+=hist[j];
        if (count>best) { best=count;peak=v; }
    }
    if (peak>250 || best*100<total*15) return 0;
    return peak-8;
}
static size_t cleanup_edge_index(unsigned side,unsigned line,unsigned depth,unsigned width,unsigned height) {
    if (side==0) return (size_t)line*width+depth;
    if (side==1) return (size_t)line*width+(width-1-depth);
    if (side==2) return (size_t)depth*width+line;
    return (size_t)(height-1-depth)*width+line;
}
/* Only edge-connected dark runs followed by stable light paper qualify. */
static unsigned cleanup_edge_depth(const unsigned char *pixels,unsigned side,unsigned line,
    unsigned width,unsigned height,unsigned channels,unsigned limit,unsigned stable) {
    const unsigned char *first=pixels+cleanup_edge_index(side,line,0,width,height)*channels;
    if (cleanup_light(first,channels)>=100 || cleanup_span(first,channels)>30) return 0;
    unsigned run=0;
    for (unsigned d=1;d<limit+stable;d++) {
        const unsigned char *p=pixels+cleanup_edge_index(side,line,d,width,height)*channels;
        if (cleanup_light(p,channels)>=180 && cleanup_span(p,channels)<=30) run++;
        else run=0;
        if (run==stable) {
            unsigned depth=d+1-stable;
            return depth<=limit?depth:0;
        }
    }
    return 0;
}
static void cleanup_edges(unsigned char *pixels,unsigned width,unsigned height,unsigned channels,unsigned dpi) {
    unsigned cap=(dpi*6+24)/25,stable=(dpi+49)/50,guard=(dpi+149)/150;
    for (unsigned side=0;side<4;side++) {
        unsigned length=side<2?height:width,depth=side<2?width:height;
        if (depth<=cap+stable || length<20) continue;
        unsigned first=0,last=length,count=0,maximum=0,run=0,longest=0;
        for (unsigned line=first;line<last;line++) {
            unsigned n=cleanup_edge_depth(pixels,side,line,width,height,channels,cap,stable);
            if (n) { count++;if (n>maximum) maximum=n;run++;if (run>longest) longest=run; }
            else run=0;
        }
        /* Long consistent strips; or very thin strips across>=10% of edge.
         * Isolated edge text is not enough. Thick dark artwork is rejected. */
        unsigned total=last-first;
        if (longest*100<total*10) continue; /* Reject scattered texture/edge text. */
        if (count*100<total*60 && !(count*100>=total*10 && maximum<=(2*dpi+24)/25)) continue;
        unsigned changed=0;
        for (unsigned line=0;line<length;line++) {
            unsigned n=cleanup_edge_depth(pixels,side,line,width,height,channels,cap,stable);
            if (!n) continue;
            n+=guard;if (n>cap) n=cap;
            for (unsigned d=0;d<n;d++) memset(pixels+cleanup_edge_index(side,line,d,width,height)*channels,255,channels);
            changed++;
        }
        printf("EDGE CLEANUP side=%u lines=%u maximum=%u pixels\n",side,changed,maximum);
    }
}
static void cleanup_apply(unsigned char *pixels,unsigned width,unsigned height,unsigned channels,unsigned dpi,int paper,int edges) {
    if (!pixels || !width || !height || (channels!=1 && channels!=3) || !dpi) return;
    unsigned white=paper?cleanup_whitepoint(pixels,width,height,channels,dpi):0;
    if (edges) cleanup_edges(pixels,width,height,channels,dpi);
    if (!white) return;
    unsigned char lut[256];
    for (unsigned v=0;v<256;v++) lut[v]=v<=180?(unsigned char)v:v>=white?255:
        (unsigned char)(180+(v-180)*75/(white-180));
    size_t count=(size_t)width*height;
    for (size_t i=0;i<count;i++) {
        unsigned char *p=pixels+i*channels;
        /* Preserve saturated colors. Only neutral highlights are adjusted. */
        if (cleanup_span(p,channels)>24) continue;
        for (unsigned c=0;c<channels;c++) p[c]=lut[p[c]];
    }
    printf("PAPER CLEANUP whitepoint=%u; dark tones<=180 and saturated colors preserved\n",white);
}
