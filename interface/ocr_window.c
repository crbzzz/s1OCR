#include "ocr_window.h"
#include "../nn/nn_ocr.h"
#include "../binary/binary_api.h"
#include "../solver/solver.h"
#include <glib/gstdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

static gboolean remove_path_recursive(const gchar *path) {
    if (!path || !*path) return TRUE;
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        GDir *dir = g_dir_open(path, 0, NULL);
        if (!dir) return (g_rmdir(path) == 0);
        const gchar *name;
        while ((name = g_dir_read_name(dir)) != NULL) {
            gchar *child = g_build_filename(path, name, NULL);
            remove_path_recursive(child);
            g_free(child);
        }
        g_dir_close(dir);
        return (g_rmdir(path) == 0);
    } else {
        return (g_remove(path) == 0);
    }
}

/* Unused: reserved for future cleanup functionality */
__attribute__((unused))
static void clear_directory_contents(const gchar *dirpath) {
    if (!dirpath || !*dirpath) return;
    if (!g_file_test(dirpath, G_FILE_TEST_IS_DIR)) return;

    GDir *dir = g_dir_open(dirpath, 0, NULL);
    if (!dir) return;

    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *child = g_build_filename(dirpath, name, NULL);
        remove_path_recursive(child);
        g_free(child);
    }
    g_dir_close(dir);
}

typedef struct _OcrAppWindowPrivate OcrAppWindowPrivate;

struct _OcrAppWindow {
    GtkApplicationWindow parent_instance;

    GtkWidget *stack;
    GtkWidget *image_widget;
    GtkWidget *status_label;
    GtkWidget *angle_spin;
    GtkWidget *scroller;
    
    /* Rotation and auto-rotation */
    GPtrArray *samples;
    guint auto_rotate_timeout;
    int sample_index;
    gdouble auto_angle;
    gboolean auto_rotation_enabled;
    GtkWidget *auto_rotate_btn;
    
    /* Binary image processing */
    BinaryImage *current_binary;
    
    /* OCR and grid solving */
    GtkWidget *grid_text_view;
    GtkWidget *words_text_view;
    GtkWidget *results_text_view;
    gchar *current_grid;
    unsigned int grid_rows, grid_cols;

};

G_DEFINE_TYPE(OcrAppWindow, ocr_app_window, GTK_TYPE_APPLICATION_WINDOW)

static void load_app_css(GtkWidget *window) {
    const gchar *css =
        "window { background: #0b0f14; }\n"
        ".header-title { font-weight: 700; }\n"
        ".accent-btn { border-radius: 12px; padding: 8px 14px; font-weight: 600; }\n"
        ".hero-enter { "
        "   font-size: 22px; "
        "   padding: 14px 28px; "
        "   border-radius: 16px; "
        "   color: white; "
        "   background: rgba(255,255,255,0.15); "
        "   border: 1px solid rgba(255,255,255,0.3); }\n"
        ".hero-enter:hover { background: rgba(255,255,255,0.25); }\n"
        ".home-dim { "
        "   background: rgba(0,0,0,0.40); "
        "   border-radius: 16px; "
        "   padding: 18px; }\n"
        ".home-dim label { color: white; }\n"
        ".status { color: #e0e0e0; }\n";

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gtk_widget_get_screen(window),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void update_status_label(OcrAppWindow *self, const gchar *message) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));
    if (!self->status_label) return;
    gtk_label_set_text(GTK_LABEL(self->status_label), message ? message : "Aucune image chargée.");
}

static void load_image_from_path(OcrAppWindow *self, const gchar *filepath) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));
    if (filepath == NULL || *filepath == '\0') {
        update_status_label(self, "Chemin d'image invalide.");
        return;
    }

    gtk_image_set_from_file(GTK_IMAGE(self->image_widget), filepath);

    g_object_set_data_full(G_OBJECT(self->image_widget),
                           "current-filepath",
                           g_strdup(filepath),
                           g_free);

    gchar *basename = g_path_get_basename(filepath);
    gchar *message = g_strdup_printf("Image chargée : %s", basename ? basename : filepath);
    update_status_label(self, message);
    g_free(message);

    GdkPixbuf *pix = gtk_image_get_pixbuf(GTK_IMAGE(self->image_widget));
    if (pix) {
        gchar *base_noext = g_strdup(basename ? basename : "image");
        gchar *dot = base_noext ? strrchr(base_noext, '.') : NULL;
        if (dot && dot != base_noext) *dot = '\0';

        gchar *outpath_load = g_strdup_printf("out/%s.png", base_noext);

        if (g_file_test(outpath_load, G_FILE_TEST_EXISTS)) {
            g_remove(outpath_load);
        }

        GError *save_err = NULL;
        if (!gdk_pixbuf_save(pix, outpath_load, "png", &save_err, NULL)) {
            g_warning("Échec sauvegarde copie de chargement: %s",
                      save_err ? save_err->message : "inconnue");
            g_clear_error(&save_err);
        }

        g_free(outpath_load);
        g_free(base_noext);
    }

    g_free(basename);
}

static void on_open_image(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        "Sélectionner une image",
        GTK_WINDOW(self),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Ouvrir",
        "Annuler");

    GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);

    GtkFileFilter *image_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(image_filter, "Images (PNG, JPEG)");
    gtk_file_filter_add_mime_type(image_filter, "image/png");
    gtk_file_filter_add_mime_type(image_filter, "image/jpeg");
    gtk_file_filter_add_pattern(image_filter, "*.png");
    gtk_file_filter_add_pattern(image_filter, "*.jpg");
    gtk_file_filter_add_pattern(image_filter, "*.jpeg");
    gtk_file_chooser_add_filter(chooser, image_filter);

    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "Tous les fichiers");
    gtk_file_filter_add_pattern(all_filter, "*.*");
    gtk_file_chooser_add_filter(chooser, all_filter);

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        gchar *filepath = gtk_file_chooser_get_filename(chooser);
        load_image_from_path(self, filepath);
        g_free(filepath);
    }

    g_object_unref(native);
}

static unsigned char* pixbuf_to_rgba(const GdkPixbuf *pix, int *out_w, int *out_h, int *out_stride) {
    if (!pix) return NULL;
    int w = gdk_pixbuf_get_width(pix);
    int h = gdk_pixbuf_get_height(pix);
    int n = gdk_pixbuf_get_n_channels(pix);
    int rs = gdk_pixbuf_get_rowstride(pix);
    gboolean has_alpha = gdk_pixbuf_get_has_alpha(pix);
    const guchar *src = gdk_pixbuf_read_pixels(pix);

    if (!src || w<=0 || h<=0) return NULL;

    unsigned char *buf = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!buf) return NULL;

    for (int y=0; y<h; ++y) {
        const unsigned char *sp = src + y*rs;
        unsigned char *dp = buf + (size_t)y * (size_t)w * 4;
        if (n == 3 || (n==4 && !has_alpha)) {
            for (int x=0; x<w; ++x) {
                dp[4*x+0] = sp[n*x+0];
                dp[4*x+1] = sp[n*x+1];
                dp[4*x+2] = sp[n*x+2];
                dp[4*x+3] = 255;
            }
        } else {
            memcpy(dp, sp, (size_t)w*4);
        }
    }
    *out_w = w;
    *out_h = h;
    *out_stride = w*4;
    return buf;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned char* rotate_rgba_nn_padauto(const unsigned char *src, int sw, int sh,
                                             double deg, int min_w, int min_h,
                                             int *out_w, int *out_h) {
    if (!src || sw <= 0 || sh <= 0 || !out_w || !out_h) return NULL;

    double a = fmod(deg, 360.0);
    if (a < 0) a += 360.0;

    double rad = a * M_PI / 180.0;
    double s = fabs(sin(rad)), c = fabs(cos(rad));

    int bw = (int)floor(sw * c + sh * s + 0.5);
    int bh = (int)floor(sw * s + sh * c + 0.5);

    int dw = bw < min_w ? min_w : bw;
    int dh = bh < min_h ? min_h : bh;

    *out_w = dw;
    *out_h = dh;

    unsigned char *dst = (unsigned char*)malloc((size_t)dw*(size_t)dh*4);
    if (!dst) return NULL;
    memset(dst, 0, (size_t)dw*(size_t)dh*4);

    double cx_s = sw * 0.5;
    double cy_s = sh * 0.5;
    double cx_d = dw * 0.5;
    double cy_d = dh * 0.5;

    double cosr = cos(-rad);
    double sinr = sin(-rad);

    if (fabs(a) < 1e-9 || fabs(a - 360.0) < 1e-9) {
        int xoff = (dw - sw) / 2;
        int yoff = (dh - sh) / 2;
        for (int y=0; y<sh; ++y) {
            memcpy(dst + (size_t)(y+yoff)*dw*4 + (size_t)(xoff)*4,
                   src + (size_t)y*sw*4,
                   (size_t)sw*4);
        }
        return dst;
    }

    for (int yd = 0; yd < dh; ++yd) {
        for (int xd = 0; xd < dw; ++xd) {
            double xdc = (xd + 0.5) - cx_d;
            double ydc = (yd + 0.5) - cy_d;

            double xs =  xdc * cosr - ydc * sinr + cx_s;
            double ys =  xdc * sinr + ydc * cosr + cy_s;

            int x0 = (int)floor(xs + 0.5);
            int y0 = (int)floor(ys + 0.5);

            if (x0 >= 0 && x0 < sw && y0 >= 0 && y0 < sh) {
                const unsigned char *sp = src + ((size_t)y0*(size_t)sw + (size_t)x0)*4;
                unsigned char *dp = dst + ((size_t)yd*(size_t)dw + (size_t)xd)*4;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=sp[3];
            }
        }
    }
    return dst;
}

static void center_scroller(GtkWidget *scroller);

static void rotate_image(OcrAppWindow *self, gdouble angle_degrees) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));

    GdkPixbuf *pix = gtk_image_get_pixbuf(GTK_IMAGE(self->image_widget));
    if (!pix) {
        update_status_label(self, "faire message err.");
        return;
    }

    int sw, sh, sstride;
    unsigned char *src_rgba = pixbuf_to_rgba(pix, &sw, &sh, &sstride);
    if (!src_rgba) {
        update_status_label(self, "erreur.");
        return;
    }

    int dw = 0, dh = 0;
    unsigned char *rot = rotate_rgba_nn_padauto(src_rgba, sw, sh, angle_degrees,
                                                sw, sh, &dw, &dh);
    free(src_rgba);

    if (!rot) {
        update_status_label(self, "Echec de rotation.");
        return;
    }

    GdkPixbuf *view = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, dw, dh);
    if (!view) {
        free(rot);
        update_status_label(self, "Pb mémoire.");
        return;
    }

    int dstride = gdk_pixbuf_get_rowstride(view);
    guchar *dp = gdk_pixbuf_get_pixels(view);
    for (int y = 0; y < dh; ++y) {
        memcpy(dp + (size_t)y * dstride, rot + (size_t)y * dw * 4, (size_t)dw * 4);
    }
    free(rot);

    gtk_image_set_from_pixbuf(GTK_IMAGE(self->image_widget), view);
    g_object_unref(view);

    center_scroller(self->scroller);

    gchar *msg = g_strdup_printf("Rotation faite : %.1f° (affiché %dx%d)", angle_degrees, dw, dh);
    update_status_label(self, msg);
    g_free(msg);
}

static void on_rotate_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);
    
    GdkPixbuf *pix = gtk_image_get_pixbuf(GTK_IMAGE(self->image_widget));
    if (!pix) {
        update_status_label(self, "Charger une image d'abord.");
        return;
    }

    gdouble angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(self->angle_spin));
    rotate_image(self, angle);
}

/* =========================== OCR Pipeline Callbacks =========================== */

static void on_binarize_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    const gchar *filepath = g_object_get_data(G_OBJECT(self->image_widget), "current-filepath");
    if (!filepath || *filepath == '\0') {
        update_status_label(self, "Veuillez d'abord charger une image.");
        return;
    }

    /* Load and binarize image */
    if (self->current_binary) {
        free(self->current_binary->data);
        free(self->current_binary);
    }

    self->current_binary = binary_load_otsu(filepath);
    if (!self->current_binary) {
        update_status_label(self, "Erreur: impossible de binariser l'image.");
        return;
    }

    /* Save binarized image for display */
    if (!binary_save_png(self->current_binary, "out/binarized.png")) {
        update_status_label(self, "Erreur: impossible de sauvegarder l'image binarisée.");
        return;
    }

    /* Display binarized result */
    gtk_image_set_from_file(GTK_IMAGE(self->image_widget), "out/binarized.png");
    center_scroller(self->scroller);

    gchar *msg = g_strdup_printf("✓ Binarisation réussie (%dx%d)", 
                                 self->current_binary->width, self->current_binary->height);
    update_status_label(self, msg);
    g_free(msg);
}

static void on_extract_grid_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    if (!self->current_binary) {
        update_status_label(self, "Veuillez d'abord binariser une image.");
        return;
    }

    /* Find connected components (letters) */
    ComponentList *components = binary_find_components(self->current_binary);
    if (!components || components->count == 0) {
        update_status_label(self, "Aucune composante détectée.");
        if (components) {
            free(components->boxes);
            free(components);
        }
        return;
    }

    /* Filter and sort components by position (top-left to bottom-right) */
    typedef struct {
        BoundingBox box;
        int original_idx;
    } ComponentInfo;
    
    ComponentInfo *filtered = (ComponentInfo *)malloc(components->count * sizeof(ComponentInfo));
    int filtered_count = 0;
    
    for (int i = 0; i < (int)components->count; i++) {
        BoundingBox box = components->boxes[i];
        int area = box.w * box.h;
        
        /* Keep components with reasonable size (filter noise and large regions) */
        /* Adjust thresholds based on typical letter size */
        if (box.w >= 4 && box.h >= 4 && box.w <= 300 && box.h <= 300 && area >= 20 && area <= 20000) {
            filtered[filtered_count].box = box;
            filtered[filtered_count].original_idx = i;
            filtered_count++;
        }
    }
    
    /* Sort by y then x (top-to-bottom, left-to-right) */
    for (int i = 0; i < filtered_count - 1; i++) {
        for (int j = i + 1; j < filtered_count; j++) {
            BoundingBox *a = &filtered[i].box;
            BoundingBox *b = &filtered[j].box;
            
            /* First sort by y, then by x */
            int ay = a->y + a->h / 2;  /* Center y */
            int by = b->y + b->h / 2;  /* Center y */
            int ax = a->x + a->w / 2;  /* Center x */
            int bx = b->x + b->w / 2;  /* Center x */
            
            if (ay > by || (ay == by && ax > bx)) {
                ComponentInfo temp = filtered[i];
                filtered[i] = filtered[j];
                filtered[j] = temp;
            }
        }
    }

    /* Create letters directory and extract individual letters */
    g_mkdir_with_parents("letters", 0755);

    for (int i = 0; i < filtered_count && i < 1000; i++) {
        BoundingBox box = filtered[i].box;

        /* Crop and resize to 28x28 for neural network */
        BinaryImage *cropped = binary_crop(self->current_binary, &box);
        if (!cropped) continue;

        /* Resize to 28x28 with aspect ratio preservation */
        int dest_size = 28;
        int max_dim = (cropped->width > cropped->height) ? cropped->width : cropped->height;
        float scale = (float)dest_size / max_dim;
        
        int scaled_w = (int)(cropped->width * scale);
        int scaled_h = (int)(cropped->height * scale);
        
        /* Ensure at least 1 pixel */
        if (scaled_w < 1) scaled_w = 1;
        if (scaled_h < 1) scaled_h = 1;
        
        unsigned char *scaled = (unsigned char *)calloc(dest_size * dest_size, sizeof(unsigned char));
        if (!scaled) {
            free(cropped->data);
            free(cropped);
            continue;
        }
        
        /* Resize with nearest neighbor */
        for (int y = 0; y < scaled_h; y++) {
            for (int x = 0; x < scaled_w; x++) {
                int sy = (y * cropped->height) / scaled_h;
                int sx = (x * cropped->width) / scaled_w;
                if (sy >= cropped->height) sy = cropped->height - 1;
                if (sx >= cropped->width) sx = cropped->width - 1;
                scaled[y * dest_size + x] = cropped->data[sy * cropped->width + sx];
            }
        }
        
        /* Center the scaled image in 28x28 */
        unsigned char *centered = (unsigned char *)calloc(dest_size * dest_size, sizeof(unsigned char));
        int offset_x = (dest_size - scaled_w) / 2;
        int offset_y = (dest_size - scaled_h) / 2;
        
        for (int y = 0; y < scaled_h; y++) {
            for (int x = 0; x < scaled_w; x++) {
                int dy = offset_y + y;
                int dx = offset_x + x;
                if (dy >= 0 && dy < dest_size && dx >= 0 && dx < dest_size) {
                    centered[dy * dest_size + dx] = scaled[y * dest_size + x];
                }
            }
        }
        
        /* Save letter image */
        BinaryImage img = { centered, dest_size, dest_size };
        gchar *letter_path = g_strdup_printf("letters/letter_%04d.png", i);
        binary_save_png(&img, letter_path);
        g_free(letter_path);

        free(scaled);
        free(centered);
        free(cropped->data);
        free(cropped);
    }

    gchar *msg = g_strdup_printf("✓ Grille extraite: %d lettres détectées et triées", filtered_count);
    update_status_label(self, msg);
    g_free(msg);

    free(filtered);
    free(components->boxes);
    free(components);
}

static void on_ocr_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    /* Initialize neural network if not already done */
    static int nn_initialized = 0;
    if (!nn_initialized) {
        /* Try to load weights, fallback to random if not found */
        nn_init("../nn/weights.txt");
        nn_initialized = 1;
    }

    /* Check if letters directory exists */
    if (!g_file_test("letters", G_FILE_TEST_IS_DIR)) {
        update_status_label(self, "Veuillez d'abord extraire une grille.");
        return;
    }

    /* Use nn_process_grid for better recognition */
    int success = nn_process_grid("letters", "/tmp/grille.txt", "/tmp/mots.txt");
    
    if (!success) {
        update_status_label(self, "Erreur lors du traitement OCR.");
        return;
    }

    /* Read results from temporary file */
    FILE *f = fopen("/tmp/mots.txt", "r");
    if (!f) {
        update_status_label(self, "Impossible de lire les résultats OCR.");
        return;
    }

    char buffer[2048] = {0};
    if (fgets(buffer, sizeof(buffer), f) == NULL) {
        fclose(f);
        update_status_label(self, "Erreur lors de la lecture des résultats OCR.");
        return;
    }
    fclose(f);

    /* Remove newline if present */
    int len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    /* Store result and display */
    g_free(self->current_grid);
    self->current_grid = g_strdup(buffer);

    /* Count letters */
    int letter_count = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] >= 'A' && buffer[i] <= 'Z') {
            letter_count++;
        }
    }

    gchar *msg = g_strdup_printf("✓ OCR terminé: %d lettres reconnues", letter_count);
    update_status_label(self, msg);
    g_free(msg);

    /* Display recognized text if we have a text view */
    if (self->grid_text_view) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->grid_text_view));
        gtk_text_buffer_set_text(buf, self->current_grid ? self->current_grid : "", -1);
    }
}

static void on_solve_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    if (!self->current_grid) {
        update_status_label(self, "Veuillez d'abord faire OCR sur une grille.");
        return;
    }

    /* Try to load words list */
    FILE *words_file = fopen("../solver/grid/words.txt", "r");
    if (!words_file) {
        update_status_label(self, "Fichier words.txt non trouvé.");
        return;
    }

    /* Determine grid dimensions (assume square for now) */
    int grid_len = strlen(self->current_grid);
    int grid_size = (int)sqrt((float)grid_len);
    if (grid_size * grid_size != grid_len) {
        grid_size = 0;
        /* Try to parse actual dimensions from grid layout */
        int rows = 0, cols = 0;
        const char *line_start = self->current_grid;
        while (*line_start != '\0') {
            if (cols == 0) {
                const char *end = strchr(line_start, '\n');
                if (!end) end = line_start + strlen(line_start);
                /* Count non-space chars */
                for (const char *p = line_start; p < end; p++) {
                    if (*p != ' ') cols++;
                }
            }
            rows++;
            if (*line_start == '\0') break;
            line_start = strchr(line_start, '\n');
            if (!line_start) break;
            line_start++;
        }
        if (cols == 0) cols = grid_len; /* Fallback */
        grid_size = cols;
    }

    /* Read and solve words */
    GString *results = g_string_new("");
    char word[256];
    int solved_count = 0;

    while (fgets(word, sizeof(word), words_file) != NULL) {
        /* Remove newline */
        int word_len = strlen(word);
        if (word_len > 0 && word[word_len - 1] == '\n') {
            word[word_len - 1] = '\0';
            word_len--;
        }
        if (word_len == 0) continue;

        /* Search for word in grid */
        Coord start = {0, 0}, end = {0, 0};
        if (search_word(self->current_grid, grid_size, grid_size, word, &start, &end) == 0) {
            g_string_append_printf(results, "✓ %s trouvé à (%u,%u)-(%u,%u)\n",
                                 word, start.x, start.y, end.x, end.y);
            solved_count++;
        }
    }

    fclose(words_file);

    if (solved_count == 0) {
        g_string_append(results, "Aucun mot trouvé dans la grille.");
    }

    /* Display results */
    if (self->results_text_view) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->results_text_view));
        gtk_text_buffer_set_text(buf, results->str, -1);
    }

    gchar *msg = g_strdup_printf("✓ Solveur: %d mots trouvés", solved_count);
    update_status_label(self, msg);
    g_free(msg);

    g_string_free(results, TRUE);
}

/* =========================== Auto-Rotation =========================== */

static gboolean on_auto_rotate_timeout(gpointer user_data) {
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);
    
    if (!self->auto_rotation_enabled) {
        self->auto_rotate_timeout = 0;
        return FALSE;
    }

    self->auto_angle += 5.0;
    if (self->auto_angle >= 360.0) {
        self->auto_angle = 0.0;
        self->sample_index++;
        
        /* Load next sample if available */
        if (self->sample_index < (int)self->samples->len) {
            gchar *next_sample = (gchar*)g_ptr_array_index(self->samples, self->sample_index);
            load_image_from_path(self, next_sample);
        } else {
            /* Loop back to first sample */
            self->sample_index = 0;
            if (self->samples->len > 0) {
                gchar *first = (gchar*)g_ptr_array_index(self->samples, 0);
                load_image_from_path(self, first);
            }
        }
    }

    rotate_image(self, self->auto_angle);
    
    /* Update button label */
    gtk_button_set_label(GTK_BUTTON(self->auto_rotate_btn), "⏸ Auto");

    return TRUE; /* Keep timeout active */
}

static void on_auto_rotate_toggled(GtkButton *button, gpointer user_data) {
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);

    self->auto_rotation_enabled = !self->auto_rotation_enabled;

    if (self->auto_rotation_enabled) {
        self->auto_angle = 0.0;
        gtk_button_set_label(GTK_BUTTON(button), "⏵ Auto");
        
        /* Start timer: 50ms per rotation step (360°/50 = ~7.2° per frame at 20fps) */
        if (self->auto_rotate_timeout == 0) {
            self->auto_rotate_timeout = g_timeout_add(50, on_auto_rotate_timeout, self);
        }
    } else {
        gtk_button_set_label(GTK_BUTTON(button), "▶ Auto");
        
        /* Stop timer */
        if (self->auto_rotate_timeout > 0) {
            g_source_remove(self->auto_rotate_timeout);
            self->auto_rotate_timeout = 0;
        }
    }
}

static void on_enter_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "main");
}

static GtkWidget* build_header_bar(OcrAppWindow *self) {
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Projet OCR");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), "Sélectionner une image");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);

    GtkWidget *open_button = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(open_button, "Ouvrir une image");
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_image), self);
    gtk_style_context_add_class(gtk_widget_get_style_context(open_button), "accent-btn");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), open_button);

    /* Rotation controls */
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    GtkWidget *spin = gtk_spin_button_new_with_range(0.0, 360.0, 1.0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0.0);
    gtk_widget_set_tooltip_text(spin, "Angle de rotation");

    GtkWidget *rot_btn = gtk_button_new_from_icon_name("object-rotate-right", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(rot_btn, "Appliquer rotation");
    gtk_style_context_add_class(gtk_widget_get_style_context(rot_btn), "accent-btn");
    g_signal_connect(rot_btn, "clicked", G_CALLBACK(on_rotate_clicked), self);

    /* Auto-rotation button */
    GtkWidget *auto_rot_btn = gtk_button_new_with_label("▶ Auto");
    gtk_widget_set_tooltip_text(auto_rot_btn, "Rotation auto 5° par 50ms");
    gtk_style_context_add_class(gtk_widget_get_style_context(auto_rot_btn), "accent-btn");
    g_signal_connect(auto_rot_btn, "clicked", G_CALLBACK(on_auto_rotate_toggled), self);
    self->auto_rotate_btn = auto_rot_btn;

    gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), rot_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), auto_rot_btn, FALSE, FALSE, 0);

    /* OCR Pipeline buttons */
    GtkWidget *pipeline_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    GtkWidget *bin_btn = gtk_button_new_with_label("⚫ Bin");
    gtk_widget_set_tooltip_text(bin_btn, "Binariser l'image (Otsu)");
    gtk_style_context_add_class(gtk_widget_get_style_context(bin_btn), "accent-btn");
    g_signal_connect(bin_btn, "clicked", G_CALLBACK(on_binarize_clicked), self);

    GtkWidget *ext_btn = gtk_button_new_with_label("📊 Extract");
    gtk_widget_set_tooltip_text(ext_btn, "Extraire composantes (grille)");
    gtk_style_context_add_class(gtk_widget_get_style_context(ext_btn), "accent-btn");
    g_signal_connect(ext_btn, "clicked", G_CALLBACK(on_extract_grid_clicked), self);

    GtkWidget *ocr_btn = gtk_button_new_with_label("🔤 OCR");
    gtk_widget_set_tooltip_text(ocr_btn, "Reconnaissance optique");
    gtk_style_context_add_class(gtk_widget_get_style_context(ocr_btn), "accent-btn");
    g_signal_connect(ocr_btn, "clicked", G_CALLBACK(on_ocr_clicked), self);

    GtkWidget *solve_btn = gtk_button_new_with_label("✓ Solve");
    gtk_widget_set_tooltip_text(solve_btn, "Résoudre la grille");
    gtk_style_context_add_class(gtk_widget_get_style_context(solve_btn), "accent-btn");
    g_signal_connect(solve_btn, "clicked", G_CALLBACK(on_solve_clicked), self);

    gtk_box_pack_start(GTK_BOX(pipeline_box), bin_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pipeline_box), ext_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pipeline_box), ocr_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pipeline_box), solve_btn, FALSE, FALSE, 0);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), pipeline_box);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), box);

    self->angle_spin = spin;
    return header_bar;
}

static GtkWidget* build_main_page(OcrAppWindow *self) {
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(content_box), 12);

    /* Image display area */
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);

    GtkWidget *image = gtk_image_new_from_icon_name("image-x-generic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(scroller), image);

    GtkWidget *status = gtk_label_new("Pas d'image chargee.");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "status");

    /* Results area: Paned with grid text and solver results */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);

    /* Left: OCR grid text */
    GtkWidget *grid_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(grid_scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *grid_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(grid_text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(grid_text_view), TRUE);
    gtk_container_add(GTK_CONTAINER(grid_scroller), grid_text_view);

    GtkWidget *grid_label = gtk_label_new("Grille (OCR):");
    GtkWidget *grid_frame = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(grid_frame), grid_label);
    gtk_container_add(GTK_CONTAINER(grid_frame), grid_scroller);

    /* Right: Solver results */
    GtkWidget *results_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *results_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(results_text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(results_text_view), TRUE);
    gtk_container_add(GTK_CONTAINER(results_scroller), results_text_view);

    GtkWidget *results_label = gtk_label_new("Résultats:");
    GtkWidget *results_frame = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(results_frame), results_label);
    gtk_container_add(GTK_CONTAINER(results_frame), results_scroller);

    gtk_paned_pack1(GTK_PANED(paned), grid_frame, TRUE, TRUE);
    gtk_paned_pack2(GTK_PANED(paned), results_frame, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(paned), 300);

    gtk_box_pack_start(GTK_BOX(content_box), scroller, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), paned, FALSE, FALSE, 0);

    self->scroller = scroller;
    self->image_widget = image;
    self->status_label = status;
    self->grid_text_view = grid_text_view;
    self->results_text_view = results_text_view;

    return content_box;
}

static void center_scroller(GtkWidget *scroller) {
    if (!scroller) return;
    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scroller));
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller));
    if (hadj) {
        gdouble center = (gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj)) / 2.0;
        if (center < 0) center = 0;
        gtk_adjustment_set_value(hadj, center);
    }
    if (vadj) {
        gdouble center = (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)) / 2.0;
        if (center < 0) center = 0;
        gtk_adjustment_set_value(vadj, center);
    }
}

static GtkWidget* build_home_page(OcrAppWindow *self) {
    GtkWidget *overlay = gtk_overlay_new();

    gchar *bg_path = NULL;
    if (g_file_test("background.jpg", G_FILE_TEST_EXISTS))
        bg_path = g_strdup("background.jpg");
    else if (g_file_test("background.png", G_FILE_TEST_EXISTS))
        bg_path = g_strdup("background.png");

    GtkWidget *bg;
    if (bg_path)
        bg = gtk_image_new_from_file(bg_path);
    else
        bg = gtk_image_new_from_icon_name("image-missing", GTK_ICON_SIZE_DIALOG);

    gtk_widget_set_halign(bg, GTK_ALIGN_FILL);
    gtk_widget_set_valign(bg, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(bg, TRUE);
    gtk_widget_set_vexpand(bg, TRUE);

    gtk_container_add(GTK_CONTAINER(overlay), bg);

    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(center_box, 120);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(panel), "home-dim");

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>Projet OCR</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);

    GtkWidget *subtitle = gtk_label_new("Projet OCR");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);

    GtkWidget *enter_btn = gtk_button_new_with_label("Entrer");
    gtk_widget_set_tooltip_text(enter_btn, "Accéder à l’interface");
    gtk_style_context_add_class(gtk_widget_get_style_context(enter_btn), "hero-enter");
    g_signal_connect(enter_btn, "clicked", G_CALLBACK(on_enter_clicked), self);

    gtk_box_pack_start(GTK_BOX(panel), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), enter_btn, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(center_box), panel, FALSE, FALSE, 0);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), center_box);

    g_free(bg_path);
    return overlay;
}

static void ocr_app_window_finalize(GObject *object);

static void ocr_app_window_class_init(OcrAppWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = ocr_app_window_finalize;
}

static void ocr_app_window_finalize(GObject *object) {
    OcrAppWindow *self = OCR_APP_WINDOW(object);

    /* Stop auto-rotation timer */
    if (self->auto_rotate_timeout > 0) {
        g_source_remove(self->auto_rotate_timeout);
        self->auto_rotate_timeout = 0;
    }

    /* Free binary image */
    if (self->current_binary) {
        free(self->current_binary->data);
        free(self->current_binary);
        self->current_binary = NULL;
    }

    /* Free grid text */
    if (self->current_grid) {
        g_free(self->current_grid);
        self->current_grid = NULL;
    }

    /* Free samples array */
    if (self->samples) {
        g_ptr_array_unref(self->samples);
        self->samples = NULL;
    }

    /* Shutdown neural network */
    nn_shutdown();

    G_OBJECT_CLASS(ocr_app_window_parent_class)->finalize(object);
}

static gboolean on_auto_rotate_timeout(gpointer user_data);

static void ocr_app_window_init(OcrAppWindow *self) {
    gtk_window_set_default_size(GTK_WINDOW(self), 1024, 768);
    gtk_window_set_position(GTK_WINDOW(self), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(self), "applications-graphics");
    gtk_window_set_title(GTK_WINDOW(self), "Projet OCR");

    /* Initialize new fields */
    self->current_binary = NULL;
    self->current_grid = NULL;
    self->grid_rows = 0;
    self->grid_cols = 0;
    self->samples = g_ptr_array_new_with_free_func(g_free);
    self->sample_index = 0;
    self->auto_angle = 0.0;
    self->auto_rotation_enabled = FALSE;
    self->auto_rotate_timeout = 0;
    self->auto_rotate_btn = NULL;
    self->grid_text_view = NULL;
    self->words_text_view = NULL;
    self->results_text_view = NULL;

    /* Initialize neural network (load weights) */
    nn_init("nn/weights.txt");

    load_app_css(GTK_WIDGET(self));

    GtkWidget *header_bar = build_header_bar(self);
    gtk_style_context_add_class(gtk_widget_get_style_context(header_bar), "header-title");
    gtk_window_set_titlebar(GTK_WINDOW(self), header_bar);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 300);

    GtkWidget *home = build_home_page(self);
    GtkWidget *main_page = build_main_page(self);

    gtk_stack_add_named(GTK_STACK(stack), home, "home");
    gtk_stack_add_named(GTK_STACK(stack), main_page, "main");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "home");

    gtk_container_add(GTK_CONTAINER(self), stack);
    self->stack = stack;
}

OcrAppWindow *ocr_app_window_new(GtkApplication *application) {
    g_return_val_if_fail(GTK_IS_APPLICATION(application), NULL);
    return g_object_new(OCR_TYPE_APP_WINDOW, "application", application, NULL);
}
