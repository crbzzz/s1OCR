#include "grid_splitter.h"
#include "grid_composants_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bloc_tab_libere(BlocTab *tab)
{
    if (tab == NULL) {
        return;
    }
    free(tab->tab);
    tab->tab = NULL;
    tab->nb = 0;
    tab->cap = 0;
}

int bloc_tab_ajoute(BlocTab *tab, Bloc bloc)
{
    if (tab == NULL) {
        return -1;
    }

    if (tab->nb == tab->cap) {
        size_t nouvelle_capacite = tab->cap ? tab->cap * 2 : 16;
        Bloc *nouveau = (Bloc *)realloc(tab->tab, nouvelle_capacite * sizeof(Bloc));
        if (nouveau == NULL) {
            return -1;
        }
        tab->tab = nouveau;
        tab->cap = nouvelle_capacite;
    }

    tab->tab[tab->nb++] = bloc;
    return 0;
}

int tri_bloc_y(const void *a, const void *b)
{
    const Bloc *ba = (const Bloc *)a;
    const Bloc *bb = (const Bloc *)b;

    if (ba->y_min != bb->y_min) {
        return ba->y_min - bb->y_min;
    }
    return ba->x_min - bb->x_min;
}

int tri_bloc_x(const void *a, const void *b)
{
    const Bloc *ba = (const Bloc *)a;
    const Bloc *bb = (const Bloc *)b;

    if (ba->x_min != bb->x_min) {
        return ba->x_min - bb->x_min;
    }
    return ba->y_min - bb->y_min;
}

int tri_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    if (da < db) {
        return -1;
    }
    if (da > db) {
        return 1;
    }
    return 0;
}

double copie_mediane(const double *vals, size_t nb)
{
    if (nb == 0) {
        return 0.0;
    }

    double *tmp = (double *)malloc(nb * sizeof(double));
    if (!tmp) {
        return vals[0];
    }

    memcpy(tmp, vals, nb * sizeof(double));
    qsort(tmp, nb, sizeof(double), tri_double);

    double med;
    if (nb % 2 == 1) {
        med = tmp[nb / 2];
    } else {
        med = 0.5 * (tmp[nb / 2 - 1] + tmp[nb / 2]);
    }

    free(tmp);
    return med;
}

int remplis_blc(const unsigned char *pix,
                       int larg,int haut,
                       unsigned char *vu,
                       int sx, int sy,
                       int seuil, Bloc *res,int *pile,
                       size_t taille_pile)
{
    size_t top = 0;
    size_t idx = (size_t)sy * (size_t)larg + (size_t)sx;
    pile[top++] = (int)idx;

    int x_min = sx;
    int x_max = sx;
    int y_min = sy;
    int y_max = sy;
    int aire = 0;

    while (top > 0) {
        idx = (size_t)pile[--top];
        int x = (int)(idx % (size_t)larg);
        int y = (int)(idx / (size_t)larg);

        if (vu[idx]) {
            continue;
        }
        vu[idx] = 1;
        aire++;

        if (x < x_min) {
            x_min = x;
        }
        if (x > x_max) {
            x_max = x;
        }
        if (y < y_min) {
            y_min = y;
        }
        if (y > y_max) {
            y_max = y;
        }

        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};

        for (size_t i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (nx < 0 || nx >= larg || ny < 0 || ny >= haut) {
                continue;
            }

            size_t nidx = (size_t)ny * (size_t)larg + (size_t)nx;
            if (vu[nidx]) {
                continue;
            }

            if (pix[nidx] < seuil) {
                if (top >= taille_pile) {
                    return -1;
                }
                pile[top++] = (int)nidx;
            }
        }
    }

    res->x_min = x_min;
    res->x_max = x_max;
    res->y_min = y_min;
    res->y_max = y_max;
    res->aire = aire;
    return 0;
}

int trouve_blocs(const unsigned char *pix, int larg, int haut, BlocTab *tab)
{
    const int seuil = 128;
    unsigned char *vu = (unsigned char *)calloc((size_t)larg * (size_t)haut, sizeof(unsigned char));
    if (!vu) {
        fprintf(stderr, "Echec allocation memoire\n");
        return -1;
    }

    size_t taille_pile = (size_t)larg * (size_t)haut;
    int *pile = (int *)malloc(taille_pile * sizeof(int));
    if (!pile) {
        free(vu);
        fprintf(stderr, "Echec allocation memoire\n");
        return -1;
    }

    for (int y = 0; y < haut; ++y) {
        for (int x = 0; x < larg; ++x) {
            size_t idx = (size_t)y * (size_t)larg + (size_t)x;
            if (vu[idx]) {
                continue;
            }
            if (pix[idx] >= seuil) {
                vu[idx] = 1;
                continue;
            }

            Bloc bloc = {x, y, x, y, 0};
            if (remplis_blc(pix, larg, haut, vu, x, y, seuil, &bloc, pile, taille_pile) != 0) {
                free(pile);
                free(vu);
                fprintf(stderr, "Erreur.\n");
                return -1;
            }

            int largeur = bloc.x_max - bloc.x_min + 1;
            int hauteur = bloc.y_max - bloc.y_min + 1;
            if (largeur <= 0 || hauteur <= 0) {
                continue;
            }

            if (bloc_tab_ajoute(tab, bloc) != 0) {
                free(pile);
                free(vu);
                fprintf(stderr, "Erreur.\n");
                return -1;
            }
        }
    }

    free(pile);
    free(vu);
    return 0;
}

int ajoute_lot(LotLig **lots, size_t *nb, size_t *cap, size_t debut, size_t nb_blocs)
{
    if (*nb == *cap) {
        size_t ncap = (*cap == 0) ? 8 : (*cap * 2);
        LotLig *tmp = (LotLig *)realloc(*lots, ncap * sizeof(LotLig));
        if (!tmp) {
            return -1;
        }
        *lots = tmp;
        *cap = ncap;
    }

    (*lots)[*nb].debut = debut;
    (*lots)[*nb].nb = nb_blocs;
    (*nb)++;
    return 0;
}

int grille_depuis_blocs(const unsigned char *pix,
                        int larg,int haut,
                        const char *dossier,
                        const BlocTab *tab,
                        const LotLig *lots,
                        size_t nb_lots)
{
    if (!tab || !lots) {
        return -1;
    }

    if (tab->nb == 0 || nb_lots == 0) {
        return -1;
    }

    size_t nb_lignes = nb_lots;
    size_t nb_cols = 0;
    for (size_t i = 0; i < nb_lots; ++i) {
        if (lots[i].nb > nb_cols) {
            nb_cols = lots[i].nb;
        }
    }

    if (nb_cols == 0) {
        return -1;
    }

    double *largeurs = (double *)malloc(nb_cols * sizeof(double));
    double *hauteurs = (double *)malloc(nb_lignes * sizeof(double));
    if (!largeurs || !hauteurs) {
        free(largeurs);
        free(hauteurs);
        return -1;
    }

    for (size_t c = 0; c < nb_cols; ++c) {
        largeurs[c] = 0.0;
    }
    for (size_t r = 0; r < nb_lignes; ++r) {
        hauteurs[r] = 0.0;
    }

    for (size_t r = 0; r < nb_lignes; ++r) {
        LotLig lot = lots[r];
        for (size_t c = 0; c < lot.nb; ++c) {
            const Bloc *bloc = &tab->tab[lot.debut + c];
            int largeur = bloc->x_max - bloc->x_min + 1;
            int hauteur = bloc->y_max - bloc->y_min + 1;
            if (c < nb_cols) {
                largeurs[c] += (double)largeur;
            }
            hauteurs[r] += (double)hauteur;
        }
    }

    for (size_t c = 0; c < nb_cols; ++c) {
        largeurs[c] /= (double)nb_lignes;
    }
    for (size_t r = 0; r < nb_lignes; ++r) {
        LotLig lot = lots[r];
        if (lot.nb > 0) {
            hauteurs[r] /= (double)lot.nb;
        }
    }

    double med_larg = copie_mediane(largeurs, nb_cols);
    double med_haut = copie_mediane(hauteurs, nb_lignes);

    if (med_larg < 1.0) {
        med_larg = 1.0;
    }
    if (med_haut < 1.0) {
        med_haut = 1.0;
    }

    const double tol = 0.4;
    size_t sauv = 0;

    for (size_t r = 0; r < nb_lignes; ++r) {
        LotLig lot = lots[r];
        for (size_t c = 0; c < lot.nb; ++c) {
            const Bloc *bloc = &tab->tab[lot.debut + c];
            int largeur = bloc->x_max - bloc->x_min + 1;
            int hauteur = bloc->y_max - bloc->y_min + 1;

            double ratio_l = (double)largeur / med_larg;
            double ratio_h = (double)hauteur / med_haut;
            if (ratio_l < (1.0 - tol) || ratio_l > (1.0 + tol)) {
                continue;
            }
            if (ratio_h < (1.0 - tol) || ratio_h > (1.0 + tol)) {
                continue;
            }

            int w = 0;
            int h = 0;
            unsigned char *case_pix = prendre_case(pix, larg, bloc->x_min, bloc->y_min, bloc->x_max + 1, bloc->y_max + 1, &w, &h);
            if (!case_pix) {
                continue;
            }
            if (sauver_case(dossier, c, r, case_pix, w, h, w) == 0) {
                sauv++;
            }
            free(case_pix);
        }
    }

    free(largeurs);
    free(hauteurs);

    return (sauv > 0) ? 0 : -1;
}
