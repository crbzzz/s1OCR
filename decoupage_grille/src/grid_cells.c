#include "grid_splitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char *prendre_case(const unsigned char *img, int larg_img, int x0, int y0, int x1, int y1, int *ret_larg, int *ret_haut)
{
    if (x1 <= x0 || y1 <= y0) {
        return NULL;
    }

    int w = x1 - x0;
    int h = y1 - y0;
    unsigned char *res = (unsigned char *)malloc((size_t)w * (size_t)h);
    if (!res) {
        return NULL;
    }

    for (int y = 0; y < h; ++y) {
        memcpy(res + (size_t)y * (size_t)w,
               img + (size_t)(y0 + y) * (size_t)larg_img + (size_t)x0,
               (size_t)w);
    }

    if (ret_larg) {
        *ret_larg = w;
    }
    if (ret_haut) {
        *ret_haut = h;
    }
    return res;
}

int sauver_case(const char *dossier, size_t col, size_t lig, const unsigned char *pix, int larg, int haut, int stride)
{
    char path[512];
    int n = snprintf(path, sizeof(path), "%s/%zu,%zu.png", dossier, col, lig);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        fprintf(stderr, "Chemin trop long pour %zu,%zu\n", col, lig);
        return -1;
    }
    if (ecrire_png(path, larg, haut, 1, pix, stride) != 0) {
        fprintf(stderr, "Echec ecriture %s\n", path);
        return -1;
    }
    return 0;
}

int couper_cases(const unsigned char *img,
                 int larg,
                 int haut,
                 const Bande *band_h,
                 size_t nb_h,
                 const Bande *band_v,
                 size_t nb_v,
                 int marge,
                 const char *dossier,
                 double *tab_larg,
                 double *tab_haut,
                 size_t max_cases,
                 size_t *nb_sortie)
{
    if (!band_h || !band_v || nb_h < 2 || nb_v < 2) {
        return -1;
    }

    size_t nb_lig = nb_h - 1;
    size_t nb_col = nb_v - 1;
    int sauves = 0;
    size_t idx = 0;

    for (size_t r = 0; r < nb_lig; ++r) {
        int top = band_h[r].fin + marge;
        int bottom = band_h[r + 1].deb - marge;
        if (bottom <= top || top < 0 || bottom > haut) {
            continue;
        }

        for (size_t c = 0; c < nb_col; ++c) {
            int left = band_v[c].fin + marge;
            int right = band_v[c + 1].deb - marge;
            if (right <= left || left < 0 || right > larg) {
                continue;
            }

            int w = 0;
            int h = 0;
            unsigned char *cell = prendre_case(img, larg, left, top, right, bottom, &w, &h);
            if (!cell) {
                continue;
            }

            if (sauver_case(dossier, c, r, cell, w, h, w) == 0) {
                sauves++;
            }
            if (tab_larg && tab_haut && nb_sortie && idx < max_cases) {
                tab_larg[idx] = (double)w;
                tab_haut[idx] = (double)h;
                idx++;
            }
            free(cell);
        }
    }

    if (nb_sortie) {
        *nb_sortie = idx;
    }
    return sauves;
}

int cases_uniformes(const double *tab_larg, const double *tab_haut, size_t nb)
{
    if (!tab_larg || !tab_haut || nb == 0) {
        return 1;
    }

    double min_w = tab_larg[0];
    double max_w = tab_larg[0];
    double min_h = tab_haut[0];
    double max_h = tab_haut[0];
    double min_area = tab_larg[0] * tab_haut[0];
    double max_area = min_area;

    for (size_t i = 1; i < nb; ++i) {
        double w = tab_larg[i];
        double h = tab_haut[i];
        double area = w * h;
        if (w <= 0.0 || h <= 0.0) {
            return 0;
        }
        if (w < min_w) {
            min_w = w;
        }
        if (w > max_w) {
            max_w = w;
        }
        if (h < min_h) {
            min_h = h;
        }
        if (h > max_h) {
            max_h = h;
        }
        if (area < min_area) {
            min_area = area;
        }
        if (area > max_area) {
            max_area = area;
        }
    }

    double ratio_w = max_w / min_w;
    double ratio_h = max_h / min_h;
    double ratio_area = max_area / min_area;

    const double seuil_w = 1.5;
    const double seuil_h = 1.5;
    const double seuil_area = 2.2;

    if (ratio_w > seuil_w) {
        return 0;
    }
    if (ratio_h > seuil_h) {
        return 0;
    }
    if (ratio_area > seuil_area) {
        return 0;
    }
    return 1;
}
