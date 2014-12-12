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

GdkDevice *mouse = NULL;

cairo_surface_t* surface = NULL;
cairo_surface_t* atlas_surface = NULL;
cairo_surface_t* bg = NULL;
GtkWidget* window = NULL;

int screen_w = 0, screen_h = 0;
int bg_x = 0, bg_y = 0, bg_w = 0, bg_h = 0;
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

#define LABEL_LEN 32
#define MAX_SPRITES 3000
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

    auto& x = s->traits;
    int i = 0;
    Rect rects[5];
    for (i = 0; i < 5; i++) {
        if (x[i].w == 0) break;
        rects[i].w = rects[i].h = 10;
        rects[i].x = x[i].x + (x[i].w - 10)/2;
        rects[i].y = x[i].y + (x[i].h - 10)/2;
        cairo_set_source_rgba(cr, 0xe2, 0x22, 0x22, 0x80);
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

static const int TEX_LEN = 200 * 15 * 4;
static unsigned char tex_slab[NSPAWN*TEX_LEN]; // large enough for all surfaces

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
    unsigned char* label_buf = &tex_slab[TEX_LEN*sprite_sp];
    s->label_surface = cairo_image_surface_create_for_data(label_buf,
            CAIRO_FORMAT_ARGB32, atlas_w, atlas_h, atlas_w * 4);
    //cerr << __func__ << "atlas " << atlas_w << "," << atlas_h << endl;

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
            buf[i*4] = 0;
            buf[i*4+1] = buf[i*4+2] = buf[i*4];
            buf[i*4+3] = bm.buffer[i] > 0 ? 0xc0: 0;
        }
        
        cairo_surface_t* surf = cairo_image_surface_create_for_data(buf,
                CAIRO_FORMAT_ARGB32, bm.width, bm.rows, bm.width*4);
        if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
            cerr << "create glyph surface failed: " << cairo_status_to_string(cairo_surface_status(surf)) << endl;
        }
        cairo_set_source_surface(cr, surf, x + slot->bitmap_left, y - slot->bitmap_top );
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle(cr, x + slot->bitmap_left, y - slot->bitmap_top, bm.width, bm.rows);
        cairo_fill(cr);

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
    Sprite* res = &sprite_slab[sprite_sp];

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

    sprite_sp++;
    return res;
}

static void update()
{
    for (int i = 0; i < sprite_sp; i++) {
        sprite_slab[i].update(&sprite_slab[i]);
    }

    {
        int step = 10;
        int x, y;
        gdk_device_get_position(mouse, NULL, &x, &y);

        int w = screen_w, h = screen_h;
        if (x < 100) {
            bg_x = max(bg_x-step, 0);
        } else if (x > w-100) {
            bg_x = min(bg_x+step, bg_w - w);
        }

        if (y < 100) {
            bg_y = max(bg_y-step, 0);
        } else if (y > h-100) {
            bg_y = min(bg_y+step, bg_h - h);
        }
    }
}

static void spawn_sprites(int n)
{
    while (n--) {
        load_sprite("sprite.png");
    }

    std::cerr << "spawn sprites done" << sprite_sp << std::endl;
}

static gboolean drag = FALSE;
static double mouse_x = 0, mouse_y = 0;
static gboolean on_button_press(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    drag = TRUE;
    mouse_x = ev->button.x, mouse_y = ev->button.y;
    return FALSE;
}

static gboolean on_button_release(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    drag = FALSE;
    return FALSE;
}

static gboolean on_mouse_motion(GtkWidget* widget, GdkEvent* ev, gpointer data)
{
    if (drag) {
        
    }
    return FALSE;
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
        tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, screen_w, screen_h);
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
}

static void load_background()
{
    GdkPixbuf* pix = gdk_pixbuf_new_from_file("background.jpg", NULL);
    bg = gdk_cairo_surface_create_from_pixbuf(pix, 0, NULL);
    g_object_unref(pix);
    bg_w = cairo_image_surface_get_width(bg), bg_h = cairo_image_surface_get_height(bg);
    cerr << "bg: " << cairo_image_surface_get_width(bg) << ", " << cairo_image_surface_get_height(bg) 
        << "  alpha: " << (cairo_image_surface_get_format(bg) == CAIRO_FORMAT_ARGB32) << endl;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget* top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GdkScreen* scr = gtk_window_get_screen(GTK_WINDOW(top));
    GdkRectangle r;
    gdk_screen_get_monitor_workarea(scr, 0, &r);
    screen_w = r.width, screen_h = r.height;

    GdkVisual *visual = gdk_screen_get_rgba_visual (scr);
    if (visual != NULL)
        gtk_widget_set_visual(GTK_WIDGET(top), visual);

    init_ft();
    load_background();

    GdkDevice *device;
    GdkDeviceManager* dev_manager = gdk_display_get_device_manager(
            gtk_widget_get_display(top));
    GList* pointers = gdk_device_manager_list_devices(dev_manager,
            GDK_DEVICE_TYPE_MASTER);
    if (pointers) {
        device = (GdkDevice*)pointers->data;
        g_list_free(pointers);
    }

    if (gdk_device_get_source(device) == GDK_SOURCE_KEYBOARD) {
        mouse = gdk_device_get_associated_device(device);
    } else {
        mouse = device;
    }

    memset(sprite_slab, 0, sizeof sprite_slab);
    memset(label_slab, 0, sizeof label_slab);
    //memset(tex_slab, 0, sizeof tex_slab);
    spawn_sprites(NSPAWN);
    
    window = gtk_drawing_area_new();
    g_object_connect(window,
            "signal::draw", (draw_callback), NULL,
            "signal::key-press-event", on_key_press, NULL,
            "signal::destroy", gtk_main_quit, NULL,
            "signal::configure-event", on_configure, NULL,
            "signal::motion-notify-event", on_mouse_motion, NULL,
            NULL);

    gtk_container_add(GTK_CONTAINER(top), window);
    
    gtk_widget_set_app_paintable(top, TRUE);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_set_can_focus(window, TRUE);
    gtk_window_maximize(GTK_WINDOW(top));
    gtk_widget_show_all(top);

    gdk_window_set_events(gtk_widget_get_window(window), GDK_ALL_EVENTS_MASK);
    get_ticks();
    g_timeout_add(500, on_timeout, NULL);

    gtk_main();
    return 0;
}
