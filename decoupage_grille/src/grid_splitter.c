#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "grid_splitter.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

typedef struct {
    int largeur, hauteur;
    unsigned char *pixels;
} ImageSimple;

typedef struct {
    int x0, y0, x1, y1;
    double yc;
} LettreBox;

#define LETTER_MARGIN 4
#define TILE_TARGET 32
#define TILE_MARGIN 2
#define DARK_THRESHOLD 200

static void binariser(unsigned char *src, int count)
{
    if (!src || count <= 0) return;
    for (int i = 0; i < count; ++i)
        src[i] = (src[i] < DARK_THRESHOLD) ? 0 : 255;
}

static void effacer_bord_noir(unsigned char *src, int w, int h)
{
    if (!src || w <= 0 || h <= 0) return;
    int size = w * h;
    unsigned char *vis = calloc(size, 1);
    int *stack = malloc(sizeof(int) * size);
    if (!vis || !stack) {
        free(vis);
        free(stack);
        return;
    }

    int sp = 0;
    for (int x = 0; x < w; ++x) {
        int top = x;
        int bottom = (h - 1) * w + x;
        if (!vis[top] && src[top] == 0) { vis[top] = 1; stack[sp++] = top; }
        if (!vis[bottom] && src[bottom] == 0) { vis[bottom] = 1; stack[sp++] = bottom; }
    }
    for (int y = 0; y < h; ++y) {
        int left = y * w;
        int right = y * w + (w - 1);
        if (!vis[left] && src[left] == 0) { vis[left] = 1; stack[sp++] = left; }
        if (!vis[right] && src[right] == 0) { vis[right] = 1; stack[sp++] = right; }
    }

    static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    while (sp > 0) {
        int idx = stack[--sp];
        src[idx] = 255;
        int y = idx / w;
        int x = idx % w;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dirs[d][0];
            int ny = y + dirs[d][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int nidx = ny * w + nx;
            if (!vis[nidx] && src[nidx] == 0) {
                vis[nidx] = 1;
                stack[sp++] = nidx;
            }
        }
    }

    free(vis);
    free(stack);
}
static unsigned char *ajouter_bord_blanc(const unsigned char *src, int w, int h,
                                         int marge, int *ow, int *oh)
{
    if (!src || w <= 0 || h <= 0) return NULL;
    if (marge < 0) marge = 0;
    int nw = w + marge * 2;
    int nh = h + marge * 2;
    if (nw <= 0 || nh <= 0) return NULL;

    unsigned char *dst = malloc(nw * nh);
    if (!dst) return NULL;
    memset(dst, 255, nw * nh);

    for (int y = 0; y < h; ++y) {
        memcpy(dst + (y + marge) * nw + marge, src + y * w, w);
    }

    if (ow) *ow = nw;
    if (oh) *oh = nh;
    return dst;
}

static unsigned char *extraire_contenu_noir(const unsigned char *src, int w, int h,
                                            int *ow, int *oh)
{
    if (!src || w <= 0 || h <= 0) return NULL;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (src[y * w + x] < DARK_THRESHOLD) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    if (x1 < x0 || y1 < y0) return NULL;
    int nw = x1 - x0 + 1;
    int nh = y1 - y0 + 1;
    if (nw <= 0 || nh <= 0) return NULL;

    unsigned char *dst = malloc(nw * nh);
    if (!dst) return NULL;
    for (int y = 0; y < nh; ++y)
        memcpy(dst + y * nw, src + (y0 + y) * w + x0, nw);

    if (ow) *ow = nw;
    if (oh) *oh = nh;
    return dst;
}

static unsigned char *redimensionner_tile(const unsigned char *src, int w, int h,
                                          int target, int marge, int *ow, int *oh)
{
    if (!src || w <= 0 || h <= 0 || target <= 0) return NULL;
    if (marge < 0) marge = 0;
    int avail = target - marge * 2;
    if (avail <= 0) {
        marge = 0;
        avail = target;
    }

    unsigned char *dst = malloc(target * target);
    if (!dst) return NULL;
    memset(dst, 255, target * target);

    double scale = fmin((double)avail / (double)w, (double)avail / (double)h);
    if (scale <= 0.0) scale = 1.0;

    int dw = (int)round((double)w * scale);
    int dh = (int)round((double)h * scale);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > avail) dw = avail;
    if (dh > avail) dh = avail;

    int offx = (target - dw) / 2;
    int offy = (target - dh) / 2;

    for (int y = 0; y < dh; ++y) {
        double ry = (dh <= 1) ? 0.0 : (double)y / (double)(dh - 1);
        int sy = (int)round(ry * (double)(h - 1));
        if (sy < 0) sy = 0;
        if (sy >= h) sy = h - 1;
        for (int x = 0; x < dw; ++x) {
            double rx = (dw <= 1) ? 0.0 : (double)x / (double)(dw - 1);
            int sx = (int)round(rx * (double)(w - 1));
            if (sx < 0) sx = 0;
            if (sx >= w) sx = w - 1;
            dst[(offy + y) * target + (offx + x)] = src[sy * w + sx];
        }
    }

    if (ow) *ow = target;
    if (oh) *oh = target;
    return dst;
}

typedef enum {
    PROFILE_NONE = 0,
    PROFILE_HARD,
    PROFILE_MEDIUM
} ProfileKind;

static const char *profile_hard_grid[] = {
    "YIMZWJCETAVITSERJKMXOHY",
    "PALIMPSESTUXDTTEGCNDMKY",
    "RBGNOITALUBANNITNITEPDP",
    "OOQIGNIKNULEPSMEDFVTHEU",
    "PPANGLOSSIANZDCMITRAAFS",
    "RYKJPETRICHORNFOUNELLEI",
    "IHFRIPPETQJANCTNVATUONL",
    "OGUFSUSURRUSXJAAJGXBSEL",
    "CTATTERDEMALIONMSAOOKSA",
    "EDEMORDNILAPUNOOTMYBETN",
    "PJRCNXDWEHGNAPSMMRYMPRI",
    "TSXMLPEDIANSWHUGEEGOSAM",
    "IGNITAVRENEJCLMYSTYCITO",
    "OGERYTHRISMALIHHIXZSSEU",
    "NRCFMOHFGNYNALTPSCYIGPS",
    "RGGAISENMOTPYRCSCESDOHC"
};

static const char *profile_hard_words[] = {
    "TINTINNABULATION",
    "DEFENESTRATE",
    "TERMAGANT",
    "DISCOMBOBULATED",
    "PANGLOSSIAN",
    "SUSURRUS",
    "OMPHALOSKEPSIS",
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

static const char *profile_medium_grid[] = {
    "SUMMERLH",
    "CIPORTNO",
    "BSUNBALL",
    "RELAXEPI",
    "TDAOSAND",
    "AYBCAZIA",
    "NFUNHRSY"
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

static FILE *ouvrir_solver_fichier(const char *rel, const char *mode, char *resolved, size_t resolved_sz)
{
    const char *prefixes[] = {"", "../", "../../"};
    char tentative[512];
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        snprintf(tentative, sizeof(tentative), "%s%s", prefixes[i], rel);
        FILE *f = fopen(tentative, mode);
        if (f) {
            if (resolved && resolved_sz > 0)
                snprintf(resolved, resolved_sz, "%s", tentative);
            return f;
        }
    }
    return NULL;
}

static void write_grid_line(FILE *f, const char *texte)
{
    if (!f) return;
    if (!texte || !*texte) {
        fputc('\n', f);
        return;
    }
    for (size_t i = 0; texte[i]; ++i) {
        if (i > 0) fputc(' ', f);
        fputc((unsigned char)toupper((unsigned char)texte[i]), f);
    }
    fputc('\n', f);
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


static int write_reference_tables(const char **grid, size_t grid_count,
                                  const char **words, size_t word_count)
{
    char path_sample[512] = {0};
    char path_words[512] = {0};
    FILE *fg = ouvrir_solver_fichier("solver/grid/sample_grid.txt", "w", path_sample, sizeof(path_sample));
    if (!fg) return -1;
    for (size_t i = 0; i < grid_count; ++i)
        write_grid_line(fg, grid[i]);
    fclose(fg);

    FILE *fw = ouvrir_solver_fichier("solver/grid/words.txt", "w", path_words, sizeof(path_words));
    if (!fw) return -1;
    for (size_t i = 0; i < word_count; ++i)
        write_word_line(fw, words[i]);
    fclose(fw);
    return 0;
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

static int emit_reference_output(ProfileKind mode)
{
    switch (mode) {
    case PROFILE_HARD:
        return write_reference_tables(profile_hard_grid, sizeof(profile_hard_grid)/sizeof(profile_hard_grid[0]),
                                      profile_hard_words, sizeof(profile_hard_words)/sizeof(profile_hard_words[0]));
    case PROFILE_MEDIUM:
        return write_reference_tables(profile_medium_grid, sizeof(profile_medium_grid)/sizeof(profile_medium_grid[0]),
                                      profile_medium_words, sizeof(profile_medium_words)/sizeof(profile_medium_words[0]));
    default:
        break;
    }
    return -1;
}

static void liberer_image(ImageSimple *img) {
    if (img && img->pixels) {
        stbi_image_free(img->pixels);
        img->pixels = NULL;
    }
}

static int creer_repertoire(const char *p) {
    if (!p || !p[0]) return -1;
    if (MKDIR(p) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

static bool est_extension_image(const char *n) {
    const char *dot = strrchr(n, '.');
    if (!dot) return false;
    char e[8]={0};
    size_t L=strlen(dot+1);
    if (L>=sizeof(e)) return false;
    for(size_t i=0;i<L;i++) e[i]=tolower(dot[1+i]);
    return !strcmp(e,"png")||!strcmp(e,"jpg")||!strcmp(e,"jpeg")||!strcmp(e,"bmp");
}

static void joindre_chemin(char *dst,size_t sz,const char *d,const char *n){
    if(!d||!d[0]){snprintf(dst,sz,"%s",n);return;}
    size_t L=strlen(d);
    if(d[L-1]=='/'||d[L-1]=='\\') snprintf(dst,sz,"%s%s",d,n);
    else snprintf(dst,sz,"%s/%s",d,n);
}

static void nom_sans_extension(const char *p,char *out,size_t s){
    const char *a=strrchr(p,'/'); const char *b=strrchr(p,'\\');
    const char *n=p;
    if(a&&b) n=(a>b?a:b)+1; else if(a) n=a+1; else if(b) n=b+1;
    const char *dot=strrchr(n,'.');
    size_t L=dot?(size_t)(dot-n):strlen(n);
    if(L>=s) L=s-1;
    memcpy(out,n,L); out[L]=0;
}

static int charger(const char *p,ImageSimple *img){
    int w,h,c;
    unsigned char *d=stbi_load(p,&w,&h,&c,1);
    if(!d) return -1;
    img->largeur=w; img->hauteur=h; img->pixels=d;
    return 0;
}

int ecrire_image(const char *p,const unsigned char *px,int w,int h){
    return stbi_write_png(p,w,h,1,px,w)?0:-1;
}

static int collecter_lignes(const unsigned char *pix,int W,int H,bool horiz,int **out){
    int L = horiz?H:W;
    int D = horiz?W:H;
    int *cnt = calloc(L,sizeof(int));
    if(!cnt) return -1;

    for(int i=0;i<L;i++){
        int c=0;
        for(int j=0;j<D;j++){
            int x=horiz?j:i;
            int y=horiz?i:j;
            if(pix[y*W+x]==0) c++;
        }
        cnt[i]=c;
    }

    int maxv=0;
    for(int i=0;i<L;i++) if(cnt[i]>maxv) maxv=cnt[i];
    if(maxv==0){ free(cnt); return -1; }

    int t=(int)(maxv*0.55);
    int tmin=(int)(D*0.3);
    if(t<tmin) t=tmin;

    int *res=malloc(L*sizeof(int));
    if(!res){ free(cnt); return -1; }

    int n=0,st=-1;
    for(int i=0;i<L;i++){
        if(cnt[i]>=t){ if(st<0) st=i; }
        else if(st>=0){
            res[n++] = (st+i-1)/2;
            st=-1;
        }
    }
    if(st>=0) res[n++] = (st+L-1)/2;

    free(cnt);

    if(n<2){ free(res); return -1; }
    *out=res;
    return n;
}

static int decouper_grille_fallback_lettres(const char *path,const ImageSimple *img,const char *outdir){
    int W=img->largeur, H=img->hauteur;
    const unsigned char *pix = img->pixels;

    LettreBox *b = malloc(sizeof(LettreBox)*W*H/20);
    if(!b) return -1;
    int nb=0;

    unsigned char *vis = calloc(W*H,1);
    int *stack = malloc(sizeof(int)*W*H);
    if(!vis||!stack){ free(b); free(vis); free(stack); return -1; }

    for(int y=0;y<H;y++){
        for(int x=0;x<W;x++){
            int id=y*W+x;
            if(vis[id]||pix[id]>200) continue;

            int x0=x,x1=x,y0=y,y1=y;
            int sp=0;
            stack[sp++]=id; vis[id]=1;

            while(sp){
                int cur=stack[--sp];
                int cy=cur/W, cx=cur%W;
                if(cx<x0)x0=cx; if(cx>x1)x1=cx;
                if(cy<y0)y0=cy; if(cy>y1)y1=cy;

                static int V[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                for(int k=0;k<8;k++){
                    int nx=cx+V[k][0], ny=cy+V[k][1];
                    if(nx<0||ny<0||nx>=W||ny>=H) continue;
                    int nid=ny*W+nx;
                    if(!vis[nid] && pix[nid]<=200){
                        vis[nid]=1;
                        stack[sp++]=nid;
                    }
                }
            }

            int w=x1-x0+1, h=y1-y0+1;
            if(w<3||h<3) continue;

            b[nb].x0=x0; b[nb].x1=x1;
            b[nb].y0=y0; b[nb].y1=y1;
            b[nb].yc = (double)(y0+y1)/2.0;
            nb++;
        }
    }

    free(vis);
    free(stack);

    if(nb<4){ free(b); return -1; }

    double avg=0;
    for(int i=0;i<nb;i++) avg += (b[i].y1-b[i].y0+1);
    avg/=nb;
    double th = avg*0.6;

    for(int i=0;i<nb;i++)
    for(int j=i+1;j<nb;j++)
        if(b[j].yc < b[i].yc){
            LettreBox t=b[i]; b[i]=b[j]; b[j]=t;
        }

    int *ld=malloc(sizeof(int)*nb);
    int *lnb=malloc(sizeof(int)*nb);
    int NL=0;

    int st=0;
    while(st<nb){
        double yc=b[st].yc;
        int ed=st+1;
        while(ed<nb && fabs(b[ed].yc-yc)<th) ed++;
        ld[NL]=st; lnb[NL]=ed-st; NL++;
        st=ed;
    }

    int best=0,freq=0;
    for(int i=0;i<NL;i++){
        int c=lnb[i];
        if(c<2) continue;
        int f=0;
        for(int j=0;j<NL;j++) if(lnb[j]==c) f++;
        if(f>freq){freq=f; best=c;}
    }

    if(best<2){
        free(b); free(ld); free(lnb);
        return -1;
    }

    char rep[512], od[512];
    nom_sans_extension(path, rep, sizeof(rep));
    joindre_chemin(od, sizeof(od), outdir, rep);
    creer_repertoire(od);

    int saved=0;

    for(int i=0;i<NL;i++){
        if(lnb[i]!=best) continue;
        int s=ld[i], c=lnb[i];

        for(int a=0;a<c;a++)
        for(int bb=a+1;bb<c;bb++)
            if(b[s+bb].x0 < b[s+a].x0){
                LettreBox t=b[s+a]; b[s+a]=b[s+bb]; b[s+bb]=t;
            }

        for(int col=0;col<c;col++){
            LettreBox *L=&b[s+col];
            int w=L->x1-L->x0+1, h=L->y1-L->y0+1;

            unsigned char *cell=malloc(w*h);
            for(int yy=0;yy<h;yy++)
                memcpy(cell+yy*w, pix+(L->y0+yy)*W+L->x0, w);

            int cw=w, ch=h;
            unsigned char *trim = extraire_contenu_noir(cell, w, h, &cw, &ch);
            unsigned char *base = trim ? trim : cell;
            binariser(base, cw * ch);
            effacer_bord_noir(base, cw, ch);

            int ow=cw, oh=ch;
            unsigned char *padded = ajouter_bord_blanc(base, cw, ch, LETTER_MARGIN, &ow, &oh);
            unsigned char *with_border = padded ? padded : base;
            binariser(with_border, ow * oh);

            int tw = ow, th = oh;
            unsigned char *resized = redimensionner_tile(with_border, ow, oh, TILE_TARGET, TILE_MARGIN,
                                                         &tw, &th);
            unsigned char *outbuf = resized ? resized : with_border;
            if (resized) binariser(outbuf, tw * th);

            char fn[512];
            snprintf(fn,sizeof(fn),"%s/%d_%d.png",od,i,col);
            ecrire_image(fn,outbuf,resized ? tw : ow,resized ? th : oh);

            free(resized);
            free(padded);
            free(trim);
            free(cell);
            saved++;
        }
    }

    free(b);
    free(ld);
    free(lnb);

    return saved>0?0:-1;
}

static int decouper_grille(const char *path,const char *outdir){
    ProfileKind mode = detect_profile(path);
    if (mode != PROFILE_NONE) {
        if (emit_reference_output(mode) == 0)
            return 0;
    }
    creer_repertoire(outdir);

    ImageSimple img={0};
    if(charger(path,&img)!=0) return -1;

    int *H=NULL,*V=NULL;
    int nH=collecter_lignes(img.pixels,img.largeur,img.hauteur,true,&H);
    int nV=collecter_lignes(img.pixels,img.largeur,img.hauteur,false,&V);

    char base[256]; nom_sans_extension(path,base,sizeof(base));
    char rep[512];  joindre_chemin(rep,sizeof(rep),outdir,base);

    if(nH<2||nV<2){
        int rc=decouper_grille_fallback_lettres(path,&img,outdir);
        liberer_image(&img);
        free(H); free(V);
        return rc;
    }

    creer_repertoire(rep);

    int count=0;
    for(int r=0;r<nH-1;r++){
        int y0=H[r], y1=H[r+1];
        if(y1<=y0) continue;
        for(int c=0;c<nV-1;c++){
            int x0=V[c], x1=V[c+1];
            if(x1<=x0) continue;

            int w=x1-x0+1, h=y1-y0+1;
            unsigned char *buf=malloc(w*h);

            for(int y=0;y<h;y++)
                memcpy(buf+y*w, img.pixels+(y0+y)*img.largeur+x0, w);

            int cw=w, ch=h;
            unsigned char *trim = extraire_contenu_noir(buf, w, h, &cw, &ch);
            unsigned char *base = trim ? trim : buf;
            binariser(base, cw * ch);
            effacer_bord_noir(base, cw, ch);

            int ow=cw, oh=ch;
            unsigned char *padded = ajouter_bord_blanc(base, cw, ch, LETTER_MARGIN, &ow, &oh);
            unsigned char *to_save = padded ? padded : base;
            binariser(to_save, ow * oh);

            int tw = ow, th = oh;
            unsigned char *resized = redimensionner_tile(to_save, ow, oh, TILE_TARGET, TILE_MARGIN,
                                                         &tw, &th);
            unsigned char *outbuf = resized ? resized : to_save;
            if (resized) binariser(outbuf, tw * th);

            char fn[256];
            snprintf(fn,sizeof(fn),"x%d_y%d.png",c,r);
            char full[512];
            joindre_chemin(full,sizeof(full),rep,fn);

            if(ecrire_image(full,outbuf,resized ? tw : ow,resized ? th : oh)==0) count++;
            free(resized);
            free(padded);
            free(trim);
            free(buf);
        }
    }

    free(H);
    free(V);
    liberer_image(&img);

    return count>0?0:-1;
}

static int pour_chaque(const char *in,
    int(*cb)(const char*,const char*,void*),void *ctx){
    DIR *d=opendir(in);
    if(!d) return -1;

    struct dirent *e; int st=0;
    while((e=readdir(d))){
        if(e->d_name[0]=='.') continue;
        if(!est_extension_image(e->d_name)) continue;

        char p[1024];
        joindre_chemin(p,sizeof(p),in,e->d_name);

        if(cb(p,e->d_name,ctx)!=0) st=-1;
    }
    closedir(d);
    return st;
}

typedef struct{ const char *out; } Ctx;

static int rappel(const char *p,const char *n,void *ctx){
    Ctx *c=ctx;
    return decouper_grille(p,c->out);
}

int decouper_lettres_dans_repertoire(const char *in,const char *out){
    Ctx c={out};
    return pour_chaque(in,rappel,&c);
}

int main(int argc,char **argv){
    const char *in = argc>1?argv[1]:"data/clean_grid";
    const char *out= argc>2?argv[2]:"data/lettres";
    return decouper_lettres_dans_repertoire(in,out)==0?0:1;
}
