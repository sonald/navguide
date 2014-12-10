#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>

#include <iostream>
#include <random>

using namespace std;

typedef struct _Sprite Sprite;

cairo_surface_t* surface = NULL;
cairo_surface_t* bg = NULL;
GtkWidget* window = NULL;

int screen_w = 0, screen_h = 0;
int bg_x = 0, bg_y = 0;
std::random_device rd;
std::uniform_int_distribution<int> dist(10, 800);
std::uniform_int_distribution<int> dir_dist(1, 4);

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

static int get_ticks()
{
    static int start = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (!start) {
        start = ts.tv_nsec / 1000000 + ts.tv_sec * 1000;
        return 0;
    }

    return ts.tv_nsec / 1000000 + ts.tv_sec * 1000 - start;
}

typedef enum {
    Up = 1, Down, Right, Left
} DIRECTION;

typedef struct {
    int x, y;
    int w, h;
} Rect;

struct _Sprite {
    Rect bound; /// x,y used as postion, w,h used ad bound
    const char* label;
    Rect traits[5];
    unsigned int update_time;
    DIRECTION dir;
    cairo_surface_t* surface;

    void (*draw)(Sprite*, cairo_t*);
    void (*update)(Sprite*);
};

#define MAX_SPRITES 4096
#define NSPAWN 2000
Sprite sprite_slab[MAX_SPRITES];
int sprite_sp = 0;

ostream& operator<<(ostream& os, const Rect& r)
{
    return os << "{" << r.x << ", " << r.y << ", " << r.w << ", " << r.h << "}";
}

static void sprite_draw(Sprite* s, cairo_t* cr)
{
    Rect r = {
        0, 0, s->bound.w, s->bound.h
    };

    auto& x = s->traits;
    int i = 0;
    Rect rects[5];
    for (i = 0; i < 5; i++) {
        if (x[i].w == 0) break;
        rects[i].w = rects[i].h = 10;
        rects[i].x = x[i].x + (x[i].w - 10)/2;
        rects[i].y = x[i].y + (x[i].h - 10)/2;
        //cerr << rects[i] << endl;
    }

    cairo_save(cr);
    cairo_translate(cr, s->bound.x, s->bound.y);
    cairo_set_source_surface(cr, s->surface, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);
    cairo_restore(cr);

    //if (i) {
        //SDL_FillRects(surface, rects, i, SDL_MapRGBA(surface->format, 0x22, 0x22, 0x22, 0x20));
    //}

    //memmove(&s->traits[1], &s->traits, sizeof(s->traits[0])*4);
    s->traits[0] = s->bound;
}

static void sprite_update(Sprite* s)
{
    if (get_ticks() > s->update_time) {
        s->dir = (DIRECTION)dir_dist(rd);
        s->update_time = get_ticks() + 5000;
    }

    int step = 8;
    switch(s->dir) {
        case Up:
            s->bound.y -= step; break;
        case Down:
            s->bound.y += step; break;
        case Left:
            s->bound.x -= step; break;
        default: // right
            s->bound.x += step; break;
    }

    s->bound.x = MIN(MAX(s->bound.x, 0), screen_w);
    s->bound.y = MIN(MAX(s->bound.y, 0), screen_h);
    s->bound.x = dist(rd);
    s->bound.y = dist(rd);
}

Sprite* load_sprite(const char* file)
{
    static int tw = 0, th = 0;
    static cairo_surface_t* surf = NULL;

    Sprite* res = &sprite_slab[sprite_sp++];

    if (!surf) {
        GdkPixbuf* pix = gdk_pixbuf_new_from_file(file, NULL);
        surf = gdk_cairo_surface_create_from_pixbuf(pix, 0, NULL);
        g_object_unref(pix);
        if (!surf) {
            err_quit("load sprite failed\n");
        }
        tw = cairo_image_surface_get_width(surf),
        th = cairo_image_surface_get_height(surf);
    }

    res->surface = surf;

    res->bound = (Rect) { dist(rd), dist(rd), tw, th };
    res->draw = sprite_draw;
    res->update = sprite_update;
    res->update_time = get_ticks();

    return res;
}

static void update()
{
    for (int i = 0; i < sprite_sp; i++) {
        sprite_slab[i].update(&sprite_slab[i]);
    }

    //{
        //int x, y;
        //SDL_GetMouseState(&x, &y);
        //int w = screen_w, h = screen_h;
        //if (x < 50) {
            //bg_x = MAX(bg_x-2, 0);
        //} else if (x > w-50) {
            //bg_x = MIN(bg_x+2, bg->w - w);
        //}

        //if (y < 50) {
            //bg_y = MAX(bg_y-2, 0);
        //} else if (y > h-50) {
            //bg_y = MIN(bg_y+2, bg->h - h);
        //}
    //}
}

static void spawn_sprites(int n)
{
    while (n--) {
        load_sprite("sprite.png");
    }

    std::cerr << "spawn sprites done" << sprite_sp << std::endl;
}

static gboolean on_key_press(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    if (ev->key.keyval == GDK_KEY_Escape) {
        gtk_main_quit();
    }
}

static gboolean on_timeout(gpointer data)
{
    update();
    gtk_widget_queue_draw(window);
    return G_SOURCE_CONTINUE;
}

static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    guint width, height;
    GdkRGBA color;

    //cerr << __func__ << ": " << get_ticks() << endl;
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);


    Rect r = { bg_x, bg_y, screen_w, screen_h };

    cairo_set_source_surface(cr, bg, bg_x, bg_y);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    for (int i = 0; i < sprite_sp; i++) {
        sprite_slab[i].draw(&sprite_slab[i], cr);
    }

    return FALSE;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    memset(sprite_slab, 0, sizeof sprite_slab);
    spawn_sprites(NSPAWN);
    
    GdkPixbuf* pix = gdk_pixbuf_new_from_file("background.jpg", NULL);
    bg = gdk_cairo_surface_create_from_pixbuf(pix, 0, NULL);
    cerr << "bg: " << cairo_image_surface_get_width(bg) << ", " << cairo_image_surface_get_height(bg) << endl;

    GtkWidget* top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    window = gtk_drawing_area_new();
    g_object_connect(window,
            "signal::draw", (draw_callback), NULL,
            "signal::key-press-event", on_key_press, NULL,
            "signal::destroy", gtk_main_quit, NULL,
            NULL);

    gtk_container_add(GTK_CONTAINER(top), window);
    
    gtk_widget_realize(top);
    gtk_widget_realize(window);

    gtk_widget_set_can_focus(window, TRUE);
    gtk_widget_show_all(top);
    gtk_window_maximize(GTK_WINDOW(top));

    gdk_window_set_events(gtk_widget_get_window(window), GDK_ALL_EVENTS_MASK);
    GdkWindow* dw = gtk_widget_get_window(top);
    screen_w = gdk_window_get_width(dw);
    screen_h = gdk_window_get_height(dw);
    gtk_widget_set_size_request(window, screen_w, screen_h);

    get_ticks();
    g_timeout_add(500, on_timeout, NULL);

    gtk_main();
    return 0;
}