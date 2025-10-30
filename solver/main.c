#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "solver.h"


static char* load_grid(const char* path, unsigned int* rows, unsigned int* cols) {
    FILE* f = fopen(path, "r");
    if (!f) 
    { 
        perror("open grid"); return NULL; 
    }

    char line[2048];
    char** lines = NULL;
    size_t n = 0, cap = 0;

    while (fgets(line, sizeof(line), f)) 
    {
        size_t len = strlen(line);
        while (len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';


        char compact[2048];
        size_t k = 0;
        for (size_t i=0; i<len; ++i) 
        {
            if (!isspace((unsigned char)line[i])) compact[k++] = line[i];
        }
        compact[k] = '\0';
        if (k == 0) continue; 

        if (n == cap) 
        {
            cap = cap ? cap*2 : 8;
            lines = (char**)realloc(lines, cap*sizeof(*lines));
            if (!lines) 
            { 
                fclose(f); return NULL; 
            }
        }
        lines[n] = strdup(compact);
        if (!lines[n]) 
        { 
            fclose(f); return NULL; 
        }
        n++;
    }
    fclose(f);

    if (n == 0) { fprintf(stderr,"Grille vide\n"); return NULL; }

    size_t c = strlen(lines[0]);
    for (size_t r=1; r<n; ++r) 
    {
        if (strlen(lines[r]) != c) 
        {
            fprintf(stderr,"Les lignes n'ont pas toute la meme longueur.\n");
            for (size_t i=0;i<n;i++) free(lines[i]); free(lines);
            return NULL;
        }
    }

    char* grid = (char*)malloc(n * c);
    if (!grid) 
    { 
        for (size_t i=0;i<n;i++) 
        {
            free(lines[i]); 
        
        }
    free(lines);
    return NULL; 
}

    for (size_t r=0; r<n; ++r) {
        memcpy(grid + r*c, lines[r], c);
        free(lines[r]);
    }
    free(lines);

    *rows = (unsigned int)n;
    *cols = (unsigned int)c;
    return grid;
}

int main(void) {
    unsigned int rows=0, cols=0;
    char* grid = load_grid("grid/sample_grid.txt", &rows, &cols);
    if (!grid) 
    {
        return 1;
    }
    printf("Grille %ux%u chargée.\n", rows, cols);

    FILE* fw = fopen("grid/words.txt", "r");
    if (!fw) { perror("open words"); free(grid); return 1; }

    char word[256];
    while (fgets(word, sizeof(word), fw)) 
    {
        size_t len = strlen(word);
        while (len && (word[len-1]=='\n' || word[len-1]=='\r')) word[--len] = '\0';
        if (len == 0) 
        {
            continue;
        }
        Coord s, e;
        int rc = search_word(grid, rows, cols, word, &s, &e);
        if (rc == 0)
       {
            printf("%s : trouvé de (%u,%u) à (%u,%u)\n", word, s.x, s.y, e.x, e.y);
        }
        else
        {
            printf("%s : non trouvé\n", word);
        }
    }

    fclose(fw);
    free(grid);
    return 0;
}
