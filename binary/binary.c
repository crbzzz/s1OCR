#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "stb_image.h"
#include "stb_image_write.h"

static void basename_no_ext(const char* path, char* out, size_t n) {
    const char* p = path;
    const char* last_slash = path;
    while (*p) { if (*p=='/'||*p=='\\') last_slash = p+1; p++; }
    const char* name = last_slash;
    const char* dot = name + strlen(name);
    while (dot>name && *dot!='.') dot--;
    if (dot==name || *dot!='.') dot = name + strlen(name);
    size_t len = (size_t)(dot - name);
    if (len >= n) len = n-1;
    memcpy(out, name, len);
    out[len] = '\0';
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <fichier_image_dans_samples> [seuil 0-255]\n", argv[0]);
        printf("Ex: %s easy.png 128\n", argv[0]);
        return 1;
    }

    int threshold = 128;
    if (argc >= 3) {
        threshold = atoi(argv[2]);
        if (threshold < 0) threshold = 0;
        if (threshold > 255) threshold = 255;
    }

    char in_path[512];
    snprintf(in_path, sizeof(in_path), "samples/%s", argv[1]);

    int w, h, comp;
    unsigned char* img = stbi_load(in_path, &w, &h, &comp, 0);
    if (!img) {
        fprintf(stderr, "Echec chargement: %s\n", in_path);
        return 2;
    }

    int src_c = comp;
    int stride = w * src_c;
    unsigned char* bw = (unsigned char*)malloc((size_t)w * (size_t)h);
    if (!bw) {
        stbi_image_free(img);
        fprintf(stderr, "Memoire insuffisante\n");
        return 3;
    }

    for (int y = 0; y < h; y++) {
        const unsigned char* row = img + (size_t)y * (size_t)stride;
        for (int x = 0; x < w; x++) {
            unsigned char r,g,b;
            if (src_c == 1) {
                r = g = b = row[x];
            } else {
                const unsigned char* px = row + (size_t)x * (size_t)src_c;
                r = px[0];
                g = px[1 % src_c];
                b = px[2 % src_c];
            }
            double ylin = 0.2126 * (double)r + 0.7152 * (double)g + 0.0722 * (double)b;
            bw[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)((ylin >= threshold) ? 255 : 0);
        }
    }

    MKDIR("out");
    char base[256];
    basename_no_ext(argv[1], base, sizeof(base));
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "out/%s_bw.png", base);

    if (!stbi_write_png(out_path, w, h, 1, bw, w * 1)) {
        fprintf(stderr, "Echec ecriture: %s\n", out_path);
        free(bw);
        stbi_image_free(img);
        return 4;
    }

    printf("OK -> %s (seuil=%d)\n", out_path, threshold);
    free(bw);
    stbi_image_free(img);
    return 0;
}

/*
Placez ces deux headers dans le même dossier que binary.c sous les noms:
  - stb_image.h
  - stb_image_write.h
Vous pouvez aussi coller leur contenu ici si vous préférez un seul fichier.
Sources officielles : https://github.com/nothings/stb
Compilation:
  gcc binary.c -o binary
Utilisation:
  ./binary easy.png
  ./binary medium.png 100
*/
