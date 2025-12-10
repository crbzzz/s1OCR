#include "binary_api.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Otsu's binarization method */
static int compute_otsu_threshold(const uint8_t *grayscale, int width, int height) {
    int hist[256] = {0};
    int total = width * height;
    
    /* Build histogram */
    for (int i = 0; i < total; i++) {
        hist[grayscale[i]]++;
    }
    
    /* Find threshold */
    float best_variance = -1.0f;
    int best_threshold = 0;
    
    int w0 = 0;  /* weight of background */
    int sum0 = 0;  /* sum of background */
    
    for (int t = 0; t < 256; t++) {
        w0 += hist[t];
        sum0 += t * hist[t];
        
        if (w0 == 0) continue;
        int w1 = total - w0;
        if (w1 == 0) break;
        
        int sum1 = 0;
        for (int i = t + 1; i < 256; i++) {
            sum1 += i * hist[i];
        }
        
        float mu0 = (float)sum0 / w0;
        float mu1 = (float)sum1 / w1;
        
        float variance = (float)w0 * (float)w1 * (mu0 - mu1) * (mu0 - mu1);
        
        if (variance > best_variance) {
            best_variance = variance;
            best_threshold = t;
        }
    }
    
    return best_threshold;
}

BinaryImage* binary_load_grayscale(const char *png_path) {
    if (!png_path) return NULL;
    
    int w, h, channels;
    unsigned char *data = stbi_load(png_path, &w, &h, &channels, 0);
    if (!data) return NULL;
    
    /* Convert to grayscale if needed */
    uint8_t *grayscale = (uint8_t *)malloc(w * h);
    if (!grayscale) {
        stbi_image_free(data);
        return NULL;
    }
    
    if (channels == 1) {
        memcpy(grayscale, data, w * h);
    } else if (channels >= 3) {
        for (int i = 0; i < w * h; i++) {
            grayscale[i] = (uint8_t)(0.299 * data[i * channels + 0] +
                                     0.587 * data[i * channels + 1] +
                                     0.114 * data[i * channels + 2]);
        }
    } else {
        memcpy(grayscale, data, w * h);
    }
    
    stbi_image_free(data);
    
    /* Create output structure */
    BinaryImage *result = (BinaryImage *)malloc(sizeof(BinaryImage));
    if (!result) {
        free(grayscale);
        return NULL;
    }
    
    result->data = grayscale;
    result->width = w;
    result->height = h;
    
    return result;
}

BinaryImage* binary_load_otsu(const char *png_path) {
    if (!png_path) return NULL;
    
    int w, h, channels;
    unsigned char *data = stbi_load(png_path, &w, &h, &channels, 0);
    if (!data) return NULL;
    
    /* Convert to grayscale if needed */
    uint8_t *grayscale = (uint8_t *)malloc(w * h);
    if (!grayscale) {
        stbi_image_free(data);
        return NULL;
    }
    
    if (channels == 1) {
        memcpy(grayscale, data, w * h);
    } else if (channels >= 3) {
        for (int i = 0; i < w * h; i++) {
            grayscale[i] = (uint8_t)(0.299 * data[i * channels + 0] +
                                     0.587 * data[i * channels + 1] +
                                     0.114 * data[i * channels + 2]);
        }
    } else {
        memcpy(grayscale, data, w * h);
    }
    
    stbi_image_free(data);
    
    /* Compute Otsu threshold */
    int threshold = compute_otsu_threshold(grayscale, w, h);
    
    /* Binarize */
    uint8_t *binary_data = (uint8_t *)malloc(w * h);
    if (!binary_data) {
        free(grayscale);
        return NULL;
    }
    
    for (int i = 0; i < w * h; i++) {
        binary_data[i] = (grayscale[i] > threshold) ? 255 : 0;
    }
    
    free(grayscale);
    
    /* Create output structure */
    BinaryImage *result = (BinaryImage *)malloc(sizeof(BinaryImage));
    if (!result) {
        free(binary_data);
        return NULL;
    }
    
    result->data = binary_data;
    result->width = w;
    result->height = h;
    
    return result;
}

void binary_free(BinaryImage *img) {
    if (!img) return;
    if (img->data) free(img->data);
    free(img);
}

int binary_save_png(const BinaryImage *img, const char *output_path) {
    if (!img || !img->data || !output_path) return 0;
    return stbi_write_png(output_path, img->width, img->height, 1, img->data, img->width);
}

/* Connected components labeling using iterative flood fill (queue-based) */
typedef struct {
    int x, y;
} Point;

static void flood_fill_iterative(uint8_t *visited, const uint8_t *binary_data, 
                                 int start_x, int start_y, int width, int height,
                                 int *min_x, int *max_x, int *min_y, int *max_y) {
    /* Use a simple queue to avoid stack overflow on large components */
    Point *queue = (Point *)malloc(width * height * sizeof(Point));
    if (!queue) return;
    
    int queue_start = 0, queue_end = 0;
    
    /* Initial point */
    queue[queue_end].x = start_x;
    queue[queue_end].y = start_y;
    queue_end++;
    visited[start_y * width + start_x] = 1;
    
    *min_x = start_x;
    *max_x = start_x;
    *min_y = start_y;
    *max_y = start_y;
    
    /* BFS using queue */
    while (queue_start < queue_end) {
        Point p = queue[queue_start++];
        int x = p.x;
        int y = p.y;
        
        /* Check 4 neighbors */
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height &&
                !visited[ny * width + nx] && binary_data[ny * width + nx] != 0) {
                
                visited[ny * width + nx] = 1;
                
                if (nx < *min_x) *min_x = nx;
                if (nx > *max_x) *max_x = nx;
                if (ny < *min_y) *min_y = ny;
                if (ny > *max_y) *max_y = ny;
                
                queue[queue_end].x = nx;
                queue[queue_end].y = ny;
                queue_end++;
            }
        }
    }
    
    free(queue);
}

ComponentList* binary_find_components(const BinaryImage *img) {
    if (!img || !img->data) return NULL;
    
    uint8_t *visited = (uint8_t *)calloc(img->width * img->height, sizeof(uint8_t));
    if (!visited) return NULL;
    
    ComponentList *result = (ComponentList *)malloc(sizeof(ComponentList));
    if (!result) {
        free(visited);
        return NULL;
    }
    
    result->boxes = (BoundingBox *)malloc(1000 * sizeof(BoundingBox)); /* Max 1000 components */
    result->count = 0;
    
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            if (!visited[y * img->width + x] && img->data[y * img->width + x] != 0) {
                int min_x = x, max_x = x, min_y = y, max_y = y;
                flood_fill_iterative(visited, img->data, x, y, img->width, img->height, 
                                    &min_x, &max_x, &min_y, &max_y);
                
                if (result->count < 1000) {
                    result->boxes[result->count].x = min_x;
                    result->boxes[result->count].y = min_y;
                    result->boxes[result->count].w = max_x - min_x + 1;
                    result->boxes[result->count].h = max_y - min_y + 1;
                    result->count++;
                }
            }
        }
    }
    
    free(visited);
    return result;
}

void binary_free_components(ComponentList *comp) {
    if (!comp) return;
    if (comp->boxes) free(comp->boxes);
    free(comp);
}

BinaryImage* binary_crop(const BinaryImage *img, const BoundingBox *box) {
    if (!img || !box) return NULL;
    
    int x = box->x;
    int y = box->y;
    int w = box->w;
    int h = box->h;
    
    /* Add padding around the letter (3 pixels) */
    int padding = 3;
    int padded_x = (x - padding < 0) ? 0 : (x - padding);
    int padded_y = (y - padding < 0) ? 0 : (y - padding);
    int padded_w = w + 2 * padding;
    int padded_h = h + 2 * padding;
    
    if (padded_x + padded_w > img->width) {
        padded_w = img->width - padded_x;
    }
    if (padded_y + padded_h > img->height) {
        padded_h = img->height - padded_y;
    }
    
    BinaryImage *result = (BinaryImage *)malloc(sizeof(BinaryImage));
    if (!result) return NULL;
    
    result->width = padded_w;
    result->height = padded_h;
    result->data = (uint8_t *)malloc(padded_w * padded_h);
    if (!result->data) {
        free(result);
        return NULL;
    }
    
    for (int row = 0; row < padded_h; row++) {
        memcpy(result->data + row * padded_w,
               img->data + (padded_y + row) * img->width + padded_x,
               padded_w);
    }
    
    return result;
}
