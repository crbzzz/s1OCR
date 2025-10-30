#include "grid_splitter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x_min;
    int y_min;
    int x_max;
    int y_max;

    int aire;
} Bloc;

typedef struct {
    Bloc *tab;
    size_t nb;

    size_t cap;
} BlocTab;


typedef struct {
    size_t debut;
    size_t nb;
} LotLig;

static void bloc_tab_libere(BlocTab *tab) {
    if (!tab)
        return;
    free(tab->tab);
    tab->tab = NULL;

    tab->nb = 0;
    tab->cap = 0;
}

static int bloc_tab_ajoute(BlocTab *tab, Bloc bloc) {
    if (!tab)
        return -1;

    if (tab->nb == tab->cap) {
        size_t cap_nouv = tab->cap ? tab->cap * 2 : 16;
        Bloc *tmp = (Bloc *)realloc(tab->tab, cap_nouv * sizeof(Bloc));
        if (!tmp)
            return -1;
        tab->tab = tmp;
        tab->cap = cap_nouv;
    }
    tab->tab[tab->nb++] = bloc;
    return 0;

}

static int tri_bloc_y(const void *a, const void *b) {
    const Bloc *ba = (const Bloc *)a;
    const Bloc *bb = (const Bloc *)b;
    if (ba->y_min != bb->y_min)
        return ba->y_min - bb->y_min;
    return ba->x_min - bb->x_min;
}


static int tri_bloc_x(const void *a, const void *b) {
    const Bloc *ba = (const Bloc *)a;
    const Bloc *bb = (const Bloc *)b;
    if (ba->x_min != bb->x_min)
        return ba->x_min - bb->x_min;
    return ba->y_min - bb->y_min;
}


static int tri_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;

    if (da > db) return 1;
    return 0;
}

static double copie_mediane(const double *vals, size_t nb) {
    if (nb == 0)
        return 0.0;
    double *tmp = (double *)malloc(nb * sizeof(double));
    if (!tmp)
        return vals[0];


    memcpy(tmp, vals, nb * sizeof(double));
    qsort(tmp, nb, sizeof(double), tri_double);
    double med;
    if (nb % 2 == 1)
        med = tmp[nb / 2];
    else
        med = 0.5 * (tmp[nb / 2 - 1] + tmp[nb / 2]);

    free(tmp);
    return med;
}

static int remplis_blc(const unsigned char *pix,
                       int larg,
                       int haut,
                       unsigned char *vu,
                       int sx,
                       int sy,
                       int seuil,
                       Bloc *res,
                       int *pile,
                       size_t t_pile) {
    size_t top = 0;
    size_t idx = (size_t)sy * (size_t)larg + (size_t)sx;
    pile[top++] = (int)idx;

    vu[idx] = 1;

    int x_min = sx;
    int x_max = sx;
    int y_min = sy;
    int y_max = sy;
    int aire = 0;

    static const int voisins[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };


    while (top > 0) {
        int cur = pile[--top];
        int cx = cur % larg;

        int cy = cur / larg;
        aire++;

        if (cx < x_min) x_min = cx;
        if (cx > x_max) x_max = cx;
        if (cy < y_min) y_min = cy;
        if (cy > y_max) y_max = cy;

        for (size_t n = 0; n < 8; ++n) {
            int nx = cx + voisins[n][0];
            int ny = cy + voisins[n][1];
            if (nx < 0 || ny < 0 || nx >= larg || ny >= haut)
                continue;
            size_t nidx = (size_t)ny * (size_t)larg + (size_t)nx;
            if (vu[nidx])
                continue;
            if (pix[nidx] > seuil)
                continue;
            if (top >= t_pile)
                continue;

            vu[nidx] = 1;
            pile[top++] = (int)nidx;
        }
    }

    if (res) {
        res->x_min = x_min;
        res->x_max = x_max;
        res->y_min = y_min;
        res->y_max = y_max;
        res->aire = aire;
    }
    return aire;
}

static int trouve_blocs(const unsigned char *pix,
                        int larg,
                        int haut,
                        BlocTab *tab) {
    const int seuil = 220;
    size_t nb_pix = (size_t)larg * (size_t)haut;

    unsigned char *vu = (unsigned char *)calloc(nb_pix, 1);
    if (!vu)
        return -1;
    int *pile = (int *)malloc(nb_pix * sizeof(int));
    if (!pile) {
        free(vu);
        return -1;
    }

    for (int y = 0; y < haut; ++y) {

        for (int x = 0; x < larg; ++x) {
            size_t pos = (size_t)y * (size_t)larg + (size_t)x;
            if (vu[pos])
                continue;
            if (pix[pos] > seuil)
                continue;
            Bloc bloc;
            int aire = remplis_blc(pix, larg, haut, vu, x, y, seuil, &bloc, pile, nb_pix);
            if (aire <= 0)
                continue;
            int bloc_larg = bloc.x_max - bloc.x_min + 1;
            int bloc_haut = bloc.y_max - bloc.y_min + 1;
            if (bloc_larg < 2 || bloc_haut < 2)
                continue;
            double ratio = (double)bloc_larg / (double)bloc_haut;

            if (ratio < 0.08 || ratio > 12.0)
                continue;
            if ((double)aire < (double)(bloc_larg * bloc_haut) * 0.1)
                continue;
            bloc.aire = aire;
            if (bloc_tab_ajoute(tab, bloc) != 0) {
                free(pile);
                free(vu);
                return -1;
            }
        }
    }



    free(pile);
    free(vu);
    if (tab->nb == 0)
        return -1;

    double somme = 0.0;
    for (size_t i = 0; i < tab->nb; ++i)
        somme += (double)tab->tab[i].aire;
    double moy = somme / (double)tab->nb;
    double seuil_air = moy * 0.2;
    if (seuil_air < 4.0)
        seuil_air = 4.0;

    size_t ecrit = 0;
    for (size_t i = 0; i < tab->nb; ++i) {
        Bloc bloc = tab->tab[i];
        if ((double)bloc.aire >= seuil_air)
            tab->tab[ecrit++] = bloc;
    }

    tab->nb = ecrit;

    return tab->nb > 0 ? 0 : -1;
}

static int ajoute_lot(LotLig **lots,
                      size_t *nb,
                      size_t *cap,
                      size_t debut,
                      size_t nb_blocs) {
    if (*nb == *cap) {
        size_t cap_nouv = *cap ? *cap * 2 : 8;

        LotLig *tmp = (LotLig *)realloc(*lots, cap_nouv * sizeof(LotLig));
        if (!tmp)
            return -1;
        *lots = tmp;
        *cap = cap_nouv;
    }
    (*lots)[(*nb)++] = (LotLig){debut, nb_blocs};
    return 0;
}

static int grille_depuis_blocs(const unsigned char *pix,
                               int larg,
                               int haut,

                               const char *dossier,
                               BlocTab *tab,
                               LotLig *lots,
                               size_t nb_lots) {
    if (nb_lots < 2 || tab->nb < 2)
        return -1;

    typedef struct {
        double haut;
        double bas;
        double centre;
        size_t debut;

        size_t nb;
    } InfosLig;

    InfosLig *infos = (InfosLig *)malloc(nb_lots * sizeof(InfosLig));
    if (!infos)
        return -1;

    double *hauteurs = (double *)malloc(nb_lots * sizeof(double));
    if (!hauteurs) {
        free(infos);
        return -1;
    }

    double tot_larg = 0.0;
    double tot_haut = 0.0;
    for (size_t i = 0; i < tab->nb; ++i) {
        double w = (double)(tab->tab[i].x_max - tab->tab[i].x_min + 1);

        double h = (double)(tab->tab[i].y_max - tab->tab[i].y_min + 1);
        tot_larg += w;
        tot_haut += h;
    }
    double moy_larg = tot_larg / (double)tab->nb;
    double moy_haut = tot_haut / (double)tab->nb;
    if (moy_larg < 1.0) moy_larg = 1.0;
    if (moy_haut < 1.0) moy_haut = 1.0;

    for (size_t i = 0; i < nb_lots; ++i) {
        LotLig lot = lots[i];
        double haut_l = (double)haut;
        double bas_l = 0.0;
        for (size_t k = 0; k < lot.nb; ++k) {
            const Bloc *bloc = &tab->tab[lot.debut + k];

            if ((double)bloc->y_min < haut_l)
                haut_l = (double)bloc->y_min;
            if ((double)(bloc->y_max + 1) > bas_l)
                bas_l = (double)(bloc->y_max + 1);
        }
        if (bas_l <= haut_l) {
            haut_l = 0.0;
            bas_l = 0.0;
        }
        infos[i].haut = haut_l;
        infos[i].bas = bas_l;
        infos[i].centre = 0.5 * (haut_l + bas_l);
        infos[i].debut = lot.debut;
        infos[i].nb = lot.nb;
        hauteurs[i] = bas_l - haut_l;
    }

    double med_haut = copie_mediane(hauteurs, nb_lots);
    if (med_haut <= 0.0) {
        free(hauteurs);
        free(infos);
        return -1;
    }
    double mini_haut = med_haut * 0.5;
    double maxi_haut = med_haut * 1.8;

    size_t *idx_ok = (size_t *)malloc(nb_lots * sizeof(size_t));
    if (!idx_ok) {
        free(hauteurs);
        free(infos);
        return -1;
    }

    size_t cible_col = 0;
    size_t freq_opt = 0;
    for (size_t i = 0; i < nb_lots; ++i) {
        size_t freq = 0;
        for (size_t j = 0; j < nb_lots; ++j) {
            if (lots[j].nb == lots[i].nb)
                freq++;
        }
        if (freq > freq_opt || (freq == freq_opt && lots[i].nb > cible_col)) {
            freq_opt = freq;
            cible_col = lots[i].nb;
        }



    }
    if (cible_col < 2) {
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    size_t nb_ok = 0;
    for (size_t i = 0; i < nb_lots; ++i) {
        double h = hauteurs[i];
        if (h < mini_haut || h > maxi_haut)
            continue;
        size_t bloc_nb = lots[i].nb;
        if (bloc_nb < 2)
            continue;
        if ((double)bloc_nb < (double)cible_col * 0.6)
            continue;
        idx_ok[nb_ok++] = i;
    }

    if (nb_ok < 2) {
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    double *centres = (double *)malloc(nb_ok * sizeof(double));
    if (!centres) {
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }
    for (size_t i = 0; i < nb_ok; ++i)
        centres[i] = infos[idx_ok[i]].centre;

    double *ecarts = NULL;
    size_t nb_ecarts = 0;
    if (nb_ok > 1) {
        nb_ecarts = nb_ok - 1;
        ecarts = (double *)malloc(nb_ecarts * sizeof(double));
        if (!ecarts) {
            free(centres);
            free(idx_ok);
            free(hauteurs);
            free(infos);
            return -1;
        }
        for (size_t i = 0; i < nb_ecarts; ++i)
            ecarts[i] = centres[i + 1] - centres[i];
            
    }

    double med_ecart = nb_ecarts ? copie_mediane(ecarts, nb_ecarts) : 0.0;
    if (med_ecart <= 0.0)
        med_ecart = moy_haut;
    if (med_ecart <= 0.0)
        med_ecart = 1.0;

    double mini_ecart = med_ecart * 0.5;
    double maxi_ecart = med_ecart * 1.8;
    if (mini_ecart < 1.0)
        mini_ecart = 1.0;

    size_t best_deb = 0;
    size_t best_len = 1;
    size_t cur_deb = 0;
    size_t cur_len = 1;

    if (nb_ok == 1) {
        best_len = 1;
    } else {
        for (size_t i = 0; i + 1 < nb_ok; ++i) {
            double diff = centres[i + 1] - centres[i];
            if (diff >= mini_ecart && diff <= maxi_ecart) {
                cur_len++;
            } else {
                if (cur_len > best_len) {
                    best_len = cur_len;
                    best_deb = cur_deb;
                }
                cur_deb = i + 1;
                cur_len = 1;
            }
        }
        if (cur_len > best_len) {
            best_len = cur_len;
            best_deb = cur_deb;
        }
    }

    if (best_len < 2) {
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    size_t *lignes_finales = (size_t *)malloc(best_len * sizeof(size_t));
    if (!lignes_finales) {
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }
    for (size_t i = 0; i < best_len; ++i)
        lignes_finales[i] = idx_ok[best_deb + i];

    size_t col_final = 0;
    size_t col_freq = 0;
    for (size_t i = 0; i < best_len; ++i) {
        size_t bloc_nb = lots[lignes_finales[i]].nb;
        size_t freq = 0;
        for (size_t j = 0; j < best_len; ++j)
            if (lots[lignes_finales[j]].nb == bloc_nb)
                freq++;
        if (freq > col_freq || (freq == col_freq && bloc_nb > col_final)) {
            col_freq = freq;
            col_final = bloc_nb;
        }
    }
    if (col_final < 2) {
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    size_t nb_filtre = 0;
    for (size_t i = 0; i < best_len; ++i) {
        size_t idx = lignes_finales[i];
        if (lots[idx].nb >= col_final)
            lignes_finales[nb_filtre++] = idx;
    }
    if (nb_filtre < 2) {
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    double marge_y = moy_haut * 0.3;
    if (marge_y < 1.0)
        marge_y = 1.0;
    double marge_x = moy_larg * 0.3;
    if (marge_x < 1.0)
        marge_x = 1.0;

    size_t nb_lignes = nb_filtre;
    double *tops = (double *)malloc(nb_lignes * sizeof(double));
    double *bas = (double *)malloc(nb_lignes * sizeof(double));
    if (!tops || !bas) {
        free(bas);
        free(tops);
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }
    for (size_t i = 0; i < nb_lignes; ++i) {
        InfosLig *info = &infos[lignes_finales[i]];
        tops[i] = info->haut;
        bas[i] = info->bas;
    }

    double *col_g = (double *)malloc(col_final * sizeof(double));
    double *col_d = (double *)malloc(col_final * sizeof(double));
    if (!col_g || !col_d) {
        free(col_d);
        free(col_g);
        free(bas);
        free(tops);
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    double *tmp = (double *)malloc(nb_lignes * sizeof(double));
    if (!tmp) {
        free(col_d);
        free(col_g);
        free(bas);
        free(tops);
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    for (size_t c = 0; c < col_final; ++c) {
        size_t nb_vals = 0;
        for (size_t r = 0; r < nb_lignes; ++r) {
            const Bloc *bloc = &tab->tab[lots[lignes_finales[r]].debut + c];
            tmp[nb_vals++] = (double)bloc->x_min;
        }
        col_g[c] = copie_mediane(tmp, nb_vals);

        nb_vals = 0;
        for (size_t r = 0; r < nb_lignes; ++r) {
            const Bloc *bloc = &tab->tab[lots[lignes_finales[r]].debut + c];
            tmp[nb_vals++] = (double)(bloc->x_max + 1);
        }
        col_d[c] = copie_mediane(tmp, nb_vals);
    }

    double *lim_h = (double *)malloc((nb_lignes + 1) * sizeof(double));
    double *lim_v = (double *)malloc((col_final + 1) * sizeof(double));
    if (!lim_h || !lim_v) {
        free(lim_v);
        free(lim_h);
        free(tmp);
        free(col_d);
        free(col_g);
        free(bas);
        free(tops);
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    lim_h[0] = tops[0] - marge_y;
    if (lim_h[0] < 0.0)
        lim_h[0] = 0.0;
    for (size_t i = 1; i < nb_lignes; ++i) {
        double bas_prec = bas[i - 1];
        double haut_cour = tops[i];
        double milieu = 0.5 * (bas_prec + haut_cour);
        if (milieu <= lim_h[i - 1])
            milieu = lim_h[i - 1] + 1.0;
        lim_h[i] = milieu;
    }
    lim_h[nb_lignes] = bas[nb_lignes - 1] + marge_y;
    if (lim_h[nb_lignes] > (double)haut)
        lim_h[nb_lignes] = (double)haut;

    lim_v[0] = col_g[0] - marge_x;
    if (lim_v[0] < 0.0)
        lim_v[0] = 0.0;
    for (size_t c = 1; c < col_final; ++c) {
        double droite_prec = col_d[c - 1];
        double gauche_cour = col_g[c];
        double milieu = 0.5 * (droite_prec + gauche_cour);
        if (milieu <= lim_v[c - 1])
            milieu = lim_v[c - 1] + 1.0;
        lim_v[c] = milieu;
    }
    lim_v[col_final] = col_d[col_final - 1] + marge_x;
    if (lim_v[col_final] > (double)larg)
        lim_v[col_final] = (double)larg;

    Bande *band_h = (Bande *)malloc((nb_lignes + 1) * sizeof(Bande));
    Bande *band_v = (Bande *)malloc((col_final + 1) * sizeof(Bande));
    if (!band_h || !band_v) {
        free(band_v);
        free(band_h);
        free(lim_v);
        free(lim_h);
        free(tmp);
        free(col_d);
        free(col_g);
        free(bas);
        free(tops);
        free(lignes_finales);
        free(ecarts);
        free(centres);
        free(idx_ok);
        free(hauteurs);
        free(infos);
        return -1;
    }

    int dernier = -1;
    for (size_t i = 0; i < nb_lignes + 1; ++i) {
        int y = (int)lround(lim_h[i]);
        if (y < 0) y = 0;
        if (y > haut) y = haut;
        if (y <= dernier) y = dernier + 1;
        if (y > haut) y = haut;
        band_h[i].deb = y;
        band_h[i].fin = y;
        dernier = y;
    }

    dernier = -1;
    for (size_t i = 0; i < col_final + 1; ++i) {
        int x = (int)lround(lim_v[i]);
        if (x < 0) x = 0;
        if (x > larg) x = larg;
        if (x <= dernier) x = dernier + 1;
        if (x > larg) x = larg;
        band_v[i].deb = x;
        band_v[i].fin = x;
        dernier = x;
    }

    int extrait = couper_cases(pix, larg, haut,
                               band_h, nb_lignes + 1,
                               band_v, col_final + 1,
                               0, dossier,
                               NULL, NULL, 0, NULL);

    free(band_v);
    free(band_h);
    free(lim_v);
    free(lim_h);
    free(tmp);
    free(col_d);
    free(col_g);
    free(bas);
    free(tops);
    free(lignes_finales);
    free(ecarts);
    free(centres);
    free(idx_ok);
    free(hauteurs);
    free(infos);

    if (extrait <= 0)
        return -1;
    printf("Grille synthetisee: %d cellules sauvegardees dans %s\n", extrait, dossier);
    return 0;
}

int decoupe_lettres(const unsigned char *pix,
                    int larg,
                    int haut,
                    const char *dossier) {
    if (pret_dossier(dossier) != 0)
        return -1;

    BlocTab tab = {0};
    if (trouve_blocs(pix, larg, haut, &tab) != 0) {
        bloc_tab_libere(&tab);
        fprintf(stderr, "Detection de lettres impossible dans l'image.\n");
        return -1;
    }

    if (tab.nb == 0) {
        bloc_tab_libere(&tab);
        fprintf(stderr, "Aucun composant detecte.\n");
        return -1;
    }

    LotLig *lots = NULL;
    size_t nb_lots = 0;
    size_t cap_lots = 0;

    qsort(tab.tab, tab.nb, sizeof(Bloc), tri_bloc_y);

    double somme_h = 0.0;
    for (size_t i = 0; i < tab.nb; ++i)
        somme_h += (double)(tab.tab[i].y_max - tab.tab[i].y_min + 1);
    double moy_h = somme_h / (double)tab.nb;
    if (moy_h < 1.0)
        moy_h = 1.0;

    double seuil_ligne = moy_h * 0.75;
    if (seuil_ligne < 2.0)
        seuil_ligne = 2.0;

    size_t deb = 0;
    size_t nb_blocs = 0;
    double centre = 0.0;

    for (size_t i = 0; i < tab.nb; ++i) {
        double milieu = ((double)tab.tab[i].y_min + (double)tab.tab[i].y_max) * 0.5;
        if (nb_blocs == 0) {
            deb = i;
            nb_blocs = 1;
            centre = milieu;
            continue;
        }
        if (milieu - centre > seuil_ligne) {
            if (ajoute_lot(&lots, &nb_lots, &cap_lots, deb, nb_blocs) != 0) {
                free(lots);
                bloc_tab_libere(&tab);

                fprintf(stderr, "Allocation memoire echouee.\n");

                return -1;
            }
            deb = i;
            nb_blocs = 1;
            centre = milieu;
        } else {
            centre = (centre * nb_blocs + milieu) / (double)(nb_blocs + 1);
            nb_blocs++;
        }
    }

    if (nb_blocs > 0) {
        if (ajoute_lot(&lots, &nb_lots, &cap_lots, deb, nb_blocs) != 0) {
            free(lots);
            bloc_tab_libere(&tab);
            fprintf(stderr, "Allocation memoire echouee.\n");
            return -1;
        }
    }

    if (nb_lots == 0) {
        free(lots);

        bloc_tab_libere(&tab);
        fprintf(stderr, "Impossible de structurer les lettres en lignes.\n");
        return -1;
    }

    for (size_t r = 0; r < nb_lots; ++r) {
        LotLig lot = lots[r];
        if (lot.nb > 0)
            qsort(tab.tab + lot.debut, lot.nb, sizeof(Bloc), tri_bloc_x);
    }

    if (grille_depuis_blocs(pix, larg, haut, dossier, &tab, lots, nb_lots) == 0) {
        free(lots);
        bloc_tab_libere(&tab);

        return 0;
    }

    const int marge = 2;
    int sauver = 0;
    for (size_t r = 0; r < nb_lots; ++r) {
        LotLig lot = lots[r];
        if (lot.nb == 0)
            continue;
        for (size_t c = 0; c < lot.nb; ++c) {
            Bloc *bloc = &tab.tab[lot.debut + c];
            int x_g = bloc->x_min - marge;

            if (x_g < 0) x_g = 0;
            int y_haut = bloc->y_min - marge;

            if (y_haut < 0) y_haut = 0;
            int x_d = bloc->x_max + marge + 1;

            if (x_d > larg) x_d = larg;
            int y_bas = bloc->y_max + marge + 1;

            if (y_bas > haut) y_bas = haut;
            int w = 0;
            int h = 0;
            unsigned char *case_pix = prendre_case(pix, larg, x_g, y_haut, x_d, y_bas, &w, &h);

            if (!case_pix)
                continue;

            if (sauver_case(dossier, c, r, case_pix, w, h, w) == 0)
                sauver++;
                
            free(case_pix);
        }
    }

    if (sauver > 0)
        printf("Extraction terminee: %d lettres sauvegardees dans %s\n", sauver, dossier);
    else
        fprintf(stderr, "Aucune lettre sauvegardee.\n");

    free(lots);
    bloc_tab_libere(&tab);
    return sauver > 0 ? 0 : -1;
}
