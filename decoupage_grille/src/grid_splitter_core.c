#include "grid_splitter.h"

#include <stdio.h>
#include <stdlib.h>

int decoupe_grille(const unsigned char *pix, int larg, int haut, const char *dossier) {
    if (pret_dossier(dossier) != 0) {
        return -1;
    }

    size_t nb_lignes = 0;
    size_t nb_col = 0;
    Bande *band_h = trouve_band_h(pix, larg, haut, &nb_lignes);
    Bande *band_v = trouve_band_v(pix, larg, haut, &nb_col);

    if (band_h && band_v && nb_lignes >= 2 && nb_col >= 2) {
        const int marge = 1;
        size_t nb_cases = (nb_lignes - 1) * (nb_col - 1);
        double *tab_l = NULL;
        double *tab_h = NULL;
        size_t nb_stat = 0;

        if (nb_cases > 0) {
            tab_l = (double *)malloc(nb_cases * sizeof(double));
            tab_h = (double *)malloc(nb_cases * sizeof(double));
            if (!tab_l || !tab_h) {
                free(tab_l);
                free(tab_h);
                tab_l = tab_h = NULL;
                nb_cases = 0;
            }
        }

        int sauv = couper_cases(pix, larg, haut, band_h, nb_lignes, band_v, nb_col, marge, dossier, tab_l, tab_h, nb_cases, &nb_stat);

        liberer_bandes(band_h);
        liberer_bandes(band_v);

        if (sauv > 0) {
            int ok = 1;
            if (tab_l && tab_h && nb_stat > 0) {
                ok = cases_uniformes(tab_l, tab_h, nb_stat);
            }

            free(tab_l);
            free(tab_h);

            if (ok) {
                printf("Fini");
                return 0;
            }

            fprintf(stderr, "Erreur, bascule vers la détection pr lettre.\n");
        } else {
            free(tab_l);
            free(tab_h);
        }

        fprintf(stderr, "Detection pr lettres.\n");
        return decoupe_lettres(pix, larg, haut, dossier);
    }

    fprintf(stderr, "Erreur");
    liberer_bandes(band_h);
    liberer_bandes(band_v);
    return decoupe_lettres(pix, larg, haut, dossier);
}
