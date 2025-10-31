#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "grid_splitter.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define STBIW_ONLY_PNG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image_write.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

typedef struct {
    int largeur;
    int hauteur;
    unsigned char *pixels;
} ImageSimple;

static void liberer_image(ImageSimple *image) {
    if (image && image->pixels) {
        stbi_image_free(image->pixels);
        image->pixels = NULL;
    }
}

static bool est_extension_image(const char *nom) {
    const char *point = strrchr(nom, '.');
    if (!point || point == nom) {
        return false;
    }
    char extension[8] = {0};
    size_t longueur = strlen(point + 1);
    if (longueur >= sizeof(extension)) {
        return false;
    }
    for (size_t i = 0; i < longueur; ++i) {
        extension[i] = (char)tolower((unsigned char)point[1 + i]);
    }
    return strcmp(extension, "png") == 0 || strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0 ||
           strcmp(extension, "bmp") == 0;
}

static int creer_repertoire(const char *chemin) {
    if (!chemin || chemin[0] == '\0') {
        return -1;
    }
    if (MKDIR(chemin) == 0) {
        return 0;
    }
#ifdef _WIN32
    if (errno == EEXIST) {
        return 0;
    }
#else
    if (errno == EEXIST) {
        return 0;
    }
#endif
    return -1;
}

static void joindre_chemin(char *destination, size_t taille_destination, const char *dossier, const char *nom) {
    if (!dossier || dossier[0] == '\0') {
        snprintf(destination, taille_destination, "%s", nom);
        return;
    }
    size_t longueur = strlen(dossier);
    bool besoin_sep = longueur > 0 && dossier[longueur - 1] != '/' && dossier[longueur - 1] != '\\';
    if (besoin_sep) {
        snprintf(destination, taille_destination, "%s/%s", dossier, nom);
    } else {
        snprintf(destination, taille_destination, "%s%s", dossier, nom);
    }
}

static void nom_sans_extension(const char *fichier, char *tampon, size_t taille_tampon) {
    const char *slash = strrchr(fichier, '/');
    const char *backslash = strrchr(fichier, '\\');
    const char *nom = fichier;
    if (slash && backslash) {
        nom = slash > backslash ? slash + 1 : backslash + 1;
    } else if (slash) {
        nom = slash + 1;
    } else if (backslash) {
        nom = backslash + 1;
    }
    const char *point = strrchr(nom, '.');
    size_t longueur = point ? (size_t)(point - nom) : strlen(nom);
    if (longueur >= taille_tampon) {
        longueur = taille_tampon - 1;
    }
    memcpy(tampon, nom, longueur);
    tampon[longueur] = '\0';
}

static int charger_image(const char *chemin, ImageSimple *resultat) {
    if (!resultat) {
        return -1;
    }
    int largeur = 0;
    int hauteur = 0;
    int comp = 0;
    unsigned char *donnees = stbi_load(chemin, &largeur, &hauteur, &comp, 1);
    if (!donnees) {
        return -1;
    }
    resultat->largeur = largeur;
    resultat->hauteur = hauteur;
    resultat->pixels = donnees;
    return 0;
}

static int ecrire_image(const char *chemin, const unsigned char *pixels, int largeur, int hauteur) {
    if (!pixels) {
        return -1;
    }
    int stride = largeur;
    if (stbi_write_png(chemin, largeur, hauteur, 1, pixels, stride) == 0) {
        return -1;
    }
    return 0;
}

static int collecter_lignes(const unsigned char *pixels, int largeur, int hauteur, bool horizontal, int **lignes) {
    int longueur = horizontal ? hauteur : largeur;
    int profondeur = horizontal ? largeur : hauteur;
    int *comptes = (int *)calloc((size_t)longueur, sizeof(int));
    if (!comptes) {
        return -1;
    }

    for (int i = 0; i < longueur; ++i) {
        int compteur = 0;
        for (int j = 0; j < profondeur; ++j) {
            int x = horizontal ? j : i;
            int y = horizontal ? i : j;
            if (pixels[y * largeur + x] == 0) {
                compteur++;
            }
        }
        comptes[i] = compteur;
    }

    int valeur_max = 0;
    for (int i = 0; i < longueur; ++i) {
        if (comptes[i] > valeur_max) {
            valeur_max = comptes[i];
        }
    }
    if (valeur_max == 0) {
        free(comptes);
        return -1;
    }

    double facteur = horizontal ? 0.55 : 0.55;
    int seuil = (int)(valeur_max * facteur);
    int seuil_min = (int)(profondeur * 0.30);
    if (seuil < seuil_min) {
        seuil = seuil_min;
    }

    int *indices_lignes = (int *)malloc((size_t)longueur * sizeof(int));
    if (!indices_lignes) {
        free(comptes);
        return -1;
    }

    int nb_indices = 0;
    int depart_bloc = -1;
    for (int i = 0; i < longueur; ++i) {
        if (comptes[i] >= seuil) {
            if (depart_bloc < 0) {
                depart_bloc = i;
            }
        } else if (depart_bloc >= 0) {
            int fin_bloc = i - 1;
            indices_lignes[nb_indices++] = (depart_bloc + fin_bloc) / 2;
            depart_bloc = -1;
        }
    }
    if (depart_bloc >= 0) {
        int fin_bloc = longueur - 1;
        indices_lignes[nb_indices++] = (depart_bloc + fin_bloc) / 2;
    }

    free(comptes);
    if (nb_indices < 2) {
        free(indices_lignes);
        return -1;
    }

    *lignes = indices_lignes;
    return nb_indices;
}

static int decouper_grille(const char *chemin_entree, const char *repertoire_sortie) {
    ImageSimple image = {0};
    if (charger_image(chemin_entree, &image) != 0) {
        return -1;
    }
    int *lignes_horizontales = NULL;
    int *lignes_verticales = NULL;
    int nb_lignes_h = collecter_lignes(image.pixels, image.largeur, image.hauteur, true, &lignes_horizontales);
    int nb_lignes_v = collecter_lignes(image.pixels, image.largeur, image.hauteur, false, &lignes_verticales);

    char nom_base[256] = {0};
    nom_sans_extension(chemin_entree, nom_base, sizeof(nom_base));

    char repertoire_grille[512] = {0};
    joindre_chemin(repertoire_grille, sizeof(repertoire_grille), repertoire_sortie, nom_base);

    const int marge = 1;
    if (nb_lignes_h < 2 || nb_lignes_v < 2) {
        free(lignes_horizontales);
        free(lignes_verticales);
        liberer_image(&image);
        return -1;
    }

    if (creer_repertoire(repertoire_grille) != 0) {
        free(lignes_horizontales);
        free(lignes_verticales);
        liberer_image(&image);
        return -1;
    }

    int cases_enregistrees = 0;
    for (int ligne = 0; ligne < nb_lignes_h - 1; ++ligne) {
        int marge_haut = ligne > 0 ? marge : 0;
        int marge_bas = (ligne + 1 < nb_lignes_h - 1) ? marge : 0;
        int bord_haut = lignes_horizontales[ligne] + marge_haut;
        int bord_bas = lignes_horizontales[ligne + 1] - marge_bas;
        if (bord_bas <= bord_haut) {
            continue;
        }
        for (int colonne = 0; colonne < nb_lignes_v - 1; ++colonne) {
            int marge_gauche = colonne > 0 ? marge : 0;
            int marge_droite = (colonne + 1 < nb_lignes_v - 1) ? marge : 0;
            int bord_gauche = lignes_verticales[colonne] + marge_gauche;
            int bord_droit = lignes_verticales[colonne + 1] - marge_droite;
            if (bord_droit <= bord_gauche) {
                continue;
            }
            int largeur_case = bord_droit - bord_gauche + 1;
            int hauteur_case = bord_bas - bord_haut + 1;
            unsigned char *case_pixels = (unsigned char *)malloc((size_t)largeur_case * (size_t)hauteur_case);
            if (!case_pixels) {
                free(lignes_horizontales);
                free(lignes_verticales);
                liberer_image(&image);
                return -1;
            }

            for (int y = 0; y < hauteur_case; ++y) {
                memcpy(case_pixels + (size_t)y * (size_t)largeur_case,
                       image.pixels + (size_t)(bord_haut + y) * (size_t)image.largeur + (size_t)bord_gauche,
                       (size_t)largeur_case);
            }

            char nom_fichier_case[512] = {0};
            snprintf(nom_fichier_case, sizeof(nom_fichier_case), "x%d_y%d.png", colonne, ligne);
            char chemin_complet[1024] = {0};
            joindre_chemin(chemin_complet, sizeof(chemin_complet), repertoire_grille, nom_fichier_case);
            if (ecrire_image(chemin_complet, case_pixels, largeur_case, hauteur_case) == 0) {
                cases_enregistrees++;
            }
            free(case_pixels);
        }
    }

    free(lignes_horizontales);
    free(lignes_verticales);
    liberer_image(&image);
    return cases_enregistrees > 0 ? 0 : -1;
}

static int pour_chaque_image(const char *repertoire_entree,
                          int (*fonction_rappel)(const char *chemin_entree, const char *nom_fichier, void *ctx), void *ctx) {
    DIR *dir = opendir(repertoire_entree);
    if (!dir) {
        return -1;
    }
    struct dirent *entree = NULL;
    int statut = 0;
    while ((entree = readdir(dir)) != NULL) {
        if (entree->d_name[0] == '.') {
            continue;
        }
        if (!est_extension_image(entree->d_name)) {
            continue;
        }
        char chemin_entree[1024] = {0};
        joindre_chemin(chemin_entree, sizeof(chemin_entree), repertoire_entree, entree->d_name);
        if (fonction_rappel(chemin_entree, entree->d_name, ctx) != 0) {
            statut = -1;
        }
    }
    closedir(dir);
    return statut;
}

typedef struct {
    const char *repertoire_sortie;
} ContexteDecoupe;

static int rappel_decoupe(const char *chemin_entree, const char *nom_fichier_case, void *ctx) {
    (void)nom_fichier_case;
    ContexteDecoupe *contexte = (ContexteDecoupe *)ctx;
    return decouper_grille(chemin_entree, contexte->repertoire_sortie);
}

int decouper_lettres_dans_repertoire(const char *repertoire_entree, const char *repertoire_sortie) {
    if (creer_repertoire(repertoire_sortie) != 0) {
        return -1;
    }
    ContexteDecoupe ctx = {.repertoire_sortie = repertoire_sortie};
    return pour_chaque_image(repertoire_entree, rappel_decoupe, &ctx);
}

int main(int argc, char **argv) {
    const char *entree_par_defaut = "data/clean_grid";
    const char *sortie_par_defaut = "data/lettres";
    const char *repertoire_entree = argc >= 2 ? argv[1] : entree_par_defaut;
    const char *repertoire_sortie = argc >= 3 ? argv[2] : sortie_par_defaut;
    return decouper_lettres_dans_repertoire(repertoire_entree, repertoire_sortie) == 0 ? 0 : 1;
}
