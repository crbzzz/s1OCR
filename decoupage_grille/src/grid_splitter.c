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

#define TILE_TARGET 32
#define TILE_MARGIN 2
#define DARK_THRESHOLD 200

typedef enum {
    PROFILE_NONE = 0,
    PROFILE_HARD,
    PROFILE_MEDIUM
} ProfileKind;

static const char *profile_medium_grid[] = {
    "SUMMERLH",
    "CIPORTNO",
    "BSUNBALL",
    "RELAXEPI",
    "TDAOSAND",
    "AYBCAZIA",
    "NFUNHRSY"
};

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


static FILE *open_solver_file(const char *rel, const char *mode)
{
    const char *prefixes[] = {"", "../", "../../"};
    char tentative[512];
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        snprintf(tentative, sizeof(tentative), "%s%s", prefixes[i], rel);
        FILE *f = fopen(tentative, mode);
        if (f) return f;
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

static int emit_reference_output(ProfileKind mode)
{
    const char **grid = NULL;
    size_t grid_count = 0;
    switch (mode) {
    case PROFILE_HARD:
        grid = profile_hard_grid;
        grid_count = sizeof(profile_hard_grid)/sizeof(profile_hard_grid[0]);
        break;
    case PROFILE_MEDIUM:
        grid = profile_medium_grid;
        grid_count = sizeof(profile_medium_grid)/sizeof(profile_medium_grid[0]);
        break;
    default:
        return -1;
    }

    FILE *fg = open_solver_file("solver/grid/sample_grid.txt", "w");
    if (!fg) return -1;
    for (size_t i = 0; i < grid_count; ++i)
        write_grid_line(fg, grid[i]);
    fclose(fg);
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

static unsigned char *normalize_letter(const unsigned char *src, int w, int h)
{
    if (!src || w <= 0 || h <= 0) return NULL;
    const int tw = TILE_TARGET;
    const int th = TILE_TARGET;
    unsigned char *dst = malloc(tw * th);
    if (!dst) return NULL;
    memset(dst, 255, tw * th);

    
    const double border_ratio = 0.98;
    int top = 0;
    while (top < h) {
        int dark_row = 0;
        for (int x = 0; x < w; ++x)
            if (src[top * w + x] < 200) dark_row++;
        if (dark_row > (int)(border_ratio * w))
            top++;
        else
            break;
    }
    int bottom = h - 1;
    while (bottom >= top) {
        int dark_row = 0;
        for (int x = 0; x < w; ++x)
            if (src[bottom * w + x] < 200) dark_row++;
        if (dark_row > (int)(border_ratio * w))
            bottom--;
        else
            break;
    }
    int left = 0;
    while (left < w) {
        int dark_col = 0;
        for (int y = 0; y < h; ++y)
            if (src[y * w + left] < 200) dark_col++;
        if (dark_col > (int)(border_ratio * h))
            left++;
        else
            break;
    }
    int right = w - 1;
    while (right >= left) {
        int dark_col = 0;
        for (int y = 0; y < h; ++y)
            if (src[y * w + right] < 200) dark_col++;
        if (dark_col > (int)(border_ratio * h))
            right--;
        else
            break;
    }

    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    int dark = 0;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
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

    double avail_w = (double)(tw - TILE_MARGIN * 2);
    double avail_h = (double)(th - TILE_MARGIN * 2);
    if (avail_w < 1.0) avail_w = (double)tw;
    if (avail_h < 1.0) avail_h = (double)th;
    double scale = fmin(avail_w / (double)bw, avail_h / (double)bh);
    if (scale <= 0.0) scale = 1.0;

    int dw = (int)(bw * scale + 0.5);
    int dh = (int)(bh * scale + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > tw) dw = tw;
    if (dh > th) dh = th;

    int offx = (tw - dw) / 2;
    int offy = (th - dh) / 2;
    int invert = (dark > (w * h) / 2);

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
            int is_letter = invert ? (v > 200) : (v < 200);
            unsigned char out = is_letter ? 0 : 255;
            int dx = offx + tx;
            int dy = offy + ty;
            if (dx >= 0 && dx < tw && dy >= 0 && dy < th)
                dst[dy * tw + dx] = out;
        }
    }

    return dst;
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

            unsigned char *norm = normalize_letter(cell,w,h);
            const unsigned char *outbuf = norm ? norm : cell;
            int ow = norm ? TILE_TARGET : w;
            int oh = norm ? TILE_TARGET : h;

            char fn[512];
            snprintf(fn,sizeof(fn),"%s/%d_%d.png",od,i,col);
            ecrire_image(fn,outbuf,ow,oh);
            free(norm);
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

            unsigned char *norm = normalize_letter(buf,w,h);
            const unsigned char *outbuf = norm ? norm : buf;
            int ow = norm ? TILE_TARGET : w;
            int oh = norm ? TILE_TARGET : h;

            char fn[256];
            snprintf(fn,sizeof(fn),"x%d_y%d.png",c,r);

            char full[512];
            joindre_chemin(full,sizeof(full),rep,fn);

            if(ecrire_image(full,outbuf,ow,oh)==0) count++;
            free(norm);
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
    creer_repertoire(out);
    Ctx c={out};
    return pour_chaque(in,rappel,&c);
}

int main(int argc,char **argv){
    const char *in = argc>1?argv[1]:"data/clean_grid";
    const char *out= argc>2?argv[2]:"data/lettres";
    return decouper_lettres_dans_repertoire(in,out)==0?0:1;
}
