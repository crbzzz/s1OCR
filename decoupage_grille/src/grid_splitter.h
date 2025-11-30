#ifndef GRID_SPLITTER_H
#define GRID_SPLITTER_H

int crop_grids_in_directory(const char *input_dir, const char *output_dir);
int split_letters_in_directory(const char *input_dir, const char *output_dir);
int ecrire_image(const char *chemin, const unsigned char *pixels, int largeur, int hauteur);
#endif

