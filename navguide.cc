#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <iostream>
#include <random>

using namespace std;

typedef struct _Sprite Sprite;

SDL_Window* window = NULL;
SDL_Surface* surface = NULL;
SDL_Surface* bg = NULL;
int bg_x = 0, bg_y = 0;
std::random_device rd;
std::uniform_int_distribution<int> dist(0, 800);
std::uniform_int_distribution<int> dir_dist(1, 4);

unsigned int current_time = 0;
unsigned int refresh_time = 0;


static void err_warn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void err_quit(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

typedef enum {
    Up = 1, Down, Right, Left
} DIR;

struct _Sprite {
    SDL_Rect bound; /// x,y used as postion, w,h used ad bound
    SDL_Surface* surface;
    const char* label;
    SDL_Rect traits[5];
    unsigned int update_time;
    DIR dir;

    void (*draw)(Sprite*);
    void (*update)(Sprite*);
};

#define MAX_SPRITES 4096
#define NSPAWN 2000
Sprite sprite_slab[MAX_SPRITES];
Sprite* sprite_sp = &sprite_slab[0];

ostream& operator<<(ostream& os, const SDL_Rect& r)
{
    return os << "{" << r.x << ", " << r.y << ", " << r.w << ", " << r.h << "}";
}

static void sprite_draw(Sprite* s)
{
    SDL_Rect r = {
        0, 0, s->bound.w, s->bound.h
    };
    SDL_BlitSurface(s->surface, &r, surface, &s->bound);

    auto& x = s->traits;
    int i = 0;
    SDL_Rect rects[5];
    for (i = 0; i < 5; i++) {
        if (x[i].w == 0) break;
        rects[i].w = rects[i].h = 10;
        rects[i].x = x[i].x + (x[i].w - 10)/2;
        rects[i].y = x[i].y + (x[i].h - 10)/2;
        //cerr << rects[i] << endl;
    }

    if (i) {
        //std::cerr << "draw " << i << " traits" << std::endl;
        SDL_FillRects(surface, rects, i, SDL_MapRGBA(surface->format, 0x22, 0x22, 0x22, 0x20));
    }

    memmove(&s->traits[1], &s->traits, sizeof(s->traits[0])*4);
    s->traits[0] = s->bound;
}

static void sprite_update(Sprite* s)
{
    if (SDL_TICKS_PASSED(SDL_GetTicks(), s->update_time)) {
        s->dir = (DIR)dir_dist(rd);
        s->update_time = SDL_GetTicks() + 5000;
    }

    switch(s->dir) {
        case Up:
            s->bound.y--; break;
        case Down:
            s->bound.y++; break;
        case Left:
            s->bound.x--; break;
        default: // right
            s->bound.x++; break;
    }

    s->bound.x = min(max(s->bound.x, 0), surface->w);
    s->bound.y = min(max(s->bound.y, 0), surface->h);
}

Sprite* load_sprite(const char* file)
{
    Sprite* res = sprite_sp;
    memset(res, 0, sizeof *res);
    sprite_sp++;

    res->surface = IMG_Load(file);
    if (!res->surface) {
        err_quit("load sprite failed\n");
    }

    res->bound = (SDL_Rect) {
        dist(rd), dist(rd), res->surface->w, res->surface->h
    };
    res->draw = sprite_draw;
    res->update = sprite_update;
    char l[64];
    std::snprintf(l, sizeof l - 1, "%s %lu", file, sprite_sp - &sprite_slab[0]);
    res->label = strdup(l);

    return res;
}

static void update()
{
    Sprite* s = &sprite_slab[0];
    while (s != sprite_sp) {
        s->update(s);
        s++;
    }

    {
        int x, y;
        SDL_GetMouseState(&x, &y);
        int w = surface->w, h = surface->h;
        if (x < 50) {
            bg_x = max(bg_x-2, 0);
        } else if (x > w-50) {
            bg_x = min(bg_x+2, bg->w - w);
        }

        if (y < 50) {
            bg_y = max(bg_y-2, 0);
        } else if (y > h-50) {
            bg_y = min(bg_y+2, bg->h - h);
        }
    }
}

static void draw()
{
    SDL_Rect r = { bg_x, bg_y, surface->w, surface->h };
    SDL_BlitSurface(bg, &r, surface, NULL);

    Sprite* s = &sprite_slab[0];
    while (s != sprite_sp) {
        s->draw(s);
        s++;
    }

    SDL_UpdateWindowSurface(window);
}

static void spawn_sprites(int n)
{
    while (n--) {
        load_sprite("sprite.png");
    }

    std::cerr << "spawn sprites done" << std::endl;
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        err_quit("Unable to initialize SDL:  %s\n", SDL_GetError());
        return 1;
    }

    atexit(SDL_Quit);

    if (!SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software")) {
        err_warn("use software render failed: %s\n", SDL_GetError());
    }

    if (!SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, 0)) {
        err_warn("disable 3d accel failed: %s\n", SDL_GetError());
    }

    window = SDL_CreateWindow("navguide", 0, 0, 1366, 768,
            SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_MAXIMIZED);
    if (!window) {
        err_quit("%s\n", SDL_GetError());
    }

    int n = SDL_GetNumDisplayModes(0);
    for (int i = 0; i < n; i++) {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, i, &mode);
        cerr << "next " << SDL_GetPixelFormatName(mode.format) << endl;
        if (mode.format == SDL_PIXELFORMAT_RGBA8888) {
            SDL_SetWindowDisplayMode(window, &mode);
            cerr << "set rgba mode\n";
            break;
        }
    }

    if (!(IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG))) {
        err_quit("png load init failed\n");
    }
    bg = IMG_Load("background.jpg");
    if (!bg) {
        err_quit("load background failed\n");
    }
    SDL_SetSurfaceBlendMode(bg, SDL_BLENDMODE_NONE);
    cerr << "background: " << bg->w << "," << bg->h << endl;

    surface = SDL_GetWindowSurface(window);
    
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    cerr << "window: " << surface->w << "," << surface->h << endl;
    cerr << SDL_GetPixelFormatName(surface->format->format) << endl;
    spawn_sprites(NSPAWN);
    
    int quit = 0;
    while (!quit) {
        current_time = SDL_GetTicks();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1; break;
            }

            switch(e.type) {
                case SDL_KEYDOWN: 
                {
                    SDL_KeyboardEvent* kev = (SDL_KeyboardEvent*)&e;
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        quit = 1; 
                    }
                    break;
                }

                case SDL_MOUSEMOTION:
                {
                    auto& m = e.motion;
                    break;
                }

                default: break;
            }
        }
        SDL_Delay(30);
        update();

        if (SDL_TICKS_PASSED(SDL_GetTicks(), refresh_time)) {
            draw();
            refresh_time = SDL_GetTicks() + 500;
        }
    }

    SDL_DestroyWindow(window);
    return 0;
}
