#include "nn_ocr.h"
#include "../binary/binary_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <glib.h>

/* Neural Network OCR - Letter recognition (A-Z)
 * Architecture: 784 (28x28 image) -> 128 -> 26 (letters)
 */

#define INPUT_SIZE 784
#define HIDDEN_SIZE 128
#define OUTPUT_SIZE 26  /* A-Z */

static float *W1 = NULL;    /* [INPUT_SIZE * HIDDEN_SIZE] */
static float *b1 = NULL;    /* [HIDDEN_SIZE] */
static float *W2 = NULL;    /* [HIDDEN_SIZE * OUTPUT_SIZE] */
static float *b2 = NULL;    /* [OUTPUT_SIZE] */

static float *h = NULL;     /* hidden layer activation */
static float *z2 = NULL;    /* output layer pre-activation */
static float *a2 = NULL;    /* output layer softmax */

int nn_init(const char *weights_path) {
    if (!weights_path) return 0;
    
    /* Allocate memory */
    W1 = (float *)malloc(INPUT_SIZE * HIDDEN_SIZE * sizeof(float));
    b1 = (float *)malloc(HIDDEN_SIZE * sizeof(float));
    W2 = (float *)malloc(HIDDEN_SIZE * OUTPUT_SIZE * sizeof(float));
    b2 = (float *)malloc(OUTPUT_SIZE * sizeof(float));
    h = (float *)malloc(HIDDEN_SIZE * sizeof(float));
    z2 = (float *)malloc(OUTPUT_SIZE * sizeof(float));
    a2 = (float *)malloc(OUTPUT_SIZE * sizeof(float));
    
    if (!W1 || !b1 || !W2 || !b2 || !h || !z2 || !a2) {
        return 0;
    }
    
    /* Load weights from file */
    FILE *f = fopen(weights_path, "r");
    if (!f) {
        /* Initialize with random weights if file not found */
        for (int i = 0; i < INPUT_SIZE * HIDDEN_SIZE; i++) {
            W1[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            b1[i] = 0.0f;
        }
        for (int i = 0; i < HIDDEN_SIZE * OUTPUT_SIZE; i++) {
            W2[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        for (int i = 0; i < OUTPUT_SIZE; i++) {
            b2[i] = 0.0f;
        }
        return 1;
    }
    
    /* Read weights */
    for (int i = 0; i < INPUT_SIZE * HIDDEN_SIZE; i++) {
        if (fscanf(f, "%f", &W1[i]) != 1) {
            fclose(f);
            return 0;
        }
    }
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        if (fscanf(f, "%f", &b1[i]) != 1) {
            fclose(f);
            return 0;
        }
    }
    for (int i = 0; i < HIDDEN_SIZE * OUTPUT_SIZE; i++) {
        if (fscanf(f, "%f", &W2[i]) != 1) {
            fclose(f);
            return 0;
        }
    }
    for (int i = 0; i < OUTPUT_SIZE; i++) {
        if (fscanf(f, "%f", &b2[i]) != 1) {
            fclose(f);
            return 0;
        }
    }
    
    fclose(f);
    return 1;
}

static float __attribute__((unused)) relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

static void __attribute__((unused)) softmax(float *logits, float *probs, int size) {
    float max_val = logits[0];
    for (int i = 1; i < size; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        probs[i] = expf(logits[i] - max_val);
        sum += probs[i];
    }
    
    for (int i = 0; i < size; i++) {
        probs[i] /= sum;
    }
}

static void __attribute__((unused)) forward_pass(const float *input) {
    /* Hidden layer: input (784) -> hidden (128) with ReLU */
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        float z = b1[i];
        for (int j = 0; j < INPUT_SIZE; j++) {
            z += W1[i * INPUT_SIZE + j] * input[j];
        }
        h[i] = relu(z);
    }
    
    /* Output layer: hidden (128) -> output (26) with softmax */
    for (int i = 0; i < OUTPUT_SIZE; i++) {
        float z = b2[i];
        for (int j = 0; j < HIDDEN_SIZE; j++) {
            z += W2[i * HIDDEN_SIZE + j] * h[j];
        }
        z2[i] = z;
    }
    
    softmax(z2, a2, OUTPUT_SIZE);
}

/* Predict with data augmentation for robustness */
static char predict_with_augmentation(const unsigned char *img_data) {
    int vote[26] = {0};
    int predictions = 0;
    
    /* Base prediction */
    {
        float input[INPUT_SIZE];
        for (int i = 0; i < INPUT_SIZE; i++) {
            input[i] = (float)img_data[i] / 255.0f;
        }
        forward_pass(input);
        
        int best_idx = 0;
        float best_val = a2[0];
        for (int i = 1; i < OUTPUT_SIZE; i++) {
            if (a2[i] > best_val) {
                best_val = a2[i];
                best_idx = i;
            }
        }
        if (best_val > 0.1f) {
            vote[best_idx] += 2;  /* Weight: base prediction counts twice */
            predictions++;
        }
    }
    
    /* Small horizontal shift augmentations */
    for (int dx = -1; dx <= 1; dx += 2) {
        unsigned char *shifted = (unsigned char *)malloc(28 * 28);
        if (!shifted) continue;
        
        memset(shifted, 0, 28 * 28);
        for (int y = 0; y < 28; y++) {
            for (int x = 0; x < 28; x++) {
                int sx = x + dx;
                if (sx >= 0 && sx < 28) {
                    shifted[y * 28 + x] = img_data[y * 28 + sx];
                }
            }
        }
        
        float input[INPUT_SIZE];
        for (int i = 0; i < INPUT_SIZE; i++) {
            input[i] = (float)shifted[i] / 255.0f;
        }
        forward_pass(input);
        
        int best_idx = 0;
        float best_val = a2[0];
        for (int i = 1; i < OUTPUT_SIZE; i++) {
            if (a2[i] > best_val) {
                best_val = a2[i];
                best_idx = i;
            }
        }
        if (best_val > 0.1f) {
            vote[best_idx]++;
            predictions++;
        }
        
        free(shifted);
    }
    
    /* Small vertical shift augmentations */
    for (int dy = -1; dy <= 1; dy += 2) {
        unsigned char *shifted = (unsigned char *)malloc(28 * 28);
        if (!shifted) continue;
        
        memset(shifted, 0, 28 * 28);
        for (int y = 0; y < 28; y++) {
            for (int x = 0; x < 28; x++) {
                int sy = y + dy;
                if (sy >= 0 && sy < 28) {
                    shifted[y * 28 + x] = img_data[sy * 28 + x];
                }
            }
        }
        
        float input[INPUT_SIZE];
        for (int i = 0; i < INPUT_SIZE; i++) {
            input[i] = (float)shifted[i] / 255.0f;
        }
        forward_pass(input);
        
        int best_idx = 0;
        float best_val = a2[0];
        for (int i = 1; i < OUTPUT_SIZE; i++) {
            if (a2[i] > best_val) {
                best_val = a2[i];
                best_idx = i;
            }
        }
        if (best_val > 0.1f) {
            vote[best_idx]++;
            predictions++;
        }
        
        free(shifted);
    }
    
    /* Find winner by vote */
    int best_letter = -1;
    int best_votes = 0;
    for (int i = 0; i < 26; i++) {
        if (vote[i] > best_votes) {
            best_votes = vote[i];
            best_letter = i;
        }
    }
    
    if (best_letter >= 0 && best_votes > 0) {
        return 'A' + best_letter;
    }
    
    return '?';
}

char nn_predict_letter_from_file(const char *png_path) {
    if (!png_path) return '?';
    
    BinaryImage *img = binary_load_grayscale(png_path);
    if (!img) return '?';
    
    char result = '?';
    
    if (img->width == 28 && img->height == 28) {
        /* Direct prediction with augmentation */
        result = predict_with_augmentation(img->data);
    } else {
        /* Resize to 28x28 using nearest neighbor */
        unsigned char *resized = (unsigned char *)malloc(28 * 28);
        if (resized) {
            for (int y = 0; y < 28; y++) {
                for (int x = 0; x < 28; x++) {
                    int sy = (y * img->height) / 28;
                    int sx = (x * img->width) / 28;
                    if (sy >= img->height) sy = img->height - 1;
                    if (sx >= img->width) sx = img->width - 1;
                    resized[y * 28 + x] = img->data[sy * img->width + sx];
                }
            }
            result = predict_with_augmentation(resized);
            free(resized);
        }
    }
    
    binary_free(img);
    return result;
}

int nn_process_grid(const char *letters_dir, 
                    const char *grille_path, 
                    const char *mots_path) {
    if (!letters_dir || !grille_path || !mots_path) return 0;
    
    /* Scan directory for letter files and sort by index */
    DIR *dir = opendir(letters_dir);
    if (!dir) return 0;
    
    typedef struct {
        int idx;
        char filename[256];
    } FileEntry;
    
    FileEntry *files = (FileEntry *)malloc(1000 * sizeof(FileEntry));
    int file_count = 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_count < 1000) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".png")) {
            int idx;
            if (sscanf(entry->d_name, "letter_%d.png", &idx) == 1) {
                files[file_count].idx = idx;
                strncpy(files[file_count].filename, entry->d_name, sizeof(files[file_count].filename) - 1);
                file_count++;
            }
        }
    }
    closedir(dir);
    
    if (file_count == 0) {
        free(files);
        return 0;
    }
    
    /* Sort files by index */
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = i + 1; j < file_count; j++) {
            if (files[i].idx > files[j].idx) {
                FileEntry temp = files[i];
                files[i] = files[j];
                files[j] = temp;
            }
        }
    }
    
    /* Process each letter with voting */
    GString *grid_text = g_string_new("");
    FILE *f_grille = fopen(grille_path, "w");
    FILE *f_mots = fopen(mots_path, "w");
    
    if (!f_grille || !f_mots) {
        if (f_grille) fclose(f_grille);
        if (f_mots) fclose(f_mots);
        g_string_free(grid_text, TRUE);
        free(files);
        return 0;
    }
    
    int processed = 0;
    for (int i = 0; i < file_count; i++) {
        char letter_path[512];
        snprintf(letter_path, sizeof(letter_path), "%s/%s", letters_dir, files[i].filename);
        
        /* Predict letter with voting (5 augmented predictions) */
        int vote_count[26] = {0};
        for (int attempt = 0; attempt < 5; attempt++) {
            char pred = nn_predict_letter_from_file(letter_path);
            if (pred >= 'A' && pred <= 'Z') {
                vote_count[pred - 'A']++;
            }
        }
        
        /* Find winner */
        char predicted = '?';
        int max_votes = 0;
        for (int j = 0; j < 26; j++) {
            if (vote_count[j] > max_votes) {
                max_votes = vote_count[j];
                predicted = 'A' + j;
            }
        }
        
        if (predicted != '?' && max_votes >= 2) {
            g_string_append_c(grid_text, predicted);
            fprintf(f_grille, "%c ", predicted);
            processed++;
        } else {
            g_string_append_c(grid_text, '?');
            fprintf(f_grille, "? ");
        }
    }
    
    fprintf(f_grille, "\n");
    g_string_append_c(grid_text, '\n');
    
    /* Write grid without spaces to mots file */
    fprintf(f_mots, "%s", grid_text->str);
    
    fclose(f_grille);
    fclose(f_mots);
    
    g_string_free(grid_text, TRUE);
    free(files);
    return 1;
}

void nn_shutdown(void) {
    if (W1) {
        free(W1);
        W1 = NULL;
    }
    if (b1) {
        free(b1);
        b1 = NULL;
    }
    if (W2) {
        free(W2);
        W2 = NULL;
    }
    if (b2) {
        free(b2);
        b2 = NULL;
    }
    if (h) {
        free(h);
        h = NULL;
    }
    if (z2) {
        free(z2);
        z2 = NULL;
    }
    if (a2) {
        free(a2);
        a2 = NULL;
    }
}


