#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../binary/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../binary/stb_image_write.h"

typedef struct {
    int x0, y0, x1, y1;
    double yc;
} Box;

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
    double seuil_ligne = 40;

    for (int a = 0; a < nb; ) {

        int b = a + 1;

        while (b < nb && fabs(boxes[b].yc - boxes[a].yc) < seuil_ligne)
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

        for (int i = a; i < b; ) {

            int j = i + 1;
            while (j < b && (boxes[j].x0 - boxes[j-1].x1) < 30)
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
