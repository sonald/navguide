#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <iostream>
#include <random>

//#define USE_OPENGL 1

using namespace std;

typedef struct _Sprite Sprite;

SDL_Window* window = NULL;
SDL_Surface* surface = NULL;
SDL_Surface* bg = NULL;

SDL_Renderer* renderer = NULL;
SDL_Texture* bg_tex = NULL;

int screen_w = 0, screen_h = 0;
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
#ifdef USE_OPENGL
    SDL_Texture* tex;
#else
    SDL_Surface* surface;
#endif
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

#ifdef USE_OPENGL
    SDL_RenderCopy(renderer, s->tex, &r, &s->bound);
    if (i) {
        SDL_RenderFillRects(renderer, rects, i);
    }

#else
    SDL_BlitSurface(s->surface, &r, surface, &s->bound);
    if (i) {
        SDL_FillRects(surface, rects, i, SDL_MapRGBA(surface->format, 0x22, 0x22, 0x22, 0x20));
    }
#endif

    memmove(&s->traits[1], &s->traits, sizeof(s->traits[0])*4);
    s->traits[0] = s->bound;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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

    s->bound.x = MIN(MAX(s->bound.x, 0), screen_w);
    s->bound.y = MIN(MAX(s->bound.y, 0), screen_h);
}

Sprite* load_sprite(const char* file)
{
    static SDL_Surface* surf = NULL;
    static SDL_Texture* tex = NULL;
    static int tw = 0, th = 0;

    Sprite* res = sprite_sp;
    memset(res, 0, sizeof *res);
    sprite_sp++;

#ifdef USE_OPENGL
    if (!tex) {
#else
    if (!surf) {
#endif
        surf = IMG_Load(file);
        if (!surf) {
            err_quit("load sprite failed\n");
        }
        tw = surf->w;
        th = surf->h;

#ifdef USE_OPENGL
        tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
#endif
    }

#ifdef USE_OPENGL
    res->tex = tex;
#else
    res->surface = surf;
#endif

    res->bound = (SDL_Rect) { dist(rd), dist(rd), tw, th };
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
        int w = screen_w, h = screen_h;
        if (x < 50) {
            bg_x = MAX(bg_x-2, 0);
        } else if (x > w-50) {
            bg_x = MIN(bg_x+2, bg->w - w);
        }

        if (y < 50) {
            bg_y = MAX(bg_y-2, 0);
        } else if (y > h-50) {
            bg_y = MIN(bg_y+2, bg->h - h);
        }
    }
}

static void draw()
{
    cerr << __func__ << ": " << SDL_GetTicks() << endl;
    SDL_Rect r = { bg_x, bg_y, screen_w, screen_h };
#ifdef USE_OPENGL
    //SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, bg_tex, &r, NULL );
#else
    SDL_BlitSurface(bg, &r, surface, NULL);
#endif

    Sprite* s = &sprite_slab[0];
    while (s != sprite_sp) {
        s->draw(s);
        s++;
    }

#ifdef USE_OPENGL
    SDL_RenderPresent( renderer );
#else
    SDL_UpdateWindowSurface(window);
#endif
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

#ifndef USE_OPENGL
    if (!SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software")) {
        err_warn("use software render failed: %s\n", SDL_GetError());
    }

    if (!SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, 0)) {
        err_warn("disable 3d accel failed: %s\n", SDL_GetError());
    }
#endif

    window = SDL_CreateWindow("navguide", 0, 0, 1366, 768,
#ifdef USE_OPENGL 
            SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_MAXIMIZED|SDL_WINDOW_OPENGL);
#else
            SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_MAXIMIZED);
#endif
    if (!window) {
        err_quit("%s\n", SDL_GetError());
    }

    SDL_GetWindowSize(window, &screen_w, &screen_h);

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

#ifdef USE_OPENGL
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 0xee, 0xee, 0x0, 0x80);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

#else
    surface = SDL_GetWindowSurface(window);
    if (SDL_ISPIXELFORMAT_ALPHA(surface->format->format)) {
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    }
    cerr << "window: " << surface->w << "," << surface->h << endl;
    cerr << SDL_GetPixelFormatName(surface->format->format) << endl;
#endif

    if (!(IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG))) {
        err_quit("png load init failed\n");
    }
    bg = IMG_Load("background.jpg");
    if (!bg) {
        err_quit("load background failed\n");
    }
    SDL_SetSurfaceBlendMode(bg, SDL_BLENDMODE_NONE);
    cerr << "background: " << bg->w << "," << bg->h << endl;
#ifdef USE_OPENGL
    bg_tex = SDL_CreateTextureFromSurface(renderer, bg);
    SDL_FreeSurface(bg);
#endif
    
    spawn_sprites(NSPAWN);
    
    refresh_time = SDL_GetTicks() + 500;
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
        //SDL_Delay(30);

        if (SDL_TICKS_PASSED(SDL_GetTicks(), refresh_time)) {
            update();
            draw();
            //refresh_time = SDL_GetTicks() + 500;
            refresh_time += 500;
        }
    }

#ifdef USE_OPENGL
    SDL_DestroyRenderer(renderer);
#else
    SDL_FreeSurface(surface);
    SDL_FreeSurface(bg);
#endif

    SDL_DestroyWindow(window);
    return 0;
}
