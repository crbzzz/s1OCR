/* grid_splitter.h
 *
 * Provides the public interface for the grid cropping and letter splitting
 * utilities used in the decoupage_grille project.
 */

#ifndef GRID_SPLITTER_H
#define GRID_SPLITTER_H

int crop_grids_in_directory(const char *input_dir, const char *output_dir);
int split_letters_in_directory(const char *input_dir, const char *output_dir);

#endif /* GRID_SPLITTER_H */

