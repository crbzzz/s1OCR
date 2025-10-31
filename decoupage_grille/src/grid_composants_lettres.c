#include "grid_splitter.h"
#include "grid_composants_internal.h"

#include <stdio.h>
#include <stdlib.h>

int decoupe_lettres(const unsigned char *pix, int larg, int haut, const char *dossier)
{
    if (pret_dossier(dossier) != 0) {
        return -1;
    }

    BlocTab tab = {0};
    if (trouve_blocs(pix, larg, haut, &tab) != 0) {
        bloc_tab_libere(&tab);
        fprintf(stderr, "impossible\n");
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
    for (size_t i = 0; i < tab.nb; ++i) {
        somme_h += (double)(tab.tab[i].y_max - tab.tab[i].y_min + 1);
    }

    double moy_h = somme_h / (double)tab.nb;
    if (moy_h < 1.0) {
        moy_h = 1.0;
    }

    double seuil_ligne = moy_h * 0.75;
    if (seuil_ligne < 2.0) {
        seuil_ligne = 2.0;
    }

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
        fprintf(stderr, "Erruer.\n");
        return -1;
    }

    for (size_t r = 0; r < nb_lots; ++r) {
        LotLig lot = lots[r];
        if (lot.nb > 0) {
            qsort(tab.tab + lot.debut, lot.nb, sizeof(Bloc), tri_bloc_x);
        }
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
        if (lot.nb == 0) {
            continue;
        }
        for (size_t c = 0; c < lot.nb; ++c) {
            Bloc *bloc = &tab.tab[lot.debut + c];
            int x_gauche = bloc->x_min - marge;
            if (x_gauche < 0) {
                x_gauche = 0;
            }
            int y_haut = bloc->y_min - marge;
            if (y_haut < 0) {
                y_haut = 0;
            }
            int x_droite = bloc->x_max + marge + 1;
            if (x_droite > larg) {
                x_droite = larg;
            }
            int y_bas = bloc->y_max + marge + 1;
            if (y_bas > haut) {
                y_bas = haut;
            }

            int w = 0;
            int h = 0;
            unsigned char *case_pix = prendre_case(pix, larg, x_gauche, y_haut, x_droite, y_bas, &w, &h);
            if (!case_pix) {
                continue;
            }
            if (sauver_case(dossier, c, r, case_pix, w, h, w) == 0) {
                sauver++;
            }
            free(case_pix);
        }
    }

    if (sauver > 0) {
        if (uniformiser_cases(dossier) != 0) {
            fprintf(stderr, "Normalisation impossible sur %s\n", dossier);
        }
        printf("fini\n");
    } else {
        fprintf(stderr, "Erreur.\n");
    }

    free(lots);
    bloc_tab_libere(&tab);
    return (sauver > 0) ? 0 : -1;
}
