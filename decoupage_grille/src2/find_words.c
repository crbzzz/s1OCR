#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct {
    int x0, y0, x1, y1;
    double xc, yc;
} Box;

int main(int argc, char **argv)
{
    if (argc < 3) return 1;
    const char *in = argv[1];
    const char *out = argv[2];

    int W, H, C;
    unsigned char *pix = stbi_load(in, &W, &H, &C, 1);
    if (!pix) return 1;

    unsigned char *vis = calloc(W*H,1);
    int *stack = malloc(W*H*sizeof(int));
    Box *L = malloc(sizeof(Box)*5000);
    int nb = 0;

    for (int y=0;y<H;y++)
    for (int x=0;x<W;x++){
        int id=y*W+x;
        if (vis[id] || pix[id]>200) continue;

        int x0=x,x1=x,y0=y,y1=y;
        int sp=0;
        vis[id]=1;
        stack[sp++]=id;

        while (sp){
            int p=stack[--sp];
            int py=p/W, px=p%W;

            if(px<x0)x0=px;
            if(px>x1)x1=px;
            if(py<y0)y0=py;
            if(py>y1)y1=py;

            static int V[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
            for(int k=0;k<4;k++){
                int nx=px+V[k][0];
                int ny=py+V[k][1];
                if(nx<0||ny<0||nx>=W||ny>=H) continue;
                int nid=ny*W+nx;
                if(!vis[nid] && pix[nid]<200){
                    vis[nid]=1;
                    stack[sp++]=nid;
                }
            }
        }

        int w=x1-x0+1, h=y1-y0+1;
        if (w<5 || h<5) continue;

        L[nb].x0=x0;
        L[nb].x1=x1;
        L[nb].y0=y0;
        L[nb].y1=y1;
        L[nb].xc=(x0+x1)/2.0;
        L[nb].yc=(y0+y1)/2.0;
        nb++;
    }

    free(vis);
    free(stack);

    for(int i=0;i<nb;i++)
    for(int j=i+1;j<nb;j++)
        if(L[j].yc < L[i].yc){
            Box t=L[i]; L[i]=L[j]; L[j]=t;
        }

    int *start = malloc(nb*sizeof(int));
    int *count = malloc(nb*sizeof(int));
    int NL=0;
    int i=0;

    while(i<nb){
        double y=L[i].yc;
        int j=i+1;
        while(j<nb && fabs(L[j].yc - y) < 15) j++;
        start[NL]=i;
        count[NL]=j-i;
        NL++;
        i=j;
    }

    char cmd[256];
    snprintf(cmd,sizeof(cmd),"mkdir -p %s",out);
    system(cmd);

    int word_id=0;

    for(int r=0;r<NL;r++){
        int st=start[r];
        int n=count[r];

        for(int a=0;a<n;a++)
        for(int b=a+1;b<n;b++)
            if(L[st+b].x0<L[st+a].x0){
                Box t=L[st+a]; L[st+a]=L[st+b]; L[st+b]=t;
            }

        for(int c=1;c<n;c++){
            double d = L[st+c].x0 - L[st+c-1].x1;
            L[st+c].yc = d;
        }

        for(int a=0; a<n;){
            int b = a+1;
            while(b<n && L[st+b].yc < 40) b++;

            int x0 = L[st+a].x0;
            int x1 = L[st+b-1].x1;
            int y0 = L[st+a].y0;
            int y1 = L[st+a].y1;

            for(int k=a; k<b; k++){
                if(L[st+k].y0 < y0) y0 = L[st+k].y0;
                if(L[st+k].y1 > y1) y1 = L[st+k].y1;
            }

            int w = x1 - x0 + 1;
            int h = y1 - y0 + 1;

            unsigned char *cut = malloc(w*h);
            for(int yy=0; yy<h; yy++)
                memcpy(cut + yy*w, pix + (y0+yy)*W + x0, w);

            char fn[256];
            snprintf(fn,sizeof(fn),"%s/word_%d.png",out,word_id++);
            stbi_write_png(fn,w,h,1,cut,w);

            free(cut);
            a = b;
        }

    }

    free(L);
    free(start);
    free(count);
    stbi_image_free(pix);
    return 0;
}
