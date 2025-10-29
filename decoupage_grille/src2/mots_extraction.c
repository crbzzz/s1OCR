#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static int creer_dossier(const char *chem) {
    if (!chem || !*chem) return -1;
#ifdef _WIN32
    if (_mkdir(chem) == 0 || errno == EEXIST) return 0;
    if (errno == ENOENT) {
        char tmp[1024];
        const char *sep = strrchr(chem, '\\');
        if (!sep) sep = strrchr(chem, '/');
        if (!sep) return -1;
        size_t len = (size_t)(sep - chem);
        if (len >= sizeof(tmp)) return -1;
        memcpy(tmp, chem, len);
        tmp[len] = '\0';
        if (creer_dossier(tmp) != 0) return -1;
        return _mkdir(chem) == 0 || errno == EEXIST ? 0 : -1;
    }
    return -1;
#else
    if (mkdir(chem, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    if (errno == ENOENT) {
        char tmp[1024];
        const char *sep = strrchr(chem, '/');
        if (!sep) return -1;
        size_t len = (size_t)(sep - chem);
        if (len >= sizeof(tmp)) return -1;
        memcpy(tmp, chem, len);
        tmp[len] = '\0';
        if (creer_dossier(tmp) != 0) return -1;
        return mkdir(chem, 0755) == 0 || errno == EEXIST ? 0 : -1;
    }
    return -1;
#endif
}

static int copie_fichier(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "Impossible d'ouvrir %s\n", src);
        return -1;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        fprintf(stderr, "Impossible de creer %s\n", dst);
        return -1;
    }
    unsigned char buf[4096];
    size_t lu;
    while ((lu = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, lu, out) != lu) {
            fclose(in);
            fclose(out);
            fprintf(stderr, "Erreur ecriture vers %s\n", dst);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static void trim_fin(char *ligne) {
    size_t len = strlen(ligne);
    while (len && (ligne[len - 1] == '\n' || ligne[len - 1] == '\r'))
        ligne[--len] = '\0';
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <dossier_lettres> <fichier_coords> <dossier_sortie>\n", argv[0]);
        return 1;
    }

    const char *dossier_lettres = argv[1];
    const char *fichier_coords = argv[2];
    const char *dossier_sortie = argv[3];

    if (creer_dossier(dossier_sortie) != 0) {
        fprintf(stderr, "Impossible de preparer %s\n", dossier_sortie);
        return 1;
    }

    FILE *f = fopen(fichier_coords, "r");
    if (!f) {
        fprintf(stderr, "Impossible d'ouvrir %s\n", fichier_coords);
        return 1;
    }

    char ligne[4096];
    size_t mot_idx = 0;
    size_t total = 0;

    while (fgets(ligne, sizeof(ligne), f)) {
        trim_fin(ligne);
        if (ligne[0] == '\0' || ligne[0] == '#')
            continue;

        char *token = strtok(ligne, " \t");
        if (!token)
            continue;

        mot_idx++;
        size_t lettre_idx = 0;

        while ((token = strtok(NULL, " \t")) != NULL) {
            char *virgule = strchr(token, ',');
            if (!virgule)
                continue;
            *virgule = '\0';
            const char *txt_col = token;
            const char *txt_lig = virgule + 1;
            char *end_col = NULL;
            char *end_lig = NULL;
            long col = strtol(txt_col, &end_col, 10);
            long lig = strtol(txt_lig, &end_lig, 10);
            if ((end_col && *end_col) || (end_lig && *end_lig))
                continue;

            char src[512];
            snprintf(src, sizeof(src), "%s/%ld,%ld.png", dossier_lettres, col, lig);

            char dst[512];
            snprintf(dst, sizeof(dst), "%s/mot%zuL%zu.png", dossier_sortie, mot_idx, ++lettre_idx);

            if (copie_fichier(src, dst) == 0)
                total++;
            else
                fprintf(stderr, "Lettre manquante : %s\n", src);
        }
    }

    fclose(f);
    printf("Lettres copiees: %zu\n", total);
    return 0;
}
