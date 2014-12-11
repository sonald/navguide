#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>


#include <ft2build.h>
#include FT_FREETYPE_H
        
#include <iostream>
#include <random>

using namespace std;

typedef struct _Sprite Sprite;

static FT_Library ft;
static FT_Face face;
static int point_size = 16;
static void draw_text(const char* text, cairo_t* cr, int x, int y);

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
    char* label;
    Rect traits[5];
    unsigned int update_time;
    DIRECTION dir;
    cairo_surface_t* surface;
    cairo_surface_t* label_surface;

    void (*draw)(Sprite*, cairo_t*);
    void (*update)(Sprite*);
};

#define LABEL_LEN 64
#define MAX_SPRITES 4096
#define NSPAWN 2000
Sprite sprite_slab[MAX_SPRITES];
char label_slab[MAX_SPRITES*LABEL_LEN];
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

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, s->surface, s->bound.x, s->bound.y);
    cairo_rectangle(cr, s->bound.x, s->bound.y, s->bound.w, s->bound.h);
    cairo_fill(cr);

    cairo_set_source_surface(cr, s->label_surface, s->bound.x+s->bound.w, s->bound.y);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    auto& x = s->traits;
    int i = 0;
    Rect rects[5];
    for (i = 0; i < 5; i++) {
        if (x[i].w == 0) break;
        rects[i].w = rects[i].h = 10;
        rects[i].x = x[i].x + (x[i].w - 10)/2;
        rects[i].y = x[i].y + (x[i].h - 10)/2;
        cairo_set_source_rgba(cr, 0x22, 0x22, 0x22, 0x20);
        cairo_rectangle(cr, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
        cairo_fill(cr);
    }

    memmove(&s->traits[1], &s->traits, sizeof(s->traits[0])*4);
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

    s->bound.x = min(max(s->bound.x, 0), screen_w);
    s->bound.y = min(max(s->bound.y, 0), screen_h);
}

static void load_text(Sprite* s, const char* text)
{
    int atlas_w = 0, atlas_h = 0;
    FT_GlyphSlot slot = face->glyph;
    for (int i = 0, n = strlen(text); i < n; i++) {
        if (FT_Load_Char(face, text[i], FT_LOAD_RENDER)) {
            std::cerr << "load " << text[i] << " failed\n";
            return;
        }
        auto& bm = slot->bitmap;

        atlas_h = std::max(atlas_h, bm.rows + ((int)slot->advance.y >> 6));
        atlas_w += (slot->advance.x >> 6);
    }

    int x = 0, y = atlas_h;
    s->label_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
            atlas_w, atlas_h);
    //cerr << __func__ << " " << cairo_image_surface_get_width(s->label_surface) 
        //<< ", " << cairo_image_surface_get_height(s->label_surface) << endl;

    cairo_t* cr = cairo_create(s->label_surface);

    for (int i = 0, n = strlen(text); i < n; i++) {
        if (FT_Load_Char(face, text[i], FT_LOAD_RENDER)) {
            std::cerr << "load " << text[i] << " failed\n";
            return;
        }

        auto& bm = slot->bitmap;
        //little-endian
        unsigned char* buf = new unsigned char[bm.width*bm.rows*4];
        for (int i = 0, n = bm.width*bm.rows; i < n; i++) {
            buf[i*4] = bm.buffer[i] > 0 ? 0: 255;
            buf[i*4+1] = buf[i*4+2] = buf[i*4];
            buf[i*4+3] = 0x80;
        }
        
        cairo_surface_t* surf = cairo_image_surface_create_for_data(buf,
                CAIRO_FORMAT_ARGB32, bm.width, bm.rows, bm.width*4);
        if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
            cerr << "create glyph surface failed: " << cairo_status_to_string(cairo_surface_status(surf)) << endl;
        }
        cairo_set_source_surface(cr, surf, x + slot->bitmap_left, y - slot->bitmap_top );
        cairo_paint(cr);
        cairo_surface_destroy(surf);
        delete buf;

        x += (slot->advance.x >> 6);
    }
    cairo_destroy(cr);
    cairo_surface_flush(s->label_surface);
}

Sprite* load_sprite(const char* file)
{
    static int tw = 0, th = 0;
    static cairo_surface_t* surf = NULL;

    char* label = &label_slab[sprite_sp*LABEL_LEN];
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
    res->label = label;
    snprintf(label, LABEL_LEN-1, "monkey #%d", sprite_sp);
    load_text(res, res->label);

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

static gboolean on_configure(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    //GdkWindow* dw = gtk_widget_get_window(widget);
    //screen_w = gdk_window_get_width(dw);
    //screen_h = gdk_window_get_height(dw);
    return FALSE;
}

static gboolean on_key_press(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    if (ev->key.keyval == GDK_KEY_Escape) {
        gtk_main_quit();
    }
    return FALSE;
}

static gboolean on_timeout(gpointer data)
{
    update();
    gtk_widget_queue_draw(window);
    return G_SOURCE_CONTINUE;
}

static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cerr << __func__ << ": " << get_ticks() << endl;

    Rect r = { bg_x, bg_y, screen_w, screen_h };

    static cairo_surface_t* tmp = NULL;
    if (!tmp) {
        tmp = cairo_image_surface_create(CAIRO_FORMAT_RGB24, screen_w, screen_h);
    }

    cairo_t* cr2 = cairo_create(tmp);

    cairo_set_operator(cr2, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr2, bg, 0 - bg_x, 0 - bg_y);
    cairo_rectangle(cr2, 0, 0, screen_w, screen_h);
    cairo_fill(cr2);

    for (int i = 0; i < sprite_sp; i++) {
        sprite_slab[i].draw(&sprite_slab[i], cr2);
    }
    cairo_surface_flush(tmp);
    cairo_destroy(cr2);

    cairo_set_source_surface(cr, tmp, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);

    return TRUE;
}

static void draw_text(const char* text, cairo_t* cr, int x, int y)
{
    FT_GlyphSlot slot = face->glyph;
    for (int i = 0, n = strlen(text); i < n; i++) {
        if (FT_Load_Char(face, text[i], FT_LOAD_RENDER)) {
            std::cerr << "load " << text[i] << " failed\n";
            return;
        }

        auto& bm = slot->bitmap;
        unsigned char* buf = new unsigned char[bm.width*bm.rows*4];
        for (int i = 0, n = bm.width*bm.rows; i < n; i++) {
            buf[i*4] = bm.buffer[i] > 0 ? 0: 255;
            buf[i*4+1] = buf[i*4+2] = buf[i*4];
            buf[i*4+3] = 0x80;
        }
        
        cairo_surface_t* surf = cairo_image_surface_create_for_data(buf,
                CAIRO_FORMAT_ARGB32, bm.width, bm.rows, bm.width*4);
        if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
            cerr << "create glyph surface failed: " << cairo_status_to_string(cairo_surface_status(surf)) << endl;
        }
        cairo_set_source_surface(cr, surf, x + slot->bitmap_left, y - slot->bitmap_top );
        cairo_paint(cr);
        cairo_surface_destroy(surf);
        delete buf;

        x += slot->advance.x >> 6;
    }
}

static void init_ft()
{
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "init freetype failed" << std::endl;
        exit(-1);
    }

    if (FT_New_Face(ft, "/usr/share/fonts/TTF/DejaVuSansMono.ttf", 0, &face)) {
        std::cerr << "load face failed" << std::endl;
        exit(-1);
    }

    FT_Set_Pixel_Sizes(face, 0, point_size);
    //FT_ULong num = 128;

    ////ASCII is loaded by default
    //for (auto i = 32; i < num; i++) {
        //load_char_helper(i);
    //}
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    init_ft();

    memset(sprite_slab, 0, sizeof sprite_slab);
    memset(label_slab, 0, sizeof label_slab);
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
            "signal::configure-event", on_configure, NULL,
            NULL);

    gtk_container_add(GTK_CONTAINER(top), window);
    
    gtk_widget_set_can_focus(window, TRUE);
    gtk_window_maximize(GTK_WINDOW(top));
    gtk_widget_show_all(top);

    GdkScreen* scr = gtk_window_get_screen(GTK_WINDOW(top));
    GdkRectangle r;
    gdk_screen_get_monitor_workarea(scr, 0, &r);
    screen_w = r.width, screen_h = r.height;

    gdk_window_set_events(gtk_widget_get_window(window), GDK_ALL_EVENTS_MASK);
    get_ticks();
    g_timeout_add(500, on_timeout, NULL);

    gtk_main();
    return 0;
}
