#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "grid_splitter.h"

unsigned char *charger_image(const char *chem, int *larg, int *haut, int *canaux) {
    return stbi_load(chem, larg, haut, canaux, 1);
}

void liberer_image(unsigned char *img) {
    stbi_image_free(img);
}

int ecrire_png(const char *chem, int larg, int haut, int comp, const void *donnees, int stride) {
    if (!stbi_write_png(chem, larg, haut, comp, donnees, stride))
        return -1;
    return 0;
}
