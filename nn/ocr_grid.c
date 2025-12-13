#define STB_IMAGE_IMPLEMENTATION
#include "../binary/stb_image.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

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

typedef struct {
    int index;
    char path[512];
} WordDir;

typedef struct {
    int index;
    char path[512];
} WordLetterFile;

static NNOCRModel g_model = {0};
static int g_tile_w = 0;
static int g_tile_h = 0;

static int cmp_word_dir(const void *a, const void *b) {
    const WordDir *da = (const WordDir *)a;
    const WordDir *db = (const WordDir *)b;
    if (da->index < db->index) return -1;
    if (da->index > db->index) return 1;
    return strcmp(da->path, db->path);
}

static int cmp_word_letter(const void *a, const void *b) {
    const WordLetterFile *la = (const WordLetterFile *)a;
    const WordLetterFile *lb = (const WordLetterFile *)b;
    if (la->index < lb->index) return -1;
    if (la->index > lb->index) return 1;
    return strcmp(la->path, lb->path);
}

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

    if (g_tile_w == 0 && g_tile_h == 0 && len == g_model.input_dim) {
        g_tile_w = w;
        g_tile_h = h;
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

static int directory_contains_grid_letters(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        int row = 0, col = 0;
        if (parse_letter_indices(ent->d_name, &row, &col)) {
            found = 1;
            break;
        }
    }
    closedir(dir);
    return found;
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

static int is_directory_path(const char *path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return 0;
    }
    return (st.st_mode & _S_IFDIR) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
#endif
}

static int is_digits(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (!isdigit(*p)) return 0;
    }
    return 1;
}

static int parse_numeric_png(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || strcmp(dot, ".png") != 0) {
        return -1;
    }
    size_t len = (size_t)(dot - name);
    if (len == 0 || len >= 32) {
        return -1;
    }
    char tmp[32];
    memcpy(tmp, name, len);
    tmp[len] = '\0';
    if (!is_digits(tmp)) {
        return -1;
    }
    return atoi(tmp);
}

static WordDir *collect_word_dirs(const char *root, size_t *out_count) {
    DIR *dir = opendir(root);
    if (!dir) {
        return NULL;
    }
    WordDir *list = NULL;
    size_t cap = 0;
    size_t count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!is_digits(ent->d_name)) continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", root, ent->d_name);
        if (!is_directory_path(full)) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            WordDir *tmp = (WordDir *)realloc(list, cap * sizeof(WordDir));
            if (!tmp) {
                free(list);
                closedir(dir);
                return NULL;
            }
            list = tmp;
        }
        list[count].index = atoi(ent->d_name);
        snprintf(list[count].path, sizeof(list[count].path), "%s", full);
        ++count;
    }
    closedir(dir);
    if (count == 0) {
        free(list);
        return NULL;
    }
    qsort(list, count, sizeof(WordDir), cmp_word_dir);
    *out_count = count;
    return list;
}

static int find_grid_directory(const char *root, char *out, size_t out_sz) {
    if (directory_contains_grid_letters(root)) {
        snprintf(out, out_sz, "%s", root);
        return 1;
    }
    DIR *dir = opendir(root);
    if (!dir) {
        return 0;
    }
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", root, ent->d_name);
        if (!is_directory_path(full)) continue;
        if (directory_contains_grid_letters(full)) {
            snprintf(out, out_sz, "%s", full);
            found = 1;
            break;
        }
    }
    closedir(dir);
    return found;
}

static WordLetterFile *collect_word_letters(const char *dir_path, size_t *out_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return NULL;
    }
    WordLetterFile *list = NULL;
    size_t cap = 0;
    size_t count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        int idx = parse_numeric_png(ent->d_name);
        if (idx < 0) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            WordLetterFile *tmp = (WordLetterFile *)realloc(list, cap * sizeof(WordLetterFile));
            if (!tmp) {
                free(list);
                closedir(dir);
                return NULL;
            }
            list = tmp;
        }
        list[count].index = idx;
        snprintf(list[count].path, sizeof(list[count].path), "%s/%s", dir_path, ent->d_name);
        ++count;
    }
    closedir(dir);
    if (count == 0) {
        free(list);
        return NULL;
    }
    qsort(list, count, sizeof(WordLetterFile), cmp_word_letter);
    *out_count = count;
    return list;
}

static float *load_word_letter_vector(const char *path) {
    if (g_tile_w <= 0 || g_tile_h <= 0) {
        return NULL;
    }
    int w = 0, h = 0, ch = 0;
    unsigned char *img = stbi_load(path, &w, &h, &ch, 1);
    if (!img || w <= 0 || h <= 0) {
        if (img) stbi_image_free(img);
        return NULL;
    }
    float *vec = (float *)malloc(sizeof(float) * (size_t)g_tile_w * (size_t)g_tile_h);
    if (!vec) {
        stbi_image_free(img);
        return NULL;
    }
    for (int ty = 0; ty < g_tile_h; ++ty) {
        int sy = (int)((double)ty * h / g_tile_h);
        if (sy < 0) sy = 0;
        if (sy >= h) sy = h - 1;
        for (int tx = 0; tx < g_tile_w; ++tx) {
            int sx = (int)((double)tx * w / g_tile_w);
            if (sx < 0) sx = 0;
            if (sx >= w) sx = w - 1;
            unsigned char v = img[sy * w + sx];
            vec[ty * g_tile_w + tx] = (v > 127) ? 1.0f : 0.0f;
        }
    }
    stbi_image_free(img);
    return vec;
}

static char *recognize_word_from_dir(const char *dir_path) {
    size_t letter_count = 0;
    WordLetterFile *letters = collect_word_letters(dir_path, &letter_count);
    if (!letters) {
        return NULL;
    }
    char *buffer = (char *)malloc(letter_count + 1);
    if (!buffer) {
        free(letters);
        return NULL;
    }
    size_t pos = 0;
    for (size_t i = 0; i < letter_count; ++i) {
        float *vec = load_word_letter_vector(letters[i].path);
        if (!vec) {
            continue;
        }
        char letter = predict_letter_from_vec(vec);
        free(vec);
        buffer[pos++] = letter;
    }
    free(letters);
    if (pos == 0) {
        free(buffer);
        return NULL;
    }
    buffer[pos] = '\0';
    return buffer;
}

static void free_words(char **words, size_t n) {
    if (!words) return;
    for (size_t i = 0; i < n; ++i) {
        free(words[i]);
    }
    free(words);
}

static int write_words_from_directories(const char *root, const char *mots_path) {
    if (g_tile_w <= 0 || g_tile_h <= 0) {
        return 0;
    }
    size_t dir_count = 0;
    WordDir *dirs = collect_word_dirs(root, &dir_count);
    if (!dirs) {
        return 0;
    }
    char **words = (char **)malloc(sizeof(char *) * dir_count);
    if (!words) {
        free(dirs);
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < dir_count; ++i) {
        char *w = recognize_word_from_dir(dirs[i].path);
        if (w && *w) {
            words[count++] = w;
        } else {
            free(w);
        }
    }
    free(dirs);
    if (count == 0) {
        free(words);
        return 0;
    }
    FILE *fm = fopen(mots_path, "w");
    if (!fm) {
        free_words(words, count);
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        fprintf(fm, "%s\n", words[i]);
    }
    fclose(fm);
    free_words(words, count);
    return 1;
}

int nn_init(const char *weights_path) {
    free_model(&g_model);
    g_tile_w = 0;
    g_tile_h = 0;
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
    char grid_dir[512];
    if (!find_grid_directory(letters_dir, grid_dir, sizeof(grid_dir))) {
        fprintf(stderr, "No grid directory found in %s\n", letters_dir);
        return 0;
    }
    LetterImage *imgs = scan_letter_images(grid_dir, &count, &rows, &cols);
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

    if (!write_words_from_directories(letters_dir, mots_path)) {
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
    }

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
    g_tile_w = 0;
    g_tile_h = 0;
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
