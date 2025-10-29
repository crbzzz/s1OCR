#include "ocr_window.h"
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

};

G_DEFINE_TYPE(OcrAppWindow, ocr_app_window, GTK_TYPE_APPLICATION_WINDOW)

/* --------- Helpers CSS --------- */
static void load_app_css(GtkWidget *root) {
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
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* --------- Statut --------- */
static void update_status_label(OcrAppWindow *self, const gchar *message) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));
    if (!self->status_label) return;
    gtk_label_set_text(GTK_LABEL(self->status_label), message ? message : "Aucune image chargée.");
}


/* --------- Chargement image (sauvegarde immédiate dans out/) --------- */
static void load_image_from_path(OcrAppWindow *self, const gchar *filepath) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));
    if (filepath == NULL || *filepath == '\0') {
        update_status_label(self, "Chemin d'image invalide.");
        return;
    }

    /* Affiche l'image */
    gtk_image_set_from_file(GTK_IMAGE(self->image_widget), filepath);

    /* Mémorise le chemin pour nommer les futurs fichiers */
    g_object_set_data_full(G_OBJECT(self->image_widget),
                           "current-filepath",
                           g_strdup(filepath),
                           g_free);

    /* Statut */
    gchar *basename = g_path_get_basename(filepath);
    gchar *message = g_strdup_printf("Image chargée : %s", basename ? basename : filepath);
    update_status_label(self, message);
    g_free(message);

    /* === Sauvegarde immédiate de la copie dans out/ ===
       (le dossier out/ existe déjà selon toi) */
    GdkPixbuf *pix = gtk_image_get_pixbuf(GTK_IMAGE(self->image_widget));
    if (pix) {
        /* base sans extension */
        gchar *base_noext = g_strdup(basename ? basename : "image");
        gchar *dot = base_noext ? strrchr(base_noext, '.') : NULL;
        if (dot && dot != base_noext) *dot = '\0';

        gchar *outpath_load = g_strdup_printf("out/%s.png", base_noext);

        /* supprime l'ancien fichier de même nom si besoin */
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



/* --------- File chooser --------- */
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
        } else { /* n==4 avec alpha */
            memcpy(dp, sp, (size_t)w*4);
        }
    }
    *out_w = w;
    *out_h = h;
    *out_stride = w*4;
    return buf;
}

/* Calcule la boîte englobante de la rotation autour du centre (en degrés) */
static void rotated_size(int sw, int sh, double deg, int *dw, int *dh) {
    double rad = deg * M_PI / 180.0;
    double s = fabs(sin(rad)), c = fabs(cos(rad));
    *dw = (int)floor(sw * c + sh * s + 0.5);
    *dh = (int)floor(sw * s + sh * c + 0.5);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Rotation NN autour du centre, avec PAD auto uniquement si nécessaire.
 * min_w/min_h = dimensions minimales à respecter (mets la taille d'origine).
 * Sort: out_w/out_h.
 */
static unsigned char* rotate_rgba_nn_padauto(const unsigned char *src, int sw, int sh,
                                             double deg, int min_w, int min_h,
                                             int *out_w, int *out_h) {
    if (!src || sw <= 0 || sh <= 0 || !out_w || !out_h) return NULL;

    double a = fmod(deg, 360.0);
    if (a < 0) a += 360.0;

    double rad = a * M_PI / 180.0;
    double s = fabs(sin(rad)), c = fabs(cos(rad));

    /* boîte englobante de la rotation */
    int bw = (int)floor(sw * c + sh * s + 0.5);
    int bh = (int)floor(sw * s + sh * c + 0.5);

    /* On garde au moins la taille d’origine */
    int dw = bw < min_w ? min_w : bw;
    int dh = bh < min_h ? min_h : bh;

    *out_w = dw;
    *out_h = dh;

    unsigned char *dst = (unsigned char*)malloc((size_t)dw*(size_t)dh*4);
    if (!dst) return NULL;
    memset(dst, 0, (size_t)dw*(size_t)dh*4); /* fond transparent */

    /* centres (coord. centre de pixel) */
    double cx_s = sw * 0.5;
    double cy_s = sh * 0.5;
    double cx_d = dw * 0.5;
    double cy_d = dh * 0.5;

    /* rotation INVERSE pour remonter vers la source */
    double cosr = cos(-rad);
    double sinr = sin(-rad);

    /* cas angle ~0: copie au milieu (pas d’agrandissement inutile) */
    if (fabs(a) < 1e-9 || fabs(a - 360.0) < 1e-9) {
        /* on place juste la source centrée dans la destination (si dw/dh > sw/sh) */
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
            /* centre du pixel destination, par rapport au centre dest */
            double xdc = (xd + 0.5) - cx_d;
            double ydc = (yd + 0.5) - cy_d;

            /* coordonnées sources continues (rotation inverse autour du centre source) */
            double xs =  xdc * cosr - ydc * sinr + cx_s;
            double ys =  xdc * sinr + ydc * cosr + cy_s;

            /* NN */
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

/* --- Prototypes de helpers utilisés plus bas --- */
static void center_scroller(GtkWidget *scroller);


/* --------- Rotation --------- */
static void rotate_image(OcrAppWindow *self, gdouble angle_degrees) {
    g_return_if_fail(OCR_IS_APP_WINDOW(self));

    GdkPixbuf *pix = gtk_image_get_pixbuf(GTK_IMAGE(self->image_widget));
    if (!pix) {
        update_status_label(self, "Aucune image à faire pivoter.");
        return;
    }

    int sw, sh, sstride;
    unsigned char *src_rgba = pixbuf_to_rgba(pix, &sw, &sh, &sstride);
    if (!src_rgba) {
        update_status_label(self, "Impossible de lire les pixels.");
        return;
    }

    int dw = 0, dh = 0;
    unsigned char *rot = rotate_rgba_nn_padauto(src_rgba, sw, sh, angle_degrees,
                                                sw, sh, &dw, &dh);
    free(src_rgba);

    if (!rot) {
        update_status_label(self, "Échec de la rotation.");
        return;
    }

    /* Crée un pixbuf à la nouvelle taille (agrandit uniquement si nécessaire) */
    GdkPixbuf *view = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, dw, dh);
    if (!view) {
        free(rot);
        update_status_label(self, "Échec d’allocation pour l’affichage.");
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

    /* Recentre l’affichage dans le scroller */
    center_scroller(self->scroller);

    /* Statut */
    gchar *msg = g_strdup_printf("Rotation appliquée : %.1f° (affiché %dx%d)", angle_degrees, dw, dh);
    update_status_label(self, msg);
    g_free(msg);

    /* === Vider out/ puis sauvegarder UNIQUEMENT la rotatée === */
    clear_directory_contents("out");

    const gchar *inpath = g_object_get_data(G_OBJECT(self->image_widget), "current-filepath");
    gchar *base = inpath ? g_path_get_basename(inpath) : g_strdup("image");
    gchar *dot = base ? strrchr(base, '.') : NULL;
    if (dot && dot != base) *dot = '\0';

    gchar *angle_str = g_strdup_printf("%.1f", angle_degrees);
    for (char *p = angle_str; *p; ++p) if (*p == ',') *p = '_';
    gchar *outpath = g_strdup_printf("out/%s_rot%s.png", base, angle_str);

    GError *save_err = NULL;
    if (!gdk_pixbuf_save(view, outpath, "png", &save_err, NULL)) {
        gchar *emsg = g_strdup_printf("Rotation OK mais échec sauvegarde: %s",
                                      save_err ? save_err->message : "inconnue");
        update_status_label(self, emsg);
        g_clear_error(&save_err);
        g_free(emsg);
    } else {
        gchar *okmsg = g_strdup_printf("Enregistré: %s", outpath);
        update_status_label(self, okmsg);
        g_free(okmsg);
    }

    g_free(outpath);
    g_free(angle_str);
    g_free(base);
}







static void on_rotate_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);
    gdouble angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(self->angle_spin));
    rotate_image(self, angle);
}

/* --------- Navigation Home -> Main --------- */
static void on_enter_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OcrAppWindow *self = OCR_APP_WINDOW(user_data);
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), "main");
}

/* --------- Header + contrôles --------- */
static GtkWidget* build_header_bar(OcrAppWindow *self) {
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Projet OCR");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), "Sélectionnez une image pour démarrer");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);

    GtkWidget *open_button = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(open_button, "Ouvrir une image");
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_image), self);
    gtk_style_context_add_class(gtk_widget_get_style_context(open_button), "accent-btn");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), open_button);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *spin = gtk_spin_button_new_with_range(0.0, 360.0, 1.0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0.0);
    gtk_widget_set_tooltip_text(spin, "Angle de rotation");

    GtkWidget *rot_btn = gtk_button_new_from_icon_name("object-rotate-right", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(rot_btn, "Appliquer la rotation");
    gtk_style_context_add_class(gtk_widget_get_style_context(rot_btn), "accent-btn");
    g_signal_connect(rot_btn, "clicked", G_CALLBACK(on_rotate_clicked), self);

    gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), rot_btn, FALSE, FALSE, 0);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), box);
    self->angle_spin = spin;
    return header_bar;
}

/* --------- Page principale --------- */
static GtkWidget* build_main_page(OcrAppWindow *self) {
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(content_box), 12);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);

    GtkWidget *image = gtk_image_new_from_icon_name("image-x-generic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(scroller), image);

    GtkWidget *status = gtk_label_new("Aucune image chargée.");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "status");

    gtk_box_pack_start(GTK_BOX(content_box), scroller, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), status, FALSE, FALSE, 0);

    self->scroller = scroller;

    self->image_widget = image;
    self->status_label = status;

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


/* --------- Page d’accueil avec image background du dossier --------- */
static GtkWidget* build_home_page(OcrAppWindow *self) {
    GtkWidget *overlay = gtk_overlay_new();

    // Recherche du fichier background.{jpg,png}
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

    // Le fond
    gtk_container_add(GTK_CONTAINER(overlay), bg);

    // --- Boîte principale pour le texte et le bouton ---
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_END);  // ✅ aligné vers le bas
    gtk_widget_set_margin_bottom(center_box, 120);     // ✅ marge pour le remonter légèrement du bord

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(panel), "home-dim");

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>Projet OCR</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);

    GtkWidget *subtitle = gtk_label_new("Analyse d’images et reconnaissance de texte");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);

    GtkWidget *enter_btn = gtk_button_new_with_label("Entrer");
    gtk_widget_set_tooltip_text(enter_btn, "Accéder à l’application");
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



/* --------- Initialisation --------- */
static void ocr_app_window_class_init(OcrAppWindowClass *klass) {
    (void)klass;
}

static void ocr_app_window_init(OcrAppWindow *self) {
    gtk_window_set_default_size(GTK_WINDOW(self), 1024, 768);
    gtk_window_set_position(GTK_WINDOW(self), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(self), "applications-graphics");
    gtk_window_set_title(GTK_WINDOW(self), "Projet OCR");

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


/* --------- Constructeur --------- */
OcrAppWindow *ocr_app_window_new(GtkApplication *application) {
    g_return_val_if_fail(GTK_IS_APPLICATION(application), NULL);
    return g_object_new(OCR_TYPE_APP_WINDOW, "application", application, NULL);
}