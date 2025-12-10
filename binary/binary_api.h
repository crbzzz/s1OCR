#ifndef BINARY_API_H
#define BINARY_API_H

#include <stdint.h>

/* Binary image structure */
typedef struct {
    uint8_t *data;
    int width;
    int height;
} BinaryImage;

/* Load PNG image as grayscale (no binarization) */
BinaryImage* binary_load_grayscale(const char *png_path);

/* Load and binarize PNG image using Otsu threshold */
BinaryImage* binary_load_otsu(const char *png_path);

/* Free binary image */
void binary_free(BinaryImage *img);

/* Save binary image to PNG */
int binary_save_png(const BinaryImage *img, const char *output_path);

/* Extract connected components (letters) from grid image */
typedef struct {
    int x, y, w, h;  /* bounding box */
} BoundingBox;

typedef struct {
    BoundingBox *boxes;
    int count;
} ComponentList;

/* Find all connected components in binary image */
ComponentList* binary_find_components(const BinaryImage *img);
void binary_free_components(ComponentList *comp);

/* Extract sub-image using bounding box */
BinaryImage* binary_crop(const BinaryImage *img, const BoundingBox *box);

#endif
