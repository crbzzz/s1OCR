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
#include "../binary/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../binary/stb_image_write.h"

typedef struct {
    int x0, y0, x1, y1;
    double yc;
} Box;

static int cmp_int(const void *a, const void *b) {
    int aa = *(const int *)a;
    int bb = *(const int *)b;
    return (aa > bb) - (aa < bb);
}

static double median_int(const int *v, int n) {
    if (n <= 0) return 0.0;
    int *tmp = malloc(sizeof(int) * n);
    if (!tmp) return 0.0;
    memcpy(tmp, v, sizeof(int) * n);
    qsort(tmp, n, sizeof(int), cmp_int);
    double m = (n % 2) ? tmp[n/2] : 0.5 * (tmp[n/2 - 1] + tmp[n/2]);
    free(tmp);
    return m;
}

static int charger(const char *path, unsigned char **pix, int *W, int *H) {
    int comp;
    *pix = stbi_load(path, W, H, &comp, 1);
    return *pix ? 0 : -1;
}

static void sauver(const char *fn, unsigned char *pix, int w, int h) {
    stbi_write_png(fn, w, h, 1, pix, w);
}

static int est_noir(unsigned char v) {
    return v < 150;
}

static void assurer_repertoire(const char *p) {
    if (!p || !p[0]) return;
    MKDIR(p);
}

int extraire_mots(const char *img_path, const char *out_dir) {

    unsigned char *pix;
    int W, H;

    if (charger(img_path, &pix, &W, &H) != 0) {
        return -1;
    }

    unsigned char *vu = calloc(W * H, 1);
    int *pile = malloc(sizeof(int) * W * H);
    Box *boxes = malloc(sizeof(Box) * (W * H) / 20);

    if (!vu || !pile || !boxes) return -1;

    int nb = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {

            int idx = y * W + x;
            if (vu[idx] || !est_noir(pix[idx])) continue;

            int x0 = x, x1 = x, y0 = y, y1 = y;

            int top = 0;
            pile[top++] = idx;
            vu[idx] = 1;

            while (top > 0) {
                int c = pile[--top];
                int cy = c / W;
                int cx = c % W;

                if (cx < x0) x0 = cx;
                if (cx > x1) x1 = cx;
                if (cy < y0) y0 = cy;
                if (cy > y1) y1 = cy;

                int v[8][2] = {
                    {1,0},{-1,0},{0,1},{0,-1},
                    {1,1},{1,-1},{-1,1},{-1,-1}
                };

                for (int k = 0; k < 8; k++) {
                    int nx = cx + v[k][0];
                    int ny = cy + v[k][1];

                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;

                    int nidx = ny * W + nx;

                    if (!vu[nidx] && est_noir(pix[nidx])) {
                        vu[nidx] = 1;
                        pile[top++] = nidx;
                    }
                }
            }

            int w = x1 - x0 + 1;
            int h = y1 - y0 + 1;

            if (w < 3 || h < 3) continue;
            
            boxes[nb++] = (Box){x0,y0,x1,y1,(y0+y1)/2.0};
        }
    }

    free(vu);
    free(pile);

    if (nb == 0) {
        free(boxes);
        return 0;
    }

    /* Ignore grid fragments and other outliers using robust statistics. */
    int *heights = malloc(nb * sizeof(int));
    int *widths  = malloc(nb * sizeof(int));
    int *areas   = malloc(nb * sizeof(int));
    if (!heights || !widths || !areas) {
        free(boxes); free(heights); free(widths); free(areas);
        return -1;
    }

    int max_area = 0;
    int max_idx = 0;
    for (int i=0;i<nb;i++){
        int w = boxes[i].x1 - boxes[i].x0 + 1;
        int h = boxes[i].y1 - boxes[i].y0 + 1;
        int a = w * h;
        widths[i] = w;
        heights[i] = h;
        areas[i] = a;
        if (a > max_area) { max_area = a; max_idx = i; }
    }

    double med_h = median_int(heights, nb);
    double med_w = median_int(widths, nb);
    double med_area = median_int(areas, nb);

    Box grid_box = boxes[max_idx];
    double grid_area = (grid_box.x1 - grid_box.x0 + 1) * (grid_box.y1 - grid_box.y0 + 1);

    int keep = 0;
    for (int i=0;i<nb;i++){
        int w = widths[i];
        int h = heights[i];
        int a = areas[i];

        if (h < med_h * 0.5 || h > med_h * 2.2) continue;
        if (w < med_w * 0.35 || w > med_w * 3.5) continue;
        if (a > med_area * 8.0) continue;

        if (grid_area > med_area * 8.0) {
            int ix0 = boxes[i].x0 > grid_box.x0 ? boxes[i].x0 : grid_box.x0;
            int iy0 = boxes[i].y0 > grid_box.y0 ? boxes[i].y0 : grid_box.y0;
            int ix1 = boxes[i].x1 < grid_box.x1 ? boxes[i].x1 : grid_box.x1;
            int iy1 = boxes[i].y1 < grid_box.y1 ? boxes[i].y1 : grid_box.y1;
            int iw = ix1 - ix0 + 1;
            int ih = iy1 - iy0 + 1;
            if (iw > 0 && ih > 0) {
                int inter = iw * ih;
                if (inter > a * 0.25) continue;
            }
        }

        boxes[keep++] = boxes[i];
    }

    free(heights);
    free(widths);
    free(areas);

    nb = keep;
    if (nb == 0) {
        free(boxes);
        return 0;
    }

    /* Recompute medians on filtered set for grouping. */
    int *h2 = malloc(nb * sizeof(int));
    int *w2 = malloc(nb * sizeof(int));
    for (int i=0;i<nb;i++){
        h2[i] = boxes[i].y1 - boxes[i].y0 + 1;
        w2[i] = boxes[i].x1 - boxes[i].x0 + 1;
    }
    med_h = median_int(h2, nb);
    med_w = median_int(w2, nb);
    free(h2);
    free(w2);

    for (int i=0;i<nb;i++){
        for (int j=i+1;j<nb;j++){
            if (boxes[j].yc < boxes[i].yc) {
                Box t = boxes[i];
                boxes[i] = boxes[j];
                boxes[j] = t;
            }
        }
    }

    int word_id = 0;
    double line_th = med_h * 0.65;
    if (line_th < 8.0) line_th = 8.0;

    assurer_repertoire(out_dir);

    for (int a = 0; a < nb; ) {

        int b = a + 1;

        while (b < nb && fabs(boxes[b].yc - boxes[a].yc) < line_th)
            b++;

        for (int i=a;i<b;i++){
            for (int j=i+1;j<b;j++){
                if (boxes[j].x0 < boxes[i].x0) {
                    Box t = boxes[i];
                    boxes[i] = boxes[j];
                    boxes[j] = t;
                }
            }
        }

        int gap_count = (b-a>1)? (b-a-1) : 0;
        int *gaps = gap_count? malloc(sizeof(int)*gap_count) : NULL;
        if (gaps){
            for (int i=a+1;i<b;i++){
                gaps[i-a-1] = boxes[i].x0 - boxes[i-1].x1;
            }
        }
        double gap_med = gap_count? median_int(gaps,gap_count) : med_w * 0.8;
        if (gaps) free(gaps);
        double split_gap = gap_med * 1.8;
        double min_split = med_w * 1.2;
        if (split_gap < min_split) split_gap = min_split;

        for (int i = a; i < b; ) {

            int j = i + 1;
            while (j < b && (boxes[j].x0 - boxes[j-1].x1) < split_gap)
                j++;

            int x0 = boxes[i].x0;
            int x1 = boxes[j-1].x1;
            int y0 = boxes[i].y0;
            int y1 = boxes[i].y1;

            for (int k=i; k<j; k++) {
                if (boxes[k].y0 < y0) y0 = boxes[k].y0;
                if (boxes[k].y1 > y1) y1 = boxes[k].y1;
            }

            int w = x1 - x0 + 1;
            int h = y1 - y0 + 1;

            unsigned char *cut = malloc(w*h);
            for (int yy = 0; yy < h; yy++)
                memcpy(cut + yy*w, pix + (y0+yy)*W + x0, w);

            char fn[256];
            snprintf(fn, sizeof(fn), "%s/word_%d.png", out_dir, word_id++);

            sauver(fn, cut, w, h);
            free(cut);

            i = j;
        }

        a = b;
    }

    free(boxes);
    stbi_image_free(pix);

    return 0;
}

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage : %s image.png dossier_sortie\n", argv[0]);
        return 1;
    }

    return extraire_mots(argv[1], argv[2]);
}
