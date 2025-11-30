#define STB_IMAGE_IMPLEMENTATION
#include "../binary/stb_image.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nn_ocr.h"

typedef struct {
    int input_dim;
    int hidden_dim;
    int output_dim;
    float *W1; /* [hidden][input] */
    float *b1; /* [hidden] */
    float *W2; /* [output][hidden] */
    float *b2; /* [output] */
} NNOCRModel;

typedef struct {
    int row;
    int col;
    char path[512];
} LetterImage;

static NNOCRModel g_model = {0};

static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static void free_model(NNOCRModel *m) {
    free(m->W1);
    free(m->b1);
    free(m->W2);
    free(m->b2);
    memset(m, 0, sizeof(*m));
}

static int load_weights(const char *weights_path) {
    FILE *f = fopen(weights_path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open weights: %s\n", weights_path);
        return 0;
    }

    if (fscanf(f, "%d %d %d", &g_model.input_dim, &g_model.hidden_dim, &g_model.output_dim) != 3) {
        fprintf(stderr, "Invalid weights header\n");
        fclose(f);
        return 0;
    }

    size_t w1_sz = (size_t)g_model.input_dim * (size_t)g_model.hidden_dim;
    size_t w2_sz = (size_t)g_model.hidden_dim * (size_t)g_model.output_dim;
    g_model.W1 = (float *)malloc(sizeof(float) * w1_sz);
    g_model.b1 = (float *)malloc(sizeof(float) * (size_t)g_model.hidden_dim);
    g_model.W2 = (float *)malloc(sizeof(float) * w2_sz);
    g_model.b2 = (float *)malloc(sizeof(float) * (size_t)g_model.output_dim);
    if (!g_model.W1 || !g_model.b1 || !g_model.W2 || !g_model.b2) {
        fprintf(stderr, "Memory allocation failed for weights\n");
        fclose(f);
        free_model(&g_model);
        return 0;
    }

    for (size_t i = 0; i < w1_sz; ++i) {
        if (fscanf(f, "%f", &g_model.W1[i]) != 1) {
            fprintf(stderr, "Invalid W1 entry\n");
            goto fail;
        }
    }
    for (int i = 0; i < g_model.hidden_dim; ++i) {
        if (fscanf(f, "%f", &g_model.b1[i]) != 1) {
            fprintf(stderr, "Invalid b1 entry\n");
            goto fail;
        }
    }
    for (size_t i = 0; i < w2_sz; ++i) {
        if (fscanf(f, "%f", &g_model.W2[i]) != 1) {
            fprintf(stderr, "Invalid W2 entry\n");
            goto fail;
        }
    }
    for (int i = 0; i < g_model.output_dim; ++i) {
        if (fscanf(f, "%f", &g_model.b2[i]) != 1) {
            fprintf(stderr, "Invalid b2 entry\n");
            goto fail;
        }
    }

    fclose(f);
    return 1;

fail:
    fclose(f);
    free_model(&g_model);
    return 0;
}

static float *load_image_vector(const char *path, int expected_len, int *out_len) {
    int w = 0, h = 0, ch = 0;
    unsigned char *img = stbi_load(path, &w, &h, &ch, 1);
    if (!img) {
        fprintf(stderr, "Failed to load %s\n", path);
        return NULL;
    }
    int len = w * h;
    if (expected_len > 0 && len != expected_len) {
        fprintf(stderr, "Dimension mismatch for %s: got %d, expected %d\n", path, len, expected_len);
        stbi_image_free(img);
        return NULL;
    }

    float *vec = (float *)malloc(sizeof(float) * (size_t)len);
    if (!vec) {
        fprintf(stderr, "Memory allocation failed for image vector %s\n", path);
        stbi_image_free(img);
        return NULL;
    }
    for (int i = 0; i < len; ++i) {
        vec[i] = (img[i] > 127) ? 1.0f : 0.0f;
    }

    if (out_len) {
        *out_len = len;
    }
    stbi_image_free(img);
    return vec;
}

static char predict_letter_from_vec(const float *input) {
    if (!g_model.W1 || !g_model.W2) {
        return '?';
    }

    int hdim = g_model.hidden_dim;
    int odim = g_model.output_dim;
    int idim = g_model.input_dim;

    float *hidden = (float *)malloc(sizeof(float) * (size_t)hdim);
    float *output = (float *)malloc(sizeof(float) * (size_t)odim);
    if (!hidden || !output) {
        free(hidden);
        free(output);
        return '?';
    }

    for (int j = 0; j < hdim; ++j) {
        float s = g_model.b1[j];
        const float *wrow = &g_model.W1[(size_t)j * (size_t)idim];
        for (int i = 0; i < idim; ++i) {
            s += wrow[i] * input[i];
        }
        hidden[j] = sigmoid(s);
    }

    int best = 0;
    float best_val = -1.0e9f;
    for (int k = 0; k < odim; ++k) {
        float s = g_model.b2[k];
        const float *wrow = &g_model.W2[(size_t)k * (size_t)hdim];
        for (int j = 0; j < hdim; ++j) {
            s += wrow[j] * hidden[j];
        }
        float y = sigmoid(s);
        output[k] = y;
        if (y > best_val) {
            best_val = y;
            best = k;
        }
    }

    free(hidden);
    free(output);

    if (best >= 0 && best < 26) {
        return (char)('A' + best);
    }
    return '?';
}

static int parse_letter_indices(const char *name, int *row, int *col) {
    int r = 0, c = 0;
    if (sscanf(name, "%d_%d.png", &r, &c) == 2) {
        *row = r;
        *col = c;
        return 1;
    }
    if (sscanf(name, "x%d_y%d.png", &c, &r) == 2) {
        *row = r;
        *col = c;
        return 1;
    }
    return 0;
}

static int cmp_letter_image(const void *a, const void *b) {
    const LetterImage *la = (const LetterImage *)a;
    const LetterImage *lb = (const LetterImage *)b;
    if (la->row != lb->row) {
        return (la->row < lb->row) ? -1 : 1;
    }
    if (la->col != lb->col) {
        return (la->col < lb->col) ? -1 : 1;
    }
    return strcmp(la->path, lb->path);
}

static LetterImage *scan_letter_images(const char *dir_path, size_t *out_count, int *out_rows, int *out_cols) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s (errno=%d)\n", dir_path, errno);
        return NULL;
    }

    LetterImage *list = NULL;
    size_t cap = 0, cnt = 0;
    int max_row = -1, max_col = -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        int row = 0, col = 0;
        if (!parse_letter_indices(ent->d_name, &row, &col)) {
            continue;
        }
        if (cnt == cap) {
            cap = cap ? cap * 2 : 64;
            LetterImage *tmp = (LetterImage *)realloc(list, cap * sizeof(LetterImage));
            if (!tmp) {
                fprintf(stderr, "Memory allocation failed while scanning %s\n", dir_path);
                free(list);
                closedir(dir);
                return NULL;
            }
            list = tmp;
        }
        list[cnt].row = row;
        list[cnt].col = col;
        snprintf(list[cnt].path, sizeof(list[cnt].path), "%s/%s", dir_path, ent->d_name);
        if (row > max_row) {
            max_row = row;
        }
        if (col > max_col) {
            max_col = col;
        }
        ++cnt;
    }
    closedir(dir);

    if (cnt == 0) {
        fprintf(stderr, "No letter images found in %s\n", dir_path);
        free(list);
        return NULL;
    }

    qsort(list, cnt, sizeof(LetterImage), cmp_letter_image);
    *out_count = cnt;
    *out_rows = max_row + 1;
    *out_cols = max_col + 1;
    return list;
}

int nn_init(const char *weights_path) {
    free_model(&g_model);
    return load_weights(weights_path);
}

char nn_predict_letter_from_file(const char *png_path) {
    int vec_len = 0;
    float *vec = load_image_vector(png_path, g_model.input_dim, &vec_len);
    if (!vec) {
        return '?';
    }
    char c = predict_letter_from_vec(vec);
    free(vec);
    return c;
}

int nn_process_grid(const char *letters_dir, const char *grille_path, const char *mots_path) {
    size_t count = 0;
    int rows = 0, cols = 0;
    LetterImage *imgs = scan_letter_images(letters_dir, &count, &rows, &cols);
    if (!imgs) {
        return 0;
    }

    char **grid = (char **)malloc(sizeof(char *) * (size_t)rows);
    if (!grid) {
        fprintf(stderr, "Memory allocation failed for grid rows\n");
        free(imgs);
        return 0;
    }
    for (int r = 0; r < rows; ++r) {
        grid[r] = (char *)malloc(sizeof(char) * (size_t)cols);
        if (!grid[r]) {
            fprintf(stderr, "Memory allocation failed for grid row\n");
            for (int i = 0; i < r; ++i) {
                free(grid[i]);
            }
            free(grid);
            free(imgs);
            return 0;
        }
        memset(grid[r], '?', (size_t)cols);
    }

    for (size_t i = 0; i < count; ++i) {
        float *vec = load_image_vector(imgs[i].path, g_model.input_dim, NULL);
        if (!vec) {
            fprintf(stderr, "Skipping %s\n", imgs[i].path);
            continue;
        }
        char letter = predict_letter_from_vec(vec);
        grid[imgs[i].row][imgs[i].col] = letter;
        free(vec);
    }

    FILE *fg = fopen(grille_path, "w");
    if (!fg) {
        fprintf(stderr, "Cannot write %s\n", grille_path);
        goto cleanup;
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            fputc(grid[r][c], fg);
            if (c + 1 < cols) {
                fputc(' ', fg);
            }
        }
        fputc('\n', fg);
    }
    fclose(fg);

    FILE *fm = fopen(mots_path, "w");
    if (!fm) {
        fprintf(stderr, "Cannot write %s\n", mots_path);
        goto cleanup;
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            fputc(grid[r][c], fm);
        }
        fputc('\n', fm);
    }
    fclose(fm);

    unsigned long img_count = (unsigned long)count;
    printf("Processed %lu images into %d x %d grid -> %s / %s\n",
           img_count, rows, cols, grille_path, mots_path);

cleanup:
    for (int r = 0; r < rows; ++r) {
        free(grid[r]);
    }
    free(grid);
    free(imgs);
    return 1;
}

void nn_shutdown(void) {
    free_model(&g_model);
}

#ifndef NN_OCR_NO_MAIN
int main(void) {
    if (!nn_init("weights.txt")) {
        return 1;
    }
    if (!nn_process_grid("grid_letters", "grille.txt", "mots.txt")) {
        nn_shutdown();
        return 1;
    }
    nn_shutdown();
    return 0;
}
#endif
