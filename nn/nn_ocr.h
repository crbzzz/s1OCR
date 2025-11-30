#ifndef NN_OCR_H
#define NN_OCR_H

/*
 * Minimal OCR API for the MLP-based letter recognizer.
 *
 * Functions:
 *   int nn_init(const char *weights_path);
 *     - Loads weights from a text file (format: input_dim hidden_dim output_dim,
 *       then W1, b1, W2, b2 as floats).
 *
 *   char nn_predict_letter_from_file(const char *png_path);
 *     - Loads a single binarized PNG letter and returns the predicted char ('A'..'Z' or '?').
 *
 *   int nn_process_grid(const char *letters_dir,
 *                       const char *grille_path,
 *                       const char *mots_path);
 *     - Scans letters_dir for files named either "row_col.png" or "xCOL_yROW.png",
 *       rebuilds the grid, writes grille_path (space-separated) and mots_path (no spaces).
 *
 *   void nn_shutdown(void);
 *     - Frees allocated buffers/weights.
 */

int nn_init(const char *weights_path);
char nn_predict_letter_from_file(const char *png_path);
int nn_process_grid(const char *letters_dir, const char *grille_path, const char *mots_path);
void nn_shutdown(void);

#endif /* NN_OCR_H */

