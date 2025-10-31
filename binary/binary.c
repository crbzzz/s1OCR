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

static void basename_no_ext(const char* path, char* out, size_t n) 
{
    const char* p = path;
    const char* last_slash = path;
    while (*p) 
    { 
        if (*p=='/'||*p=='\\') 
        {
            last_slash = p+1; p++; 
        }
    }
    const char* name = last_slash;
    const char* dot = name + strlen(name);
    while (dot>name && *dot!='.') dot--;
    if (dot==name || *dot!='.') 
    {
        dot = name + strlen(name);
    }
    size_t len = (size_t)(dot - name);
    if (len >= n) 
    {
        len = n-1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

static int otsu_threshold(const unsigned int hist[256], unsigned int total) 
{
    double sum = 0.0;
    for (int t = 0; t < 256; ++t) 
    {
        sum += t * (double)hist[t];
    }

    double sumB = 0.0;
    unsigned int wB = 0;
    unsigned int wF = 0;
    double varMax = -1.0;
    int threshold = 128;

    for (int t = 0; t < 256; ++t) 
    {
        wB += hist[t];
        if (wB == 0) continue;
        wF = total - wB;
        if (wF == 0) break;

        sumB += t * (double)hist[t];
        double mB = sumB / wB;
        double mF = (sum - sumB) / wF;

        double varBetween = (double)wB * (double)wF * (mB - mF) * (mB - mF);
        if (varBetween > varMax) 
        {
            varMax = varBetween;
            threshold = t;
        }
    }
    return threshold;
}

int main(int argc, char** argv)
{ 
    if (argc < 2) 
    {
        return 1;
    }

    int use_forced_threshold = 0;
    int threshold = 128;
    if (argc >= 3) 
    {
        threshold = atoi(argv[2]);
        if (threshold < 0) threshold = 0;
        if (threshold > 255) threshold = 255;
        use_forced_threshold = 1;
    }

    char in_path[512];
    snprintf(in_path, sizeof(in_path), "samples/%s", argv[1]);

    int w, h, comp;
    unsigned char* img = stbi_load(in_path, &w, &h, &comp, 4);
    if (!img) 
    {
        fprintf(stderr, "Echec : %s\n", in_path);
        return 2;
    }
    const int src_c = 4;
    const int stride = w * src_c;

    unsigned char* gray = (unsigned char*)malloc((size_t)w * (size_t)h);
    if (!gray) 
    {
        stbi_image_free(img);
        fprintf(stderr, "pas assez de mémoire\n");
        return 3;
    }

    unsigned int hist[256] = {0};

    for (int y = 0; y < h; y++) 
    {
        const unsigned char* row = img + (size_t)y * (size_t)stride;
        for (int x = 0; x < w; x++) 
        {
            const unsigned char* px = row + (size_t)x * (size_t)src_c;
            unsigned int r = px[0], g = px[1], b = px[2], a = px[3];

            r = (r * a + 255u * (255u - a)) / 255u;
            g = (g * a + 255u * (255u - a)) / 255u;
            b = (b * a + 255u * (255u - a)) / 255u;

            unsigned int y8 = (unsigned int)(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
            if (y8 > 255u) y8 = 255u;

            gray[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)y8;
            hist[y8]++;
        }
    }

    if (!use_forced_threshold) 
    {
        threshold = otsu_threshold(hist, (unsigned int)((unsigned long long)w * (unsigned long long)h));
    }

    unsigned char* bw = (unsigned char*)malloc((size_t)w * (size_t)h);
    if (!bw) 
    {
        free(gray);
        stbi_image_free(img);
        fprintf(stderr, "X mémoire\n");
        return 3;
    }
    for (int i = 0, n = w * h; i < n; ++i) 
    {
        bw[i] = (gray[i] >= threshold) ? 255 : 0;
    }

    MKDIR("out");
    char base[256];
    basename_no_ext(argv[1], base, sizeof(base));
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "out/%s_bw.png", base);

    if (!stbi_write_png(out_path, w, h, 1, bw, w)) 
    {
        fprintf(stderr, "Echec ecriture: %s\n", out_path);
        free(bw);
        free(gray);
        stbi_image_free(img);
        return 4;
    }

    printf("OK -> %s", out_path);
    free(bw);
    free(gray);
    stbi_image_free(img);
    return 0;
}
