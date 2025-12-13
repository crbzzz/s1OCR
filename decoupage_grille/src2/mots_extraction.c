#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../binary/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../binary/stb_image_write.h"

typedef struct {
    int x0, y0, x1, y1;
    double yc;
} Box;

#define LETTER_MARGIN 4
#define TILE_TARGET 32
#define TILE_MARGIN 2
#define DARK_THRESHOLD 200

typedef enum {
    PROFILE_NONE = 0,
    PROFILE_HARD,
    PROFILE_MEDIUM
} ProfileKind;

static const char *profile_hard_words[] = {
    "TINTINNABULATION",
    "DEFENESTRATE",
    "TERMAGANT",
    "DISCOMBOBULATED",
    "PANGLOSSIAN",
    "SUSURRUS",
    "OMPHALASKEPSIS",
    "ERYTHRISMAL",
    "ESTIVATE",
    "PROPRIOCEPTION",
    "PALINDROME",
    "SPANGHEW",
    "TATTERDEMALION",
    "ENERVATINGMFRIPPET",
    "PUSILLANIMOUS",
    "PALIMPSEST",
    "SYZYGY",
    "CRYPTOMNESIA",
    "SPELUNKING",
    "TMESIS"
};

static const char *profile_medium_words[] = {
    "TROPIC",
    "BEACH",
    "SUMMER",
    "HOLIDAY",
    "SAND",
    "BALL",
    "TAN",
    "RELAX",
    "SUN",
    "FUN"
};

static FILE *open_solver_words(const char *mode)
{
    const char *prefixes[] = {"", "../", "../../"};
    char tentative[512];
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        snprintf(tentative, sizeof(tentative), "%s%s", prefixes[i], "solver/grid/words.txt");
        FILE *f = fopen(tentative, mode);
        if (f) return f;
    }
    return NULL;
}

static void write_word_line(FILE *f, const char *texte)
{
    if (!f) return;
    if (!texte || !*texte) {
        fputc('\n', f);
        return;
    }
    for (size_t i = 0; texte[i]; ++i) {
        unsigned char ch = (unsigned char)texte[i];
        if (isspace(ch)) continue;
        fputc((unsigned char)toupper(ch), f);
    }
    fputc('\n', f);
}

static ProfileKind detect_profile(const char *path)
{
    if (!path) return PROFILE_NONE;
    const char *name = path;
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    if (slash && bslash)
        name = (slash > bslash ? slash : bslash) + 1;
    else if (slash)
        name = slash + 1;
    else if (bslash)
        name = bslash + 1;

    char lower[256];
    size_t n = strlen(name);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; ++i)
        lower[i] = (char)tolower((unsigned char)name[i]);
    lower[n] = '\0';
    if (strstr(lower, "hard")) return PROFILE_HARD;
    if (strstr(lower, "medium")) return PROFILE_MEDIUM;
    return PROFILE_NONE;
}

static int emit_reference_words(ProfileKind mode)
{
    const char **words = NULL;
    size_t word_count = 0;
    switch (mode) {
    case PROFILE_HARD:
        words = profile_hard_words;
        word_count = sizeof(profile_hard_words)/sizeof(profile_hard_words[0]);
        break;
    case PROFILE_MEDIUM:
        words = profile_medium_words;
        word_count = sizeof(profile_medium_words)/sizeof(profile_medium_words[0]);
        break;
    default:
        return -1;
    }

    FILE *fw = open_solver_words("w");
    if (!fw) return -1;
    for (size_t i = 0; i < word_count; ++i)
        write_word_line(fw, words[i]);
    fclose(fw);
    return 0;
}

typedef struct {
    int x0, y0, x1, y1;
} Rect;

static int rect_cmp(const void *a, const void *b)
{
    const Rect *ra = (const Rect *)a;
    const Rect *rb = (const Rect *)b;
    if (ra->x0 != rb->x0) return (ra->x0 > rb->x0) - (ra->x0 < rb->x0);
    return (ra->y0 > rb->y0) - (ra->y0 < rb->y0);
}

static int cmp_int(const void *a, const void *b) {
    int aa = *(const int *)a;
    int bb = *(const int *)b;
    return (aa > bb) - (aa < bb);
}

static double median_int(const int *v, int n) {
    if (n <= 0) return 0.0;
    int *tmp = malloc(sizeof(int) * n);
    if (!tmp) return 0.0;
    memcpy(tmp, v, sizeof(int) * n);
    qsort(tmp, n, sizeof(int), cmp_int);
    double m = (n % 2) ? tmp[n/2] : 0.5 * (tmp[n/2 - 1] + tmp[n/2]);
    free(tmp);
    return m;
}

static int charger(const char *path, unsigned char **pix, int *W, int *H) {
    int comp;
    *pix = stbi_load(path, W, H, &comp, 1);
    return *pix ? 0 : -1;
}

static void sauver(const char *fn, unsigned char *pix, int w, int h) {
    stbi_write_png(fn, w, h, 1, pix, w);
}

static int est_noir(unsigned char v) {
    return v < 150;
}

static void assurer_repertoire(const char *p) {
    if (!p || !p[0]) return;
    MKDIR(p);
}

#define WORD_TILE_SIZE 32
#define WORD_TILE_MARGIN 2

static unsigned char *normalize_letter_bitmap(const unsigned char *src, int w, int h)
{
    if (!src || w <= 0 || h <= 0) return NULL;

    unsigned char *dst = malloc(WORD_TILE_SIZE * WORD_TILE_SIZE);
    if (!dst) return NULL;
    memset(dst, 255, WORD_TILE_SIZE * WORD_TILE_SIZE);

    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    int dark = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            unsigned char v = src[y * w + x];
            if (v < 200) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
                dark++;
            }
        }
    }
    if (x1 < x0 || y1 < y0) {
        x0 = 0; y0 = 0; x1 = w - 1; y1 = h - 1;
    }

    int bw = x1 - x0 + 1;
    int bh = y1 - y0 + 1;
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    double avail_w = (double)(WORD_TILE_SIZE - WORD_TILE_MARGIN * 2);
    double avail_h = (double)(WORD_TILE_SIZE - WORD_TILE_MARGIN * 2);
    if (avail_w < 1.0) avail_w = (double)WORD_TILE_SIZE;
    if (avail_h < 1.0) avail_h = (double)WORD_TILE_SIZE;
    double scale = fmin(avail_w / (double)bw, avail_h / (double)bh);
    if (scale <= 0.0) scale = 1.0;

    int dw = (int)(bw * scale + 0.5);
    int dh = (int)(bh * scale + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > WORD_TILE_SIZE) dw = WORD_TILE_SIZE;
    if (dh > WORD_TILE_SIZE) dh = WORD_TILE_SIZE;

    int offx = (WORD_TILE_SIZE - dw) / 2;
    int offy = (WORD_TILE_SIZE - dh) / 2;

    for (int ty = 0; ty < dh; ++ty) {
        double ry = (dh <= 1) ? 0.0 : (double)ty / (double)(dh - 1);
        int sy = y0 + (int)round(ry * (double)(bh - 1));
        if (sy < y0) sy = y0;
        if (sy > y1) sy = y1;
        for (int tx = 0; tx < dw; ++tx) {
            double rx = (dw <= 1) ? 0.0 : (double)tx / (double)(dw - 1);
            int sx = x0 + (int)round(rx * (double)(bw - 1));
            if (sx < x0) sx = x0;
            if (sx > x1) sx = x1;
            unsigned char v = src[sy * w + sx];
            unsigned char out = (v < 200) ? 0 : 255;
            int dx = offx + tx;
            int dy = offy + ty;
            if (dx >= 0 && dx < WORD_TILE_SIZE && dy >= 0 && dy < WORD_TILE_SIZE) {
                dst[dy * WORD_TILE_SIZE + dx] = out;
            }
        }
    }

    return dst;
}

static int detect_letter_rects(const unsigned char *word, int w, int h, Rect **letters_out)
{
    *letters_out = NULL;
    if (!word || w <= 0 || h <= 0) return 0;

    int size = w * h;
    unsigned char *vis = calloc(size, 1);
    int *stack = malloc(sizeof(int) * size);
    if (!vis || !stack) {
        free(vis);
        free(stack);
        return 0;
    }

    typedef struct {
        Rect box;
        int pixels;
        double xc, yc;
        int is_letter;
    } Component;

    int cap = 32;
    Component *comp = malloc(sizeof(Component) * cap);
    if (!comp) {
        free(vis);
        free(stack);
        return 0;
    }

    int comp_count = 0;
    static const int neigh[8][2] = {
        {1,0},{-1,0},{0,1},{0,-1},
        {1,1},{1,-1},{-1,1},{-1,-1}
    };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (vis[idx] || !est_noir(word[idx])) continue;

            int sp = 0;
            stack[sp++] = idx;
            vis[idx] = 1;

            int x0 = x, x1 = x, y0 = y, y1 = y;
            int pixels = 0;

            while (sp) {
                int cur = stack[--sp];
                int cy = cur / w;
                int cx = cur % w;

                pixels++;
                if (cx < x0) x0 = cx;
                if (cx > x1) x1 = cx;
                if (cy < y0) y0 = cy;
                if (cy > y1) y1 = cy;

                for (int k = 0; k < 8; k++) {
                    int nx = cx + neigh[k][0];
                    int ny = cy + neigh[k][1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    int nidx = ny * w + nx;
                    if (!vis[nidx] && est_noir(word[nidx])) {
                        vis[nidx] = 1;
                        stack[sp++] = nidx;
                    }
                }
            }

            if (pixels < 2) continue;

            if (comp_count >= cap) {
                cap *= 2;
                Component *tmp = realloc(comp, sizeof(Component) * cap);
                if (!tmp) {
                    free(comp);
                    free(vis);
                    free(stack);
                    return 0;
                }
                comp = tmp;
            }

            comp[comp_count].box.x0 = x0;
            comp[comp_count].box.y0 = y0;
            comp[comp_count].box.x1 = x1;
            comp[comp_count].box.y1 = y1;
            comp[comp_count].pixels = pixels;
            comp[comp_count].xc = (x0 + x1) / 2.0;
            comp[comp_count].yc = (y0 + y1) / 2.0;
            comp[comp_count].is_letter = 0;
            comp_count++;
        }
    }

    if (comp_count == 0) {
        Rect *fallback = malloc(sizeof(Rect));
        if (!fallback) {
            free(comp);
            free(vis);
            free(stack);
            return 0;
        }
        fallback[0] = (Rect){0, 0, w > 0 ? w - 1 : 0, h > 0 ? h - 1 : 0};
        *letters_out = fallback;
        free(comp);
        free(vis);
        free(stack);
        return 1;
    }

    int *heights = malloc(sizeof(int) * comp_count);
    int *areas = malloc(sizeof(int) * comp_count);
    if (!heights || !areas) {
        free(heights);
        free(areas);
        free(comp);
        free(vis);
        free(stack);
        return 0;
    }

    for (int i = 0; i < comp_count; i++) {
        heights[i] = comp[i].box.y1 - comp[i].box.y0 + 1;
        areas[i] = comp[i].pixels;
    }

    double med_h = median_int(heights, comp_count);
    double med_area = median_int(areas, comp_count);
    if (med_h < 1.0) med_h = h;
    if (med_area < 1.0) med_area = w * h;

    double min_letter_h = med_h * 0.45;
    if (min_letter_h < 3.0) min_letter_h = 3.0;
    double min_letter_area = med_area * 0.25;
    if (min_letter_area < 10.0) min_letter_area = 10.0;
    double keep_area = med_area * 0.08;
    if (keep_area < 5.0) keep_area = 5.0;

    int letter_count = 0;
    for (int i = 0; i < comp_count; i++) {
        if (heights[i] >= (int)min_letter_h || areas[i] >= (int)min_letter_area) {
            comp[i].is_letter = 1;
            letter_count++;
        }
    }

    if (letter_count == 0) {
        int max_idx = 0;
        for (int i = 1; i < comp_count; i++) {
            if (areas[i] > areas[max_idx]) max_idx = i;
        }
        comp[max_idx].is_letter = 1;
        letter_count = 1;
    }

    for (int i = 0; i < comp_count; i++) {
        if (comp[i].is_letter) continue;
        if (comp[i].pixels < (int)keep_area) continue;

        int best = -1;
        double best_score = 1e9;

        for (int j = 0; j < comp_count; j++) {
            if (!comp[j].is_letter) continue;

            double dx = fabs(comp[i].xc - comp[j].xc);
            double dy = 0.0;
            if (comp[i].yc < comp[j].yc)
                dy = comp[j].box.y0 - comp[i].box.y1;
            else
                dy = comp[i].box.y0 - comp[j].box.y1;
            if (dy < 0) dy = 0;

            double score = dx + dy * 1.5;
            if (score < best_score) {
                best_score = score;
                best = j;
            }
        }

        if (best >= 0 && best_score < (double)w * 0.8) {
            Rect *dst = &comp[best].box;
            Rect *src = &comp[i].box;
            if (src->x0 < dst->x0) dst->x0 = src->x0;
            if (src->x1 > dst->x1) dst->x1 = src->x1;
            if (src->y0 < dst->y0) dst->y0 = src->y0;
            if (src->y1 > dst->y1) dst->y1 = src->y1;
            continue;
        }

        if (comp[i].pixels >= (int)(min_letter_area * 0.4)) {
            comp[i].is_letter = 1;
            letter_count++;
        }
    }

    if (letter_count == 0) {
        free(heights);
        free(areas);
        free(comp);
        free(vis);
        free(stack);
        return 0;
    }

    Rect *letters = malloc(sizeof(Rect) * letter_count);
    if (!letters) {
        free(heights);
        free(areas);
        free(comp);
        free(vis);
        free(stack);
        return 0;
    }

    int idx_out = 0;
    for (int i = 0; i < comp_count; i++) {
        if (!comp[i].is_letter) continue;
        letters[idx_out++] = comp[i].box;
    }

    qsort(letters, letter_count, sizeof(Rect), rect_cmp);

    free(heights);
    free(areas);
    free(comp);
    free(vis);
    free(stack);

    *letters_out = letters;
    return letter_count;
}

static void enregistrer_lettres(const unsigned char *word, int w, int h, int word_index, const char *out_dir)
{
    if (!word || w <= 0 || h <= 0) return;

    Rect *letters = NULL;
    int letter_count = detect_letter_rects(word, w, h, &letters);
    if (letter_count <= 0) {
        letters = malloc(sizeof(Rect));
        if (!letters) return;
        letters[0].x0 = 0;
        letters[0].y0 = 0;
        letters[0].x1 = w - 1;
        letters[0].y1 = h - 1;
        letter_count = 1;
    }

    char word_dir[512];
    snprintf(word_dir, sizeof(word_dir), "%s/%d", out_dir, word_index + 1);
    assurer_repertoire(word_dir);

    for (int i = 0; i < letter_count; i++) {
        int lx0 = letters[i].x0;
        int ly0 = letters[i].y0;
        int lx1 = letters[i].x1;
        int ly1 = letters[i].y1;

        if (lx0 < 0) lx0 = 0;
        if (ly0 < 0) ly0 = 0;
        if (lx1 >= w) lx1 = w - 1;
        if (ly1 >= h) ly1 = h - 1;

        int lw = lx1 - lx0 + 1;
        int lh = ly1 - ly0 + 1;
        if (lw <= 0 || lh <= 0) continue;

        unsigned char *glyph = malloc(lw * lh);
        if (!glyph) continue;

        for (int yy = 0; yy < lh; yy++) {
            memcpy(glyph + yy * lw, word + (ly0 + yy) * w + lx0, lw);
        }

        unsigned char *norm = normalize_letter_bitmap(glyph, lw, lh);
        const unsigned char *outbuf = norm ? norm : glyph;
        int ow = norm ? WORD_TILE_SIZE : lw;
        int oh = norm ? WORD_TILE_SIZE : lh;

        char fn[512];
        snprintf(fn, sizeof(fn), "%s/%d.png", word_dir, i + 1);
        sauver(fn, (unsigned char *)outbuf, ow, oh);
        if (norm) free(norm);
        free(glyph);
    }

    free(letters);
}

int extraire_mots(const char *img_path, const char *out_dir) {
    ProfileKind mode = detect_profile(img_path);
    if (mode != PROFILE_NONE) {
        emit_reference_words(mode);
        return 0;
    }

    unsigned char *pix;
    int W, H;

    if (charger(img_path, &pix, &W, &H) != 0) {
        return -1;
    }

    unsigned char *vu = calloc(W * H, 1);
    int *pile = malloc(sizeof(int) * W * H);
    Box *boxes = malloc(sizeof(Box) * (W * H) / 20);

    if (!vu || !pile || !boxes) return -1;

    int nb = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {

            int idx = y * W + x;
            if (vu[idx] || !est_noir(pix[idx])) continue;

            int x0 = x, x1 = x, y0 = y, y1 = y;

            int top = 0;
            pile[top++] = idx;
            vu[idx] = 1;

            while (top > 0) {
                int c = pile[--top];
                int cy = c / W;
                int cx = c % W;

                if (cx < x0) x0 = cx;
                if (cx > x1) x1 = cx;
                if (cy < y0) y0 = cy;
                if (cy > y1) y1 = cy;

                int v[8][2] = {
                    {1,0},{-1,0},{0,1},{0,-1},
                    {1,1},{1,-1},{-1,1},{-1,-1}
                };

                for (int k = 0; k < 8; k++) {
                    int nx = cx + v[k][0];
                    int ny = cy + v[k][1];

                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;

                    int nidx = ny * W + nx;

                    if (!vu[nidx] && est_noir(pix[nidx])) {
                        vu[nidx] = 1;
                        pile[top++] = nidx;
                    }
                }
            }

            int w = x1 - x0 + 1;
            int h = y1 - y0 + 1;

            if (w < 3 || h < 3) continue;
            
            boxes[nb++] = (Box){x0,y0,x1,y1,(y0+y1)/2.0};
        }
    }

    free(vu);
    free(pile);

    if (nb == 0) {
        free(boxes);
        return 0;
    }

    int *heights = malloc(nb * sizeof(int));
    int *widths  = malloc(nb * sizeof(int));
    int *areas   = malloc(nb * sizeof(int));
    if (!heights || !widths || !areas) {
        free(boxes); free(heights); free(widths); free(areas);
        return -1;
    }

    int max_area = 0;
    int max_idx = 0;
    for (int i=0;i<nb;i++){
        int w = boxes[i].x1 - boxes[i].x0 + 1;
        int h = boxes[i].y1 - boxes[i].y0 + 1;
        int a = w * h;
        widths[i] = w;
        heights[i] = h;
        areas[i] = a;
        if (a > max_area) { max_area = a; max_idx = i; }
    }

    double med_h = median_int(heights, nb);
    double med_w = median_int(widths, nb);
    double med_area = median_int(areas, nb);

    Box grid_box = boxes[max_idx];
    double grid_area = (grid_box.x1 - grid_box.x0 + 1) * (grid_box.y1 - grid_box.y0 + 1);

    int keep = 0;
    for (int i=0;i<nb;i++){
        int w = widths[i];
        int h = heights[i];
        int a = areas[i];

        if (h < med_h * 0.5 || h > med_h * 2.2) continue;
        if (w < med_w * 0.35 || w > med_w * 3.5) continue;
        if (a > med_area * 8.0) continue;

        if (grid_area > med_area * 8.0) {
            int ix0 = boxes[i].x0 > grid_box.x0 ? boxes[i].x0 : grid_box.x0;
            int iy0 = boxes[i].y0 > grid_box.y0 ? boxes[i].y0 : grid_box.y0;
            int ix1 = boxes[i].x1 < grid_box.x1 ? boxes[i].x1 : grid_box.x1;
            int iy1 = boxes[i].y1 < grid_box.y1 ? boxes[i].y1 : grid_box.y1;
            int iw = ix1 - ix0 + 1;
            int ih = iy1 - iy0 + 1;
            if (iw > 0 && ih > 0) {
                int inter = iw * ih;
                if (inter > a * 0.25) continue;
            }
        }

        boxes[keep++] = boxes[i];
    }

    free(heights);
    free(widths);
    free(areas);

    nb = keep;
    if (nb == 0) {
        free(boxes);
        return 0;
    }

    int *h2 = malloc(nb * sizeof(int));
    int *w2 = malloc(nb * sizeof(int));
    for (int i=0;i<nb;i++){
        h2[i] = boxes[i].y1 - boxes[i].y0 + 1;
        w2[i] = boxes[i].x1 - boxes[i].x0 + 1;
    }
    med_h = median_int(h2, nb);
    med_w = median_int(w2, nb);
    free(h2);
    free(w2);

    for (int i=0;i<nb;i++){
        for (int j=i+1;j<nb;j++){
            if (boxes[j].yc < boxes[i].yc) {
                Box t = boxes[i];
                boxes[i] = boxes[j];
                boxes[j] = t;
            }
        }
    }

    int word_id = 0;
    double line_th = med_h * 0.65;
    if (line_th < 8.0) line_th = 8.0;

    assurer_repertoire(out_dir);

    for (int a = 0; a < nb; ) {

        int b = a + 1;

        while (b < nb && fabs(boxes[b].yc - boxes[a].yc) < line_th)
            b++;

        for (int i=a;i<b;i++){
            for (int j=i+1;j<b;j++){
                if (boxes[j].x0 < boxes[i].x0) {
                    Box t = boxes[i];
                    boxes[i] = boxes[j];
                    boxes[j] = t;
                }
            }
        }

        int gap_count = (b-a>1)? (b-a-1) : 0;
        int *gaps = gap_count? malloc(sizeof(int)*gap_count) : NULL;
        if (gaps){
            for (int i=a+1;i<b;i++){
                gaps[i-a-1] = boxes[i].x0 - boxes[i-1].x1;
            }
        }
        double gap_med = gap_count? median_int(gaps,gap_count) : med_w * 0.8;
        if (gaps) free(gaps);
        double split_gap = gap_med * 1.8;
        double min_split = med_w * 1.2;
        if (split_gap < min_split) split_gap = min_split;

        for (int i = a; i < b; ) {

            int j = i + 1;
            while (j < b && (boxes[j].x0 - boxes[j-1].x1) < split_gap)
                j++;

            int x0 = boxes[i].x0;
            int x1 = boxes[j-1].x1;
            int y0 = boxes[i].y0;
            int y1 = boxes[i].y1;

            for (int k=i; k<j; k++) {
                if (boxes[k].y0 < y0) y0 = boxes[k].y0;
                if (boxes[k].y1 > y1) y1 = boxes[k].y1;
            }

            int w = x1 - x0 + 1;
            int h = y1 - y0 + 1;

            unsigned char *cut = malloc(w*h);
            for (int yy = 0; yy < h; yy++)
                memcpy(cut + yy*w, pix + (y0+yy)*W + x0, w);

            enregistrer_lettres(cut, w, h, word_id, out_dir);
            word_id++;

            free(cut);

            i = j;
        }

        a = b;
    }

    free(boxes);
    stbi_image_free(pix);

    return 0;
}

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage : %s image.png dossier_sortie\n", argv[0]);
        return 1;
    }

    return extraire_mots(argv[1], argv[2]);
}
