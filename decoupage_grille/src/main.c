#include "grid_splitter.h"

#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        return 1;
    }

    const char *chem_img = argv[1];
    const char *chem_sortie = argv[2];

    int larg = 0;
    int haut = 0;
    int canaux = 0;
    unsigned char *img = charger_image(chem_img, &larg, &haut, &canaux);
    if (!img) {
        fprintf(stderr, "Echec chargement %s\n", chem_img);
        return 1;
    }

    if (decoupe_grille(img, larg, haut, chem_sortie) != 0) {
        liberer_image(img);
        return 1;
    }

    liberer_image(img);
    return 0;
}
