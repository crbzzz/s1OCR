#ifndef NN_OCR_H
#define NN_OCR_H



int nn_init(const char *weights_path);
char nn_predict_letter_from_file(const char *png_path);
int nn_process_grid(const char *letters_dir, const char *grille_path, const char *mots_path);
void nn_shutdown(void);

#endif 

