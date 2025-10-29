#include "solver.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int match_at(const char *grid,
                    unsigned int rows,
                    unsigned int cols,
                    const char *word,
                    unsigned int x,
                    unsigned int y,
                    int dx,
                    int dy,
                    Coord *start,
                    Coord *end) {
    unsigned int len = (unsigned int)strlen(word);
    unsigned int cx = x;
    unsigned int cy = y;

    for (unsigned int i = 0; i < len; ++i) {
        if (cx >= cols || cy >= rows) {
            return -1;
        }
        char grid_ch = grid[cy * cols + cx];
        char word_ch = word[i];
        if ((unsigned char)grid_ch != (unsigned char)word_ch) {
            if (toupper((unsigned char)grid_ch) !=
                toupper((unsigned char)word_ch)) {
                return -1;
            }
        }
        cx = (unsigned int)((int)cx + dx);
        cy = (unsigned int)((int)cy + dy);
    }

    if (start != NULL) {
        start->x = x;
        start->y = y;
    }
    if (end != NULL) {
        unsigned int ex = (unsigned int)((int)x + dx * ((int)len - 1));
        unsigned int ey = (unsigned int)((int)y + dy * ((int)len - 1));
        end->x = ex;
        end->y = ey;
    }
    return 0;
}

int search_word(const char *grid,
                unsigned int rows,
                unsigned int cols,
                const char *word,
                Coord *start,
                Coord *end) {
    if (grid == NULL || word == NULL) {
        return -1;
    }
    if (rows == 0 || cols == 0) {
        return -1;
    }
    if (word[0] == '\0') {
        return -1;
    }

    static const int directions[8][2] = {
        {1, 0},  {0, 1},  {-1, 0}, {0, -1},
        {1, 1},  {-1, -1}, {1, -1}, {-1, 1}
    };

    unsigned int len = (unsigned int)strlen(word);

    for (unsigned int y = 0; y < rows; ++y) {
        for (unsigned int x = 0; x < cols; ++x) {
            for (size_t dir = 0; dir < 8; ++dir) {
                int dx = directions[dir][0];
                int dy = directions[dir][1];
                unsigned int ex =
                    (unsigned int)((int)x + dx * ((int)len - 1));
                unsigned int ey =
                    (unsigned int)((int)y + dy * ((int)len - 1));
                if (dx > 0 && ex >= cols) {
                    continue;
                }
                if (dx < 0 && (int)x - (int)(len - 1) < 0) {
                    continue;
                }
                if (dy > 0 && ey >= rows) {
                    continue;
                }
                if (dy < 0 && (int)y - (int)(len - 1) < 0) {
                    continue;
                }
                if (match_at(grid, rows, cols, word, x, y, dx, dy,
                             start, end) == 0) {
                    return 0;
                }
            }
        }
    }

    return -1;
}