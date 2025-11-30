#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct {
    int x0, y0, x1, y1;
    double xc, yc;
} Box;

static int cmp_int(const void *a, const void *b)
{
    int aa = *(const int *)a;
    int bb = *(const int *)b;
    return (aa > bb) - (aa < bb);
}

static double median_int(const int *v, int n)
{
    if (n <= 0) return 0.0;
    int *tmp = malloc(sizeof(int) * n);
    if (!tmp) return 0.0;
    memcpy(tmp, v, sizeof(int) * n);
    qsort(tmp, n, sizeof(int), cmp_int);
    double m = (n % 2) ? tmp[n/2] : 0.5 * (tmp[n/2 - 1] + tmp[n/2]);
    free(tmp);
    return m;
}

static void ensure_dir(const char *p)
{
    if (!p || !p[0]) return;
    MKDIR(p);
}

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
    if (nb == 0) {
        free(L);
        return 0;
    }

    int *heights = malloc(nb * sizeof(int));
    int *widths  = malloc(nb * sizeof(int));
    int *areas   = malloc(nb * sizeof(int));
    if (!heights || !widths || !areas) {
        free(L); free(heights); free(widths); free(areas);
        return 1;
    }

    int max_area = 0;
    int max_idx = 0;
    for (int i=0;i<nb;i++){
        int w = L[i].x1 - L[i].x0 + 1;
        int h = L[i].y1 - L[i].y0 + 1;
        int a = w * h;
        widths[i] = w;
        heights[i] = h;
        areas[i] = a;
        if (a > max_area) { max_area = a; max_idx = i; }
    }

    double med_h = median_int(heights, nb);
    double med_w = median_int(widths, nb);
    double med_area = median_int(areas, nb);

    Box grid_box = L[max_idx];
    double grid_area = (grid_box.x1 - grid_box.x0 + 1) * (grid_box.y1 - grid_box.y0 + 1);
    int keep_count = 0;
    for (int i=0;i<nb;i++){
        int w = widths[i];
        int h = heights[i];
        int a = areas[i];

        if (h < med_h * 0.5 || h > med_h * 2.2) continue;
        if (w < med_w * 0.35 || w > med_w * 3.5) continue;
        if (a > med_area * 8.0) continue;

        if (grid_area > med_area * 8.0) {
            int ix0 = (L[i].x0 > grid_box.x0) ? L[i].x0 : grid_box.x0;
            int iy0 = (L[i].y0 > grid_box.y0) ? L[i].y0 : grid_box.y0;
            int ix1 = (L[i].x1 < grid_box.x1) ? L[i].x1 : grid_box.x1;
            int iy1 = (L[i].y1 < grid_box.y1) ? L[i].y1 : grid_box.y1;
            int iw = ix1 - ix0 + 1;
            int ih = iy1 - iy0 + 1;
            if (iw > 0 && ih > 0) {
                int inter = iw * ih;
                if (inter > a * 0.25) continue;
            }
        }

        L[keep_count++] = L[i];
    }

    free(heights);
    free(widths);
    free(areas);

    nb = keep_count;
    if (nb == 0) {
        free(L);
        return 0;
    }

    for(int i=0;i<nb;i++)
    for(int j=i+1;j<nb;j++)
        if(L[j].yc < L[i].yc){
            Box t=L[i]; L[i]=L[j]; L[j]=t;
        }

    int *start = malloc(nb*sizeof(int));
    int *count = malloc(nb*sizeof(int));
    int NL=0;
    int i=0;
    double line_th = med_h * 0.65;
    if (line_th < 8.0) line_th = 8.0;

    while(i<nb){
        double y=L[i].yc;
        int j=i+1;
        while(j<nb && fabs(L[j].yc - y) < line_th) j++;
        start[NL]=i;
        count[NL]=j-i;
        NL++;
        i=j;
    }

    ensure_dir(out);
    char cmd[256];
#ifdef _WIN32
    snprintf(cmd,sizeof(cmd),"if not exist \"%s\" mkdir \"%s\"",out,out);
#else
    snprintf(cmd,sizeof(cmd),"mkdir -p \"%s\"",out);
#endif
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

        int gap_count = (n>1)? n-1 : 0;
        int *gaps = gap_count? malloc(sizeof(int)*gap_count) : NULL;
        if (gaps){
            for(int c=1;c<n;c++){
                gaps[c-1] = L[st+c].x0 - L[st+c-1].x1;
            }
        }
        double gap_med = gap_count? median_int(gaps,gap_count) : med_w * 0.8;
        if (gaps) free(gaps);
        double split_gap = gap_med * 1.8;
        double min_split = med_w * 1.2;
        if (split_gap < min_split) split_gap = min_split;

        for(int c=1;c<n;c++){
            double d = L[st+c].x0 - L[st+c-1].x1;
            L[st+c].yc = d;
        }

        for(int a=0; a<n;){
            int b = a+1;
            while(b<n && (L[st+b].x0 - L[st+b-1].x1) < split_gap) b++;

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
