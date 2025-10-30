#ifndef GRID_SPLITTER_H
#define GRID_SPLITTER_H

#include <stddef.h>

typedef struct {
    int deb;
    int fin;
} Bande;

int creer_dossier(const char *chem);
int vider_dossier(const char *chem);
int pret_dossier(const char *chem);

unsigned char *charger_image(const char *chem, int *larg, int *haut, int *canaux);
void liberer_image(unsigned char *img);
int ecrire_png(const char *chem, int larg, int haut, int comp, const void *donnees, int stride);

Bande *trouve_band_h(const unsigned char *pix, int larg, int haut, size_t *nb);
Bande *trouve_band_v(const unsigned char *pix, int larg, int haut, size_t *nb);
void liberer_bandes(Bande *bandes);

unsigned char *prendre_case(const unsigned char *img,int larg_img,int x0,int y0, int x1,int y1,int *ret_larg, int *ret_haut);
int sauver_case(const char *dossier, size_t col, size_t lig, const unsigned char *pix,  int larg,int haut,int stride);
int couper_cases(const unsigned char *img, int larg,int haut,const Bande *band_h, size_t nb_h,const Bande *band_v, 
    size_t nb_v,int marge, const char *dossier,
    double *tab_larg,double *tab_haut,size_t max_cases,size_t *nb_sortie);

int cases_uniformes(const double *tab_larg,const double *tab_haut,
                    size_t nb);

int decoupe_lettres(const unsigned char *img,int larg,int haut,
                    const char *dossier);
int decoupe_grille(const unsigned char *img,
                   int larg,int haut,const char *dossier);

#endif
