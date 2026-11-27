#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cleanup.h"

int main(void) {
    const unsigned dpis[]={150,300,600};
    for (unsigned d=0;d<3;d++) for (unsigned channels=1;channels<=3;channels+=2) {
        unsigned dpi=dpis[d],scale=dpi/150,w=300*scale,h=420*scale;
        size_t size=(size_t)w*h*channels;
        unsigned char *p=malloc(size),*original=malloc(size);assert(p && original);
        /* Gray paper, slanted left/right edges and thin top/bottom strips. */
        for (unsigned y=0;y<h;y++) for (unsigned x=0;x<w;x++) {
            unsigned edge=(20*scale+y*10*scale/h);
            unsigned value=x<edge || x>=w-4*scale || y<2*scale || y>=h-2*scale?0:234+(x+y)%5;
            memset(p+((size_t)y*w+x)*channels,value,channels);
        }
        /* Black text, pale pencil-like gray and color patches within paper. */
        for (unsigned y=100*scale;y<120*scale;y++) for (unsigned x=80*scale;x<120*scale;x++)
            memset(p+((size_t)y*w+x)*channels,x<100*scale?20:170,channels);
        if (channels==3) for (unsigned y=200*scale;y<220*scale;y++) for (unsigned x=80*scale;x<120*scale;x++) {
            unsigned char *v=p+((size_t)y*w+x)*channels;v[0]=220;v[1]=100;v[2]=90;
        }
        memcpy(original,p,size);
        cleanup_apply(p,w,h,channels,dpi,0,0);assert(!memcmp(p,original,size));
        cleanup_apply(p,w,h,channels,dpi,1,1);
        for (unsigned y=20*scale;y<h-20*scale;y++) for (unsigned x=0;x<36*scale;x++)
            for (unsigned c=0;c<channels;c++) assert(p[((size_t)y*w+x)*channels+c]==255);
        for (unsigned x=40*scale;x<w-20*scale;x++) for (unsigned y=0;y<2*scale;y++)
            assert(p[((size_t)y*w+x)*channels]==255);
        assert(p[((size_t)110*scale*w+90*scale)*channels]==20);
        assert(p[((size_t)110*scale*w+110*scale)*channels]==170);
        if (channels==3) assert(!memcmp(p+((size_t)210*scale*w+90*scale)*3,original+((size_t)210*scale*w+90*scale)*3,3));
        /* Independent controls: whitening leaves black edge; edge-only leaves
         * paper's original intensity. The original input stays untouched. */
        memcpy(p,original,size);cleanup_apply(p,w,h,channels,dpi,1,0);
        assert(p[(size_t)100*scale*w*channels]==0);
        memcpy(p,original,size);cleanup_apply(p,w,h,channels,dpi,0,1);
        size_t paper=((size_t)50*scale*w+150*scale)*channels;
        assert(p[paper]==original[paper]);
        /* Thick dark artwork and full black pages do not qualify as borders. */
        memset(p,230,size);
        for (unsigned y=0;y<h;y++) memset(p+(size_t)y*w*channels,0,60*scale*channels);
        memcpy(original,p,size);cleanup_apply(p,w,h,channels,dpi,0,1);assert(!memcmp(p,original,size));
        memset(p,0,size);cleanup_apply(p,w,h,channels,dpi,1,1);
        for (size_t i=0;i<size;i++) assert(p[i]==0);
        /* Small isolated edge marks survive; already-white paper is unchanged. */
        memset(p,255,size);
        for (unsigned y=h/2;y<h/2+3*scale;y++) memset(p+(size_t)y*w*channels,0,4*scale*channels);
        memcpy(original,p,size);cleanup_apply(p,w,h,channels,dpi,1,1);assert(!memcmp(p,original,size));
        /* Broad gradients are not interpreted as uniform white paper. */
        for (unsigned y=0;y<h;y++) for (unsigned x=0;x<w;x++) memset(p+((size_t)y*w+x)*channels,180+x*75/w,channels);
        memcpy(original,p,size);cleanup_apply(p,w,h,channels,dpi,1,0);assert(!memcmp(p,original,size));
        /* Periodic high-contrast texture at page boundaries is not a coherent
         * scanner strip, even when many short dark runs touch the edge. */
        for (size_t i=0;i<size;i++) p[i]=(unsigned char)(i%251);
        memcpy(original,p,size);cleanup_apply(p,w,h,channels,dpi,1,1);assert(!memcmp(p,original,size));
        free(original);free(p);printf("CLEANUP TEST dpi=%u channels=%u PASSED\n",dpi,channels);
    }
    unsigned char tiny[3]={23,24,25};cleanup_apply(tiny,1,1,3,150,1,1);assert(tiny[0]==23 && tiny[2]==25);
    puts("Cleanup tests passed: edges,neutral paper,dark text,color patches,independent controls,photos/gradient rejection,tiny images.");
    return 0;
}
