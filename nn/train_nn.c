#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../binary/stb_image.h"

#define OUTPUT_DIM 26
#define MAX_PATH_LEN 512
#define LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

typedef struct {
    float *inputs;
    float *targets;
    int count;
    int input_dim;
    int width;
    int height;
} Dataset;

typedef struct {
    const char *data_path;
    const char *out_path;
    int hidden_dim;
    int epochs;
    float lr;
    float threshold;
} TrainOptions;

static void init_default_options(TrainOptions *opts) {
    opts->data_path = "dataset/train";
    opts->out_path = "weights.txt";
    opts->hidden_dim = 64;
    opts->epochs = 800;
    opts->lr = 0.1f;
    opts->threshold = 0.5f;
}

static int has_png_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    ++dot;
    char buf[8] = {0};
    size_t len = strlen(dot);
    if (len >= sizeof(buf)) return 0;
    for (size_t i = 0; i < len; ++i)
        buf[i] = (char)tolower((unsigned char)dot[i]);
    return strcmp(buf, "png") == 0;
}

static size_t count_images_in_letter(const char *root, char letter) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%c", root, letter);
    DIR *dir = opendir(path);
    if (!dir) return 0;
    size_t total = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (has_png_extension(entry->d_name))
            ++total;
    }
    closedir(dir);
    return total;
}

static size_t count_total_images(const char *root) {
    size_t total = 0;
    for (size_t i = 0; i < OUTPUT_DIM; ++i) {
        total += count_images_in_letter(root, LETTERS[i]);
    }
    return total;
}

static int load_dataset(const TrainOptions *opts, Dataset *ds) {
    memset(ds, 0, sizeof(*ds));
    size_t total = count_total_images(opts->data_path);
    if (total == 0) {
        fprintf(stderr, "Aucune image trouvée dans %s\n", opts->data_path);
        return -1;
    }

    float *inputs = NULL;
    float *targets = NULL;
    int input_dim = 0;
    int width = 0, height = 0;
    size_t index = 0;

    for (size_t letter_idx = 0; letter_idx < OUTPUT_DIM; ++letter_idx) {
        char letter = LETTERS[letter_idx];
        char dir_path[MAX_PATH_LEN];
        snprintf(dir_path, sizeof(dir_path), "%s/%c", opts->data_path, letter);
        DIR *dir = opendir(dir_path);
        if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (!has_png_extension(entry->d_name)) continue;
            char file_path[MAX_PATH_LEN];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            int w, h, comp;
            unsigned char *pix = stbi_load(file_path, &w, &h, &comp, 1);
            if (!pix) {
                fprintf(stderr, "Impossible de charger %s\n", file_path);
                closedir(dir);
                free(inputs);
                free(targets);
                return -1;
            }
            if (input_dim == 0) {
                input_dim = w * h;
                width = w;
                height = h;
                inputs = (float *)malloc(sizeof(float) * input_dim * total);
                targets = (float *)calloc(total * OUTPUT_DIM, sizeof(float));
                if (!inputs || !targets) {
                    fprintf(stderr, "Allocation mémoire impossible.\n");
                    stbi_image_free(pix);
                    closedir(dir);
                    free(inputs);
                    free(targets);
                    return -1;
                }
            } else if (input_dim != w * h) {
                fprintf(stderr, "Taille incohérente pour %s (%dx%d vs %dx%d attendu)\n",
                        file_path, w, h, width, height);
                stbi_image_free(pix);
                closedir(dir);
                free(inputs);
                free(targets);
                return -1;
            }

            float *dst = inputs + index * input_dim;
            for (int i = 0; i < input_dim; ++i) {
                float v = pix[i] / 255.0f;
                dst[i] = (v > opts->threshold) ? 1.0f : 0.0f;
            }
            float *tgt = targets + index * OUTPUT_DIM;
            tgt[letter_idx] = 1.0f;
            ++index;
            stbi_image_free(pix);
        }
        closedir(dir);
    }

    if (index != total) {
        fprintf(stderr, "Avertissement: comptage (%zu) != chargement (%zu)\n", total, index);
        total = index;
    }

    if (total == 0) {
        fprintf(stderr, "Aucune image exploitable trouvée.\n");
        free(inputs);
        free(targets);
        return -1;
    }

    ds->inputs = inputs;
    ds->targets = targets;
    ds->count = (int)total;
    ds->input_dim = input_dim;
    ds->width = width;
    ds->height = height;
    return 0;
}

static void free_dataset(Dataset *ds) {
    free(ds->inputs);
    free(ds->targets);
    memset(ds, 0, sizeof(*ds));
}

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

static void initialize_weights(int input_dim, int hidden_dim, int output_dim,
                               float **W1, float **b1, float **W2, float **b2) {
    *W1 = (float *)malloc(sizeof(float) * hidden_dim * input_dim);
    *b1 = (float *)calloc(hidden_dim, sizeof(float));
    *W2 = (float *)malloc(sizeof(float) * output_dim * hidden_dim);
    *b2 = (float *)calloc(output_dim, sizeof(float));
    if (!*W1 || !*b1 || !*W2 || !*b2) {
        fprintf(stderr, "Allocation mémoire impossible pour les poids.\n");
        exit(1);
    }
    const float scale = 0.2f;
    for (int i = 0; i < hidden_dim * input_dim; ++i)
        (*W1)[i] = (randf() - 0.5f) * scale;
    for (int i = 0; i < output_dim * hidden_dim; ++i)
        (*W2)[i] = (randf() - 0.5f) * scale;
}

static void save_weights(const char *path, int input_dim, int hidden_dim, int output_dim,
                         const float *W1, const float *b1, const float *W2, const float *b2) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Impossible d'écrire %s : %s\n", path, strerror(errno));
        return;
    }
    fprintf(f, "%d %d %d\n", input_dim, hidden_dim, output_dim);
    for (int i = 0; i < hidden_dim * input_dim; ++i)
        fprintf(f, "%g%c", W1[i], (i + 1 == hidden_dim * input_dim) ? '\n' : ' ');
    for (int i = 0; i < hidden_dim; ++i)
        fprintf(f, "%g%c", b1[i], (i + 1 == hidden_dim) ? '\n' : ' ');
    for (int i = 0; i < output_dim * hidden_dim; ++i)
        fprintf(f, "%g%c", W2[i], (i + 1 == output_dim * hidden_dim) ? '\n' : ' ');
    for (int i = 0; i < output_dim; ++i)
        fprintf(f, "%g%c", b2[i], (i + 1 == output_dim) ? '\n' : ' ');
    fclose(f);
    printf("Écrit %s (input_dim=%d, hidden_dim=%d, output_dim=%d)\n",
           path, input_dim, hidden_dim, output_dim);
}

static void train_network(const Dataset *ds, const TrainOptions *opts) {
    int input_dim = ds->input_dim;
    int hidden_dim = opts->hidden_dim;
    int output_dim = OUTPUT_DIM;
    int samples = ds->count;

    float *W1, *b1, *W2, *b2;
    initialize_weights(input_dim, hidden_dim, output_dim, &W1, &b1, &W2, &b2);

    float *grad_W1 = (float *)calloc(hidden_dim * input_dim, sizeof(float));
    float *grad_b1 = (float *)calloc(hidden_dim, sizeof(float));
    float *grad_W2 = (float *)calloc(output_dim * hidden_dim, sizeof(float));
    float *grad_b2 = (float *)calloc(output_dim, sizeof(float));
    float *hidden = (float *)malloc(sizeof(float) * hidden_dim);
    float *output = (float *)malloc(sizeof(float) * output_dim);

    if (!grad_W1 || !grad_b1 || !grad_W2 || !grad_b2 || !hidden || !output) {
        fprintf(stderr, "Allocation mémoire impossible pour l'entraînement.\n");
        free(W1); free(b1); free(W2); free(b2);
        free(grad_W1); free(grad_b1); free(grad_W2); free(grad_b2);
        free(hidden); free(output);
        exit(1);
    }

    const float eps = 1e-6f;
    for (int epoch = 1; epoch <= opts->epochs; ++epoch) {
        memset(grad_W1, 0, sizeof(float) * hidden_dim * input_dim);
        memset(grad_b1, 0, sizeof(float) * hidden_dim);
        memset(grad_W2, 0, sizeof(float) * output_dim * hidden_dim);
        memset(grad_b2, 0, sizeof(float) * output_dim);
        float loss = 0.0f;

        for (int sample = 0; sample < samples; ++sample) {
            const float *x = ds->inputs + sample * input_dim;
            const float *y_true = ds->targets + sample * OUTPUT_DIM;

            for (int j = 0; j < hidden_dim; ++j) {
                float sum = b1[j];
                const float *w_row = W1 + j * input_dim;
                for (int i = 0; i < input_dim; ++i)
                    sum += w_row[i] * x[i];
                hidden[j] = 1.0f / (1.0f + expf(-sum));
            }

            float delta2[OUTPUT_DIM];
            for (int k = 0; k < output_dim; ++k) {
                float sum = b2[k];
                const float *w_row = W2 + k * hidden_dim;
                for (int j = 0; j < hidden_dim; ++j)
                    sum += w_row[j] * hidden[j];
                float y_hat = 1.0f / (1.0f + expf(-sum));
                output[k] = y_hat;
                float yt = y_true[k];
                loss += -(yt * logf(y_hat + eps) + (1.0f - yt) * logf(1.0f - y_hat + eps));
                delta2[k] = (y_hat - yt);
                grad_b2[k] += delta2[k];
                for (int j = 0; j < hidden_dim; ++j)
                    grad_W2[k * hidden_dim + j] += delta2[k] * hidden[j];
            }

            for (int j = 0; j < hidden_dim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < output_dim; ++k)
                    sum += delta2[k] * W2[k * hidden_dim + j];
                float delta1 = sum * hidden[j] * (1.0f - hidden[j]);
                grad_b1[j] += delta1;
                for (int i = 0; i < input_dim; ++i)
                    grad_W1[j * input_dim + i] += delta1 * x[i];
            }
        }

        float inv_n = 1.0f / samples;
        for (int i = 0; i < hidden_dim * input_dim; ++i)
            grad_W1[i] *= inv_n;
        for (int i = 0; i < hidden_dim; ++i)
            grad_b1[i] *= inv_n;
        for (int i = 0; i < output_dim * hidden_dim; ++i)
            grad_W2[i] *= inv_n;
        for (int i = 0; i < output_dim; ++i)
            grad_b2[i] *= inv_n;

        for (int i = 0; i < hidden_dim * input_dim; ++i)
            W1[i] -= opts->lr * grad_W1[i];
        for (int i = 0; i < hidden_dim; ++i)
            b1[i] -= opts->lr * grad_b1[i];
        for (int i = 0; i < output_dim * hidden_dim; ++i)
            W2[i] -= opts->lr * grad_W2[i];
        for (int i = 0; i < output_dim; ++i)
            b2[i] -= opts->lr * grad_b2[i];

        loss /= samples;
        int report_step = opts->epochs / 10;
        if (report_step < 1) report_step = 1;
        if (epoch % report_step == 0 || epoch == 1) {
            printf("[%d/%d] loss=%.4f\n", epoch, opts->epochs, loss);
        }
    }

    save_weights(opts->out_path, input_dim, hidden_dim, output_dim, W1, b1, W2, b2);

    free(W1); free(b1); free(W2); free(b2);
    free(grad_W1); free(grad_b1); free(grad_W2); free(grad_b2);
    free(hidden); free(output);
}

static void parse_args(int argc, char **argv, TrainOptions *opts) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            opts->data_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            opts->out_path = argv[++i];
        } else if (strcmp(argv[i], "--hidden") == 0 && i + 1 < argc) {
            opts->hidden_dim = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            opts->epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc) {
            opts->lr = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            opts->threshold = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--data path] [--hidden N] [--epochs N] [--lr X] [--threshold X] [--out fichier]\n", argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Argument inconnu: %s\n", argv[i]);
            printf("Utilisation: %s [options]\n", argv[0]);
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
    TrainOptions opts;
    init_default_options(&opts);
    parse_args(argc, argv, &opts);

    srand(42);

    Dataset ds;
    if (load_dataset(&opts, &ds) != 0) {
        return 1;
    }

    printf("Dataset: %d images, taille tuile=%dx%d, input_dim=%d\n",
           ds.count, ds.width, ds.height, ds.input_dim);
    train_network(&ds, &opts);
    free_dataset(&ds);
    return 0;
}

