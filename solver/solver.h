#ifndef OCR_SOLVER_H
#define OCR_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int x;
    unsigned int y;
} Coord;


int search_word(const char *grid,
                unsigned int rows,
                unsigned int cols,
                const char *word,
                Coord *start,
                Coord *end);

#ifdef __cplusplus
}
#endif

#endif