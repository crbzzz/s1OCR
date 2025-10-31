#ifndef GRID_COMPOSANTS_INTERNAL_H
#define GRID_COMPOSANTS_INTERNAL_H

#include <stddef.h>

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

void bloc_tab_libere(BlocTab *tab);
int bloc_tab_ajoute(BlocTab *tab, Bloc bloc);
int tri_bloc_y(const void *a, const void *b);
int tri_bloc_x(const void *a, const void *b);
int tri_double(const void *a, const void *b);
double copie_mediane(const double *vals, size_t nb);
int remplis_blc(const unsigned char *pix,
                int larg,int haut,
                unsigned char *vu,
                int sx, int sy,
                int seuil, Bloc *res,int *pile,
                size_t taille_pile);
int trouve_blocs(const unsigned char *pix, int larg, int haut, BlocTab *tab);
int ajoute_lot(LotLig **lots, size_t *nb, size_t *cap, size_t debut, size_t nb_blocs);
int grille_depuis_blocs(const unsigned char *pix,
                        int larg,int haut,
                        const char *dossier,
                        const BlocTab *tab,
                        const LotLig *lots,
                        size_t nb_lots);

#endif
