#include "grid_splitter.h"

#include <stdlib.h>

static void ajoute_bande(Bande **liste, size_t *nb, size_t *cap, int deb, int fin)
{
    if (deb > fin) {
        return;
    }

    if (*nb == *cap) {
        size_t nouvelle_capacite = *cap ? *cap * 2 : 8;
        Bande *nouvelle_liste = (Bande *)realloc(*liste, nouvelle_capacite * sizeof(Bande));
        if (!nouvelle_liste) {
            return;
        }
        *liste = nouvelle_liste;
        *cap = nouvelle_capacite;
    }

    (*liste)[(*nb)++] = (Bande){deb, fin};
}

Bande *trouve_band_h(const unsigned char *pix, int larg, int haut, size_t *nb_sortie)
{
    Bande *bandes = NULL;
    size_t nb = 0;
    size_t cap = 0;
    int dans_bande = 0;
    int deb_bande = 0;

    int *compte = (int *)malloc((size_t)haut * sizeof(int));
    if (!compte) {
        return NULL;
    }

    int noir_max = 0;
    long long noir_total = 0;

    for (int y = 0; y < haut; ++y) {
        int noir = 0;
        const unsigned char *lig = pix + (size_t)y * (size_t)larg;
        for (int x = 0; x < larg; ++x) {
            if (lig[x] <= 200) {
                noir++;
            }
        }
        compte[y] = noir;
        if (noir > noir_max) {
            noir_max = noir;
        }
        noir_total += noir;
    }

    if (noir_max == 0) {
        free(compte);
        return NULL;
    }

    double moyenne = (double)noir_total / (double)haut;
    double seuil = (noir_max + moyenne) * 0.5;
    double alt = noir_max * 0.35;
    if (seuil < alt) {
        seuil = alt;
    }
    if (seuil < 5.0) {
        seuil = 5.0;
    }

    for (int y = 0; y < haut; ++y) {
        if (compte[y] >= seuil) {
            if (!dans_bande) {
                dans_bande = 1;
                deb_bande = y;
            }
        } else if (dans_bande) {
            ajoute_bande(&bandes, &nb, &cap, deb_bande, y - 1);
            dans_bande = 0;
        }
    }
    if (dans_bande) {
        ajoute_bande(&bandes, &nb, &cap, deb_bande, haut - 1);
    }

    if (!bandes || nb == 0) {
        free(bandes);
        bandes = NULL;
        nb = 0;
        cap = 0;
        dans_bande = 0;
        int seuil2 = (int)(noir_max * 0.25);
        if (seuil2 < 3) {
            seuil2 = 3;
        }
        for (int y = 0; y < haut; ++y) {
            if (compte[y] >= seuil2) {
                if (!dans_bande) {
                    dans_bande = 1;
                    deb_bande = y;
                }
            } else if (dans_bande) {
                ajoute_bande(&bandes, &nb, &cap, deb_bande, y - 1);
                dans_bande = 0;
            }
        }
        if (dans_bande) {
            ajoute_bande(&bandes, &nb, &cap, deb_bande, haut - 1);
        }
    }

    free(compte);
    if (nb_sortie) {
        *nb_sortie = bandes ? nb : 0;
    }
    return bandes;
}

Bande *trouve_band_v(const unsigned char *pix, int larg, int haut, size_t *nb_sortie)
{
    Bande *bandes = NULL;
    size_t nb = 0;
    size_t cap = 0;
    int dans_bande = 0;
    int deb_bande = 0;

    int *compte = (int *)malloc((size_t)larg * sizeof(int));
    if (!compte) {
        return NULL;
    }

    int noir_max = 0;
    long long noir_total = 0;

    for (int x = 0; x < larg; ++x) {
        int noir = 0;
        for (int y = 0; y < haut; ++y) {
            if (pix[(size_t)y * (size_t)larg + (size_t)x] <= 200) {
                noir++;
            }
        }
        compte[x] = noir;
        if (noir > noir_max) {
            noir_max = noir;
        }
        noir_total += noir;
    }

    if (noir_max == 0) {
        free(compte);
        return NULL;
    }

    double moyenne = (double)noir_total / (double)larg;
    double seuil = (noir_max + moyenne) * 0.5;
    double alt = noir_max * 0.35;
    if (seuil < alt) {
        seuil = alt;
    }
    if (seuil < 5.0) {
        seuil = 5.0;
    }

    for (int x = 0; x < larg; ++x) {
        if (compte[x] >= seuil) {
            if (!dans_bande) {
                dans_bande = 1;
                deb_bande = x;
            }
        } else if (dans_bande) {
            ajoute_bande(&bandes, &nb, &cap, deb_bande, x - 1);
            dans_bande = 0;
        }
    }
    if (dans_bande) {
        ajoute_bande(&bandes, &nb, &cap, deb_bande, larg - 1);
    }

    if (!bandes || nb == 0) {
        free(bandes);
        bandes = NULL;
        nb = 0;
        cap = 0;
        dans_bande = 0;
        int seuil2 = (int)(noir_max * 0.25);
        if (seuil2 < 3) {
            seuil2 = 3;
        }
        for (int x = 0; x < larg; ++x) {
            if (compte[x] >= seuil2) {
                if (!dans_bande) {
                    dans_bande = 1;
                    deb_bande = x;
                }
            } else if (dans_bande) {
                ajoute_bande(&bandes, &nb, &cap, deb_bande, x - 1);
                dans_bande = 0;
            }
        }
        if (dans_bande) {
            ajoute_bande(&bandes, &nb, &cap, deb_bande, larg - 1);
        }
    }

    free(compte);
    if (nb_sortie) {
        *nb_sortie = bandes ? nb : 0;
    }
    return bandes;
}

void liberer_bandes(Bande *bandes)
{
    free(bandes);
}
