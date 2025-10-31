#include "grid_splitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <strings.h>
#endif

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
                 int larg,int haut,
                 const Bande *band_h, size_t nb_h,
                 const Bande *band_v,
                 size_t nb_v, int marge,
                 const char *dossier,double *tab_larg,
                 double *tab_haut, size_t max_cases,
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

typedef struct {
    char nom[512];
} FichierPNG;

static void libere_liste_png(FichierPNG *liste)
{
    free(liste);
}

static int ajoute_png(FichierPNG **liste, size_t *nb, size_t *cap, const char *nom)
{
    if (*nb == *cap) {
        size_t nouvelle_capacite = (*cap == 0) ? 32 : *cap * 2;
        FichierPNG *tmp = (FichierPNG *)realloc(*liste, nouvelle_capacite * sizeof(FichierPNG));
        if (!tmp) {
            return -1;
        }
        *liste = tmp;
        *cap = nouvelle_capacite;
    }
    strncpy((*liste)[*nb].nom, nom, sizeof((*liste)[*nb].nom) - 1);
    (*liste)[*nb].nom[sizeof((*liste)[*nb].nom) - 1] = '\0';
    (*nb)++;
    return 0;
}

static int collecte_png(const char *dossier, FichierPNG **liste, size_t *nb)
{
    size_t cap = 0;
    *liste = NULL;
    *nb = 0;

#ifdef _WIN32
    char pattern[1024];
    int n = snprintf(pattern, sizeof(pattern), "%s\\*.png", dossier);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        return -1;
    }

    struct _finddata_t data;
    intptr_t handle = _findfirst(pattern, &data);
    if (handle == -1) {
        return 0;
    }

    do {
        if (data.attrib & _A_SUBDIR) {
            continue;
        }
        if (ajoute_png(liste, nb, &cap, data.name) != 0) {
            _findclose(handle);
            libere_liste_png(*liste);
            *liste = NULL;
            *nb = 0;
            return -1;
        }
    } while (_findnext(handle, &data) == 0);
    _findclose(handle);
#else
    DIR *dir = opendir(dossier);
    if (!dir) {
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        #ifdef DT_DIR
        if (entry->d_type == DT_DIR) {
            continue;
        }
        #endif
        const char *nom = entry->d_name;
        size_t len = strlen(nom);
        if (len < 4) {
            continue;
        }
        const char *ext = nom + len - 4;
        if (strcasecmp(ext, ".png") != 0) {
            continue;
        }
        if (ajoute_png(liste, nb, &cap, nom) != 0) {
            closedir(dir);
            libere_liste_png(*liste);
            *liste = NULL;
            *nb = 0;
            return -1;
        }
    }
    closedir(dir);
#endif
    return 0;
}

static int largeur_max(const char *chemin, int *larg, int *haut)
{
    int w = 0;
    int h = 0;
    int canaux = 0;
    unsigned char *img = charger_image(chemin, &w, &h, &canaux);
    if (!img) {
        return -1;
    }
    liberer_image(img);
    if (larg) {
        *larg = w;
    }
    if (haut) {
        *haut = h;
    }
    return 0;
}

static int padding_image(const char *chemin, int cible_larg, int cible_haut)
{
    int w = 0;
    int h = 0;
    int canaux = 0;
    unsigned char *src = charger_image(chemin, &w, &h, &canaux);
    if (!src) {
        return -1;
    }

    if (w == cible_larg && h == cible_haut) {
        liberer_image(src);
        return 0;
    }

    size_t total = (size_t)cible_larg * (size_t)cible_haut;
    unsigned char *dst = (unsigned char *)malloc(total);
    if (!dst) {
        liberer_image(src);
        return -1;
    }
    memset(dst, 255, total);

    if (w > cible_larg) {
        w = cible_larg;
    }
    if (h > cible_haut) {
        h = cible_haut;
    }

    int offset_x = (cible_larg - w) / 2;
    int offset_y = (cible_haut - h) / 2;
    if (offset_x < 0) {
        offset_x = 0;
    }
    if (offset_y < 0) {
        offset_y = 0;
    }

    for (int y = 0; y < h; ++y) {
        memcpy(dst + (size_t)(offset_y + y) * (size_t)cible_larg + (size_t)offset_x,
               src + (size_t)y * (size_t)w,
               (size_t)w);
    }

    int rc = ecrire_png(chemin, cible_larg, cible_haut, 1, dst, cible_larg);
    free(dst);
    liberer_image(src);
    return rc;
}

int uniformiser_cases(const char *dossier)
{
    if (!dossier || !*dossier) {
        return -1;
    }

    FichierPNG *liste = NULL;
    size_t nb = 0;
    if (collecte_png(dossier, &liste, &nb) != 0) {
        return -1;
    }
    if (nb == 0) {
        libere_liste_png(liste);
        return 0;
    }

    int max_larg = 0;
    int max_haut = 0;
    for (size_t i = 0; i < nb; ++i) {
        char chemin[1024];
#ifdef _WIN32
        int n = snprintf(chemin, sizeof(chemin), "%s\\%s", dossier, liste[i].nom);
#else
        int n = snprintf(chemin, sizeof(chemin), "%s/%s", dossier, liste[i].nom);
#endif
        if (n < 0 || (size_t)n >= sizeof(chemin)) {
            continue;
        }
        int w = 0;
        int h = 0;
        if (largeur_max(chemin, &w, &h) != 0) {
            continue;
        }
        if (w > max_larg) {
            max_larg = w;
        }
        if (h > max_haut) {
            max_haut = h;
        }
    }

    if (max_larg <= 0 || max_haut <= 0) {
        libere_liste_png(liste);
        return 0;
    }

    for (size_t i = 0; i < nb; ++i) {
        char chemin[1024];
#ifdef _WIN32
        int n = snprintf(chemin, sizeof(chemin), "%s\\%s", dossier, liste[i].nom);
#else
        int n = snprintf(chemin, sizeof(chemin), "%s/%s", dossier, liste[i].nom);
#endif
        if (n < 0 || (size_t)n >= sizeof(chemin)) {
            continue;
        }
        padding_image(chemin, max_larg, max_haut);
    }

    libere_liste_png(liste);
    return 0;
}
