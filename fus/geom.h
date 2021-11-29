
#include <stdlib.h>

#define MAX_VEC_DIMS 4

typedef unsigned char Uint8;

typedef int _vec_t[MAX_VEC_DIMS];
typedef int _boundbox_t[MAX_VEC_DIMS*2];

typedef struct {
    char r;
    char g;
    char b;
} SDL_Color;

typedef struct {
    int w, h;
    void *pixels;
} SDL_Surface;

void SDL_FreeSurface(SDL_Surface *surface) {
    free(surface->pixels);
    free(surface);
}

typedef struct {
    const char **strings;
} stringstore_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} position_box_t;

#define RENDERGRAPH_BITMAP_EXTRA_CLEANUP if (it->surface) SDL_FreeSurface(it->surface);
