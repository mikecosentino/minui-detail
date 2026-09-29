// minui-detail shows one thing in detail for MinUI and NextUI paks: a cover
// image, a title, a column of facts and a description that scrolls, over a
// row of button hints.
//
//   minui-detail --file /tmp/detail.json \
//       --confirm-text DOWNLOAD --action-button X --action-text STAR
//   case $? in 0) download ;; 2) back ;; 4) star ;; esac
//
// The file's format is described in detail_doc.h. minui-list can only show
// rows of single-line text, so a pak's "about this game" screen ends up as
// "Developer: ..." rows with the summary chopped into more of them; this
// draws the same facts as a page.
//
// Built on minui-presenter (MIT, Jose Diaz-Gonzalez) by way of minui-progress:
// the same MinUI/NextUI platform code, button groups, power handling and build
// scaffolding.

#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <msettings.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#include "detail_doc.h"
#include "image_fit.h"
#include "text_wrap.h"

// Platform compatibility: NextUI names this PWR_isOnline
#ifdef PLATFORM_NEXTUI
#define PLAT_isOnline PWR_isOnline
#endif

// the same numbers minui-list uses, so a pak can switch between the two
// without relearning what $? means
enum
{
    ExitCodeSuccess = 0,
    ExitCodeError = 1,
    ExitCodeCancelButton = 2,
    ExitCodeActionButton = 4,
    ExitCodeKeyboardInterrupt = 130,
    ExitCodeSigterm = 143,
};

// the widest the cover may be, as a fraction of the screen; on a 4:3 screen
// this is what stops it crowding the text column into a sliver
#define COVER_MAX_FRACTION 0.36
// rounding on the cover's corners, before scaling
#define COVER_RADIUS 6
// the description scrollbar's width, before scaling
#define SCROLLBAR_WIDTH 3
#define TITLE_MAX_LINES 2
// how many lines a field's value may wrap to before it is cut short
#define VALUE_MAX_LINES 3
#define MAX_ITEMS 1200

struct Button
{
    char name[16];
    char text[256];
    int btn;
};

struct AppState
{
    int redraw;
    int quitting;
    int exit_code;

    char file[1024];
    struct Button confirm;
    struct Button cancel;
    struct Button action;
    bool show_hardware_group;

    DetailDoc doc;
};

// One line of text in the scrolling column, positioned relative to the top
// of the column's content. The text points into the DetailDoc.
enum TextStyle
{
    StyleText,
    StyleDim,
    StyleAccent,
};

struct Item
{
    TTF_Font *font;
    enum TextStyle style;
    int x;
    int y;
    int w;
    const char *text;
    size_t len;
};

// Everything whose size depends on the screen, worked out once at startup.
//
// Only the cover stays put. Everything to its right -- title, facts,
// description -- is one column that scrolls as a whole: every MinUI screen is
// about 240 units tall once scaled, which leaves no room to give the
// description a fixed box of its own under the facts.
struct Layout
{
    int margin;
    int gap;

    SDL_Surface *cover; // already scaled, in the screen's format; may be NULL
    SDL_Rect cover_rect;

    // the column's visible window on the screen
    SDL_Rect view;
    // how far down the scrollbar starts, to clear the battery and wifi
    int scrollbar_top;

    struct Item items[MAX_ITEMS];
    int item_count;
    int content_h;
    int scroll;
    bool scrollbar;
};

SDL_Surface *screen = NULL;
static struct Layout layout;

void log_error(const char *msg)
{
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "%s\n", msg);
}

// The theme's colors on NextUI; MinUI's fixed palette otherwise.
static SDL_Color text_color(void)
{
#ifdef PLATFORM_NEXTUI
    return uintToColour(THEME_COLOR4_255);
#else
    return COLOR_WHITE;
#endif
}

static SDL_Color dim_text_color(void)
{
#ifdef PLATFORM_NEXTUI
    return uintToColour(THEME_COLOR6_255);
#else
    return COLOR_GRAY;
#endif
}

static SDL_Color accent_text_color(void)
{
#ifdef PLATFORM_NEXTUI
    return uintToColour(THEME_COLOR1_255);
#else
    return COLOR_WHITE;
#endif
}

static SDL_Color background_rgb(void)
{
#ifdef PLATFORM_NEXTUI
    return uintToColour(THEME_COLOR7_255);
#else
    return (SDL_Color){0, 0, 0, 255};
#endif
}

static uint32_t background_color(SDL_Surface *dst)
{
#ifdef PLATFORM_NEXTUI
    (void)dst;
    return THEME_COLOR7;
#else
    return SDL_MapRGBA(dst->format, 0, 0, 0, 255);
#endif
}

// the scrollbar borrows the progress bar's track and fill, which are the
// system sliders' colors
static uint32_t track_color(void)
{
#ifdef PLATFORM_NEXTUI
    return THEME_COLOR3;
#else
    return RGB_DARK_GRAY;
#endif
}

static uint32_t thumb_color(void)
{
#ifdef PLATFORM_NEXTUI
    return THEME_COLOR1;
#else
    return RGB_WHITE;
#endif
}

// draw_text blits a line of text at (x, y), left-aligned inside w and
// truncated with an ellipsis if too wide. Returns the height used.
static int draw_text(SDL_Surface *dst, TTF_Font *f, const char *text, SDL_Color color,
                     int x, int y, int w)
{
    if (text == NULL || text[0] == '\0')
        return 0;

    char truncated[DETAIL_TEXT_MAX * 2];
    GFX_truncateText(f, text, truncated, w, 0);

    SDL_Surface *surface = TTF_RenderUTF8_Blended(f, truncated, color);
    if (surface == NULL)
        return 0;

    SDL_Rect pos = {x, y, surface->w, surface->h};
    SDL_BlitSurface(surface, NULL, dst, &pos);
    int h = surface->h;
    SDL_FreeSurface(surface);
    return h;
}

static int text_width(TTF_Font *f, const char *text)
{
    int w = 0;
    if (text != NULL && text[0] != '\0')
        TTF_SizeUTF8(f, text, &w, NULL);
    return w;
}

// measure_font is Text_Wrap's TextMeasure, backed by the font in ctx
static int measure_font(const char *s, size_t len, void *ctx)
{
    char buf[DETAIL_TEXT_MAX * 4];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    int w = 0;
    TTF_SizeUTF8((TTF_Font *)ctx, buf, &w, NULL);
    return w;
}

// load_cover reads the image, flattens it onto the background (so a PNG
// with transparency looks right without the scaler having to know about
// alpha), and box-filters it to fit inside box_w x box_h. Returns NULL if
// the image is missing or unreadable; the screen then goes without.
static SDL_Surface *load_cover(const char *path, int box_w, int box_h)
{
    if (path == NULL || path[0] == '\0')
        return NULL;
    SDL_Surface *img = IMG_Load(path);
    if (img == NULL)
    {
        fprintf(stderr, "minui-detail: could not load image '%s': %s\n", path, IMG_GetError());
        return NULL;
    }

    const uint32_t rm = 0x00FF0000, gm = 0x0000FF00, bm = 0x000000FF, am = 0xFF000000;
    SDL_Surface *flat = SDL_CreateRGBSurface(0, img->w, img->h, 32, rm, gm, bm, am);
    int dw, dh;
    Image_Fit(img->w, img->h, box_w, box_h, &dw, &dh);
    SDL_Surface *scaled = SDL_CreateRGBSurface(0, dw, dh, 32, rm, gm, bm, am);
    SDL_Surface *out = NULL;

    if (flat != NULL && scaled != NULL)
    {
        SDL_Color bg = background_rgb();
        SDL_FillRect(flat, NULL, SDL_MapRGBA(flat->format, bg.r, bg.g, bg.b, 255));
        SDL_BlitSurface(img, NULL, flat, NULL);

        SDL_LockSurface(flat);
        SDL_LockSurface(scaled);
        int ok = Image_Resample(flat->pixels, flat->w, flat->h, flat->pitch / 4,
                                scaled->pixels, dw, dh, scaled->pitch / 4);
        SDL_UnlockSurface(scaled);
        SDL_UnlockSurface(flat);

        if (ok)
            out = SDL_ConvertSurface(scaled, screen->format, 0);
    }

    SDL_FreeSurface(img);
    if (flat)
        SDL_FreeSurface(flat);
    if (scaled)
        SDL_FreeSurface(scaled);
    return out;
}

// round_corners paints the background over the four corners of rect outside
// a quarter circle of radius r, the same scanline inset fill_pill uses in
// minui-progress
static void round_corners(SDL_Surface *dst, SDL_Rect rect, int r, uint32_t color)
{
    if (r * 2 > rect.w)
        r = rect.w / 2;
    if (r * 2 > rect.h)
        r = rect.h / 2;
    for (int row = 0; row < r; row++)
    {
        double dy = r - (row + 0.5);
        int in = (int)(r - sqrt((double)r * r - dy * dy) + 0.5);
        if (in <= 0)
            continue;
        int top = rect.y + row;
        int bottom = rect.y + rect.h - 1 - row;
        SDL_FillRect(dst, &(SDL_Rect){rect.x, top, in, 1}, color);
        SDL_FillRect(dst, &(SDL_Rect){rect.x + rect.w - in, top, in, 1}, color);
        SDL_FillRect(dst, &(SDL_Rect){rect.x, bottom, in, 1}, color);
        SDL_FillRect(dst, &(SDL_Rect){rect.x + rect.w - in, bottom, in, 1}, color);
    }
}

static SDL_Color style_color(enum TextStyle style)
{
    if (style == StyleDim)
        return dim_text_color();
    if (style == StyleAccent)
        return accent_text_color();
    return text_color();
}

static void add_item(TTF_Font *f, enum TextStyle style, int x, int y, int w,
                     const char *text, size_t len)
{
    if (layout.item_count >= MAX_ITEMS)
        return;
    layout.items[layout.item_count++] = (struct Item){f, style, x, y, w, text, len};
}

// add_wrapped lays text out as lines of at most w, starting at y, and returns
// the y below it. With max_lines, whatever does not fit is run onto the last
// line, which draw_text then cuts short with an ellipsis.
static int add_wrapped(TTF_Font *f, enum TextStyle style, int x, int y, int w,
                       const char *text, int max_lines)
{
    static TextLine lines[MAX_ITEMS];
    if (max_lines <= 0 || max_lines > MAX_ITEMS)
        max_lines = MAX_ITEMS;
    int more = 0;
    int n = Text_Wrap(text, w, measure_font, f, lines, max_lines, &more);
    int line_h = TTF_FontLineSkip(f);
    for (int i = 0; i < n; i++)
    {
        size_t len = lines[i].len;
        if (i == n - 1 && more)
            len = strlen(text) - lines[i].start;
        add_item(f, style, x, y, w, text + lines[i].start, len);
        y += line_h;
    }
    return y;
}

// layout_column places the column's lines for a given width and returns the
// height they take up:
//
//   title           (large, up to two lines)
//   subtitle        (small, dimmed)
//   Label  value    (one row per field, labels dimmed, values wrapping)
//   note            (small, accent)
//   description     (small)
static int layout_column(const DetailDoc *doc, int x, int w, int title_w, int gap)
{
    layout.item_count = 0;
    int y = 0;

    y = add_wrapped(font.large, StyleText, x, y, title_w, doc->title, TITLE_MAX_LINES);
    if (doc->subtitle[0] != '\0')
        y = add_wrapped(font.small, StyleDim, x, y, w, doc->subtitle, 1);

    if (doc->field_count > 0 || doc->note[0] != '\0')
    {
        if (y > 0)
            y += gap;

        // labels line up in a column, as wide as the widest -- up to a point
        int label_w = 0;
        for (int i = 0; i < doc->field_count; i++)
        {
            int lw = text_width(font.small, doc->fields[i].label);
            if (lw > label_w)
                label_w = lw;
        }
        if (label_w > w * 2 / 5)
            label_w = w * 2 / 5;
        int value_x = x + (label_w > 0 ? label_w + gap : 0);
        int value_w = x + w - value_x;

        for (int i = 0; i < doc->field_count; i++)
        {
            const DetailField *f = &doc->fields[i];
            add_wrapped(font.small, StyleDim, x, y, label_w, f->label, 1);
            y = add_wrapped(font.small, StyleText, value_x, y, value_w, f->value, VALUE_MAX_LINES);
        }
        if (doc->note[0] != '\0')
            y = add_wrapped(font.small, StyleAccent, x, y, w, doc->note, 1);
    }

    if (doc->description[0] != '\0')
    {
        if (y > 0)
            y += gap;
        y = add_wrapped(font.small, StyleText, x, y, w, doc->description, 0);
    }
    return y;
}

static void build_layout(struct AppState *state)
{
    struct Layout *l = &layout;
    DetailDoc *doc = &state->doc;

    l->margin = SCALE1(PADDING);
    l->gap = SCALE1(PADDING);
    // the button hints sit on the bottom edge, PILL_SIZE tall, PADDING in
    int top = l->margin;
    int bottom = screen->h - SCALE1(PADDING + PILL_SIZE) - l->gap;

    int col_x = l->margin;
    int box_w = (int)(screen->w * COVER_MAX_FRACTION);
    l->cover = load_cover(doc->image, box_w, bottom - top);
    if (l->cover != NULL)
    {
        l->cover_rect = (SDL_Rect){l->margin, top, l->cover->w, l->cover->h};
        col_x = l->margin + l->cover->w + l->gap * 2;
    }
    int col_w = screen->w - col_x - l->margin;
    l->view = (SDL_Rect){col_x, top, col_w, bottom - top};

    // leave room for battery and wifi beside the title
    int reserve = state->show_hardware_group ? SCALE1(PILL_SIZE * 2) + l->gap : 0;
    l->scrollbar_top = state->show_hardware_group ? SCALE1(PILL_SIZE) + l->gap / 2 : 0;

    // lay out at the full width first; only if that overflows the window is
    // a scrollbar needed, and then the text wraps again beside it
    l->scrollbar = false;
    l->content_h = layout_column(doc, col_x, col_w, col_w - reserve, l->gap);
    if (l->content_h > l->view.h)
    {
        l->scrollbar = true;
        int w = col_w - SCALE1(SCROLLBAR_WIDTH) - l->gap;
        l->content_h = layout_column(doc, col_x, w, w - reserve, l->gap);
    }
    l->scroll = 0;
}

static int max_scroll(void)
{
    int m = layout.content_h - layout.view.h;
    return m > 0 ? m : 0;
}

static void draw_scrollbar(SDL_Surface *dst)
{
    struct Layout *l = &layout;
    int sw = SCALE1(SCROLLBAR_WIDTH);
    int x = l->view.x + l->view.w - sw;
    int top = l->view.y + l->scrollbar_top;
    int h = l->view.h - l->scrollbar_top;
    SDL_FillRect(dst, &(SDL_Rect){x, top, sw, h}, track_color());

    int thumb_h = h * l->view.h / l->content_h;
    if (thumb_h < SCALE1(8))
        thumb_h = SCALE1(8);
    int thumb_y = top + (h - thumb_h) * l->scroll / max_scroll();
    SDL_FillRect(dst, &(SDL_Rect){x, thumb_y, sw, thumb_h}, thumb_color());
}

void draw_screen(SDL_Surface *dst, struct AppState *state)
{
    struct Layout *l = &layout;

    SDL_FillRect(dst, NULL, background_color(dst));

    if (l->cover != NULL)
    {
        SDL_Rect r = l->cover_rect;
        SDL_BlitSurface(l->cover, NULL, dst, &r);
        round_corners(dst, l->cover_rect, SCALE1(COVER_RADIUS), background_color(dst));
    }

    // lines part-scrolled out of the window are cut at its edge
    SDL_Rect clip = l->view;
    SDL_SetClipRect(dst, &clip);
    for (int i = 0; i < l->item_count; i++)
    {
        struct Item *it = &l->items[i];
        int y = l->view.y + it->y - l->scroll;
        if (y + TTF_FontLineSkip(it->font) <= l->view.y || y >= l->view.y + l->view.h)
            continue;
        char buf[DETAIL_TEXT_MAX * 2];
        size_t len = it->len < sizeof(buf) - 1 ? it->len : sizeof(buf) - 1;
        memcpy(buf, it->text, len);
        buf[len] = '\0';
        draw_text(dst, it->font, buf, style_color(it->style), it->x, y, it->w);
    }
    SDL_SetClipRect(dst, NULL);

    if (l->scrollbar)
        draw_scrollbar(dst);

    // the action on the left, back and confirm on the right, as MinUI's own
    // screens place them
    if (state->action.text[0] != '\0')
        GFX_blitButtonGroup((char *[]){state->action.name, state->action.text, NULL}, 0, dst, 0);
    GFX_blitButtonGroup((char *[]){state->cancel.name, state->cancel.text,
                                   state->confirm.name, state->confirm.text, NULL},
                        1, dst, 1);

    state->redraw = 0;
}

static int button_for(const char *name)
{
    if (strcmp(name, "A") == 0)
        return BTN_A;
    if (strcmp(name, "B") == 0)
        return BTN_B;
    if (strcmp(name, "X") == 0)
        return BTN_X;
    if (strcmp(name, "Y") == 0)
        return BTN_Y;
    return BTN_NONE;
}

static void scroll_by(struct AppState *state, int px)
{
    int s = layout.scroll + px;
    if (s > max_scroll())
        s = max_scroll();
    if (s < 0)
        s = 0;
    if (s != layout.scroll)
    {
        layout.scroll = s;
        state->redraw = 1;
    }
}

void handle_input(struct AppState *state)
{
    PAD_poll();

    // up and down move a line; left, right, L1 and R1 a page, keeping one
    // line of the last page in view
    int line = TTF_FontLineSkip(font.small);
    int page = layout.view.h - line > line ? layout.view.h - line : line;
    if (PAD_justRepeated(BTN_UP))
        scroll_by(state, -line);
    else if (PAD_justRepeated(BTN_DOWN))
        scroll_by(state, line);
    else if (PAD_justRepeated(BTN_L1) || PAD_justRepeated(BTN_LEFT))
        scroll_by(state, -page);
    else if (PAD_justRepeated(BTN_R1) || PAD_justRepeated(BTN_RIGHT))
        scroll_by(state, page);

    // released rather than pressed, so the press doesn't carry over into
    // whatever screen the pak shows next
    if (PAD_justReleased(state->confirm.btn))
    {
        state->quitting = 1;
        state->exit_code = ExitCodeSuccess;
    }
    else if (PAD_justReleased(state->cancel.btn))
    {
        state->quitting = 1;
        state->exit_code = ExitCodeCancelButton;
    }
    else if (state->action.text[0] != '\0' && PAD_justReleased(state->action.btn))
    {
        state->quitting = 1;
        state->exit_code = ExitCodeActionButton;
    }
}

void signal_handler(int signal)
{
    if (signal == SIGINT)
        exit(ExitCodeKeyboardInterrupt);
    if (signal == SIGTERM)
        exit(ExitCodeSigterm);
    exit(ExitCodeError);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --file <path.json>\n"
            "          [--confirm-button A|B|X|Y] [--confirm-text <text>]\n"
            "          [--cancel-button A|B|X|Y] [--cancel-text <text>]\n"
            "          [--action-button A|B|X|Y] [--action-text <text>]\n"
            "          [--show-hardware-group]\n"
            "\n"
            "Shows the cover, title, fields and description in --file. Exits 0 on\n"
            "confirm, 2 on cancel, 4 on the action (shown only with --action-text).\n",
            argv0);
}

static bool set_button(struct Button *b, const char *which)
{
    b->btn = button_for(b->name);
    if (b->btn == BTN_NONE)
    {
        fprintf(stderr, "minui-detail: --%s-button must be A, B, X or Y\n", which);
        return false;
    }
    return true;
}

bool parse_arguments(struct AppState *state, int argc, char *argv[])
{
    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"confirm-button", required_argument, 0, 'a'},
        {"confirm-text", required_argument, 0, 'A'},
        {"cancel-button", required_argument, 0, 'b'},
        {"cancel-text", required_argument, 0, 'B'},
        {"action-button", required_argument, 0, 'x'},
        {"action-text", required_argument, 0, 'X'},
        {"show-hardware-group", no_argument, 0, 'H'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "f:a:A:b:B:x:X:Hh", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'f':
            strncpy(state->file, optarg, sizeof(state->file) - 1);
            break;
        case 'a':
            strncpy(state->confirm.name, optarg, sizeof(state->confirm.name) - 1);
            break;
        case 'A':
            strncpy(state->confirm.text, optarg, sizeof(state->confirm.text) - 1);
            break;
        case 'b':
            strncpy(state->cancel.name, optarg, sizeof(state->cancel.name) - 1);
            break;
        case 'B':
            strncpy(state->cancel.text, optarg, sizeof(state->cancel.text) - 1);
            break;
        case 'x':
            strncpy(state->action.name, optarg, sizeof(state->action.name) - 1);
            break;
        case 'X':
            strncpy(state->action.text, optarg, sizeof(state->action.text) - 1);
            break;
        case 'H':
            state->show_hardware_group = true;
            break;
        default:
            usage(argv[0]);
            return false;
        }
    }

    if (state->file[0] == '\0')
    {
        log_error("minui-detail: --file is required");
        usage(argv[0]);
        return false;
    }
    if (!set_button(&state->confirm, "confirm") || !set_button(&state->cancel, "cancel") ||
        !set_button(&state->action, "action"))
        return false;
    if (state->confirm.btn == state->cancel.btn ||
        (state->action.text[0] != '\0' &&
         (state->action.btn == state->confirm.btn || state->action.btn == state->cancel.btn)))
    {
        log_error("minui-detail: the confirm, cancel and action buttons must differ");
        return false;
    }

    return true;
}

// read_file returns the whole file, NUL-terminated, or NULL
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    while (buf != NULL)
    {
        size_t n = fread(buf + len, 1, cap - len - 1, f);
        len += n;
        if (len < cap - 1)
            break;
        cap *= 2;
        char *grown = realloc(buf, cap);
        if (grown == NULL)
        {
            free(buf);
            buf = NULL;
            break;
        }
        buf = grown;
    }
    fclose(f);
    if (buf != NULL)
        buf[len] = '\0';
    return buf;
}

// MinUI's init and teardown print to stdout on some platforms; the caller is
// usually a script whose stdout is a log or a command substitution, so both
// run with stdout and stderr pointed at /dev/null.
static void quietly(void (*func)(void))
{
    int out = dup(STDOUT_FILENO);
    int err = dup(STDERR_FILENO);
    fcntl(out, F_SETFD, FD_CLOEXEC);
    fcntl(err, F_SETFD, FD_CLOEXEC);
    int devnull = open("/dev/null", O_WRONLY);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    func();

    fflush(stdout);
    fflush(stderr);
    dup2(out, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
    close(out);
    close(err);
}

static void init(void)
{
    PWR_setCPUSpeed(CPU_SPEED_MENU);
    screen = GFX_init(MODE_MAIN);
    PAD_init();
    PWR_init();
    InitSettings();
}

static void destruct(void)
{
    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();
}

int main(int argc, char *argv[])
{
    struct AppState state;
    memset(&state, 0, sizeof(state));
    state.redraw = 1;
    state.exit_code = ExitCodeSuccess;
    strncpy(state.confirm.name, "A", sizeof(state.confirm.name) - 1);
    strncpy(state.confirm.text, "SELECT", sizeof(state.confirm.text) - 1);
    strncpy(state.cancel.name, "B", sizeof(state.cancel.name) - 1);
    strncpy(state.cancel.text, "BACK", sizeof(state.cancel.text) - 1);
    strncpy(state.action.name, "X", sizeof(state.action.name) - 1);

    if (!parse_arguments(&state, argc, argv))
        return ExitCodeError;

    // read and parse before touching the screen, so a bad file is reported
    // as an error without flashing a blank screen first
    char *json = read_file(state.file);
    if (json == NULL)
    {
        fprintf(stderr, "minui-detail: could not read '%s'\n", state.file);
        return ExitCodeError;
    }
    char err[256];
    int parsed = DetailDoc_Parse(json, &state.doc, err, sizeof(err));
    free(json);
    if (!parsed)
    {
        fprintf(stderr, "minui-detail: %s: %s\n", state.file, err);
        DetailDoc_Free(&state.doc);
        return ExitCodeError;
    }

    quietly(init);

    struct sigaction sa = {.sa_handler = signal_handler, .sa_flags = SA_RESTART};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    build_layout(&state);
    // development aid, alongside MINUI_DETAIL_SNAPSHOT: start scrolled down
    // this many pixels, to check the view part-way down without a d-pad
    const char *start_scroll = getenv("MINUI_DETAIL_SCROLL");
    if (start_scroll != NULL)
        scroll_by(&state, atoi(start_scroll));

    int was_online = PLAT_isOnline();
    int show_setting = 0;

    while (!state.quitting)
    {
        GFX_startFrame();
        PWR_update(&state.redraw, &show_setting, NULL, NULL);

        int is_online = PLAT_isOnline();
        if (was_online != is_online)
            state.redraw = 1;
        was_online = is_online;

        handle_input(&state);
        if (state.quitting)
            break;

        if (state.redraw)
        {
            GFX_clear(screen);
            draw_screen(screen, &state);
            if (state.show_hardware_group)
                GFX_blitHardwareGroup(screen, show_setting);
            // development aid: MINUI_DETAIL_SNAPSHOT=<path.bmp> saves every
            // drawn frame, so the layout can be checked without a device
            const char *snapshot = getenv("MINUI_DETAIL_SNAPSHOT");
            if (snapshot != NULL && snapshot[0] != '\0')
            {
                // written aside and renamed, so a reader never catches a
                // half-written frame
                char tmp[1100];
                snprintf(tmp, sizeof(tmp), "%s.tmp", snapshot);
                if (SDL_SaveBMP(screen, tmp) == 0)
                    rename(tmp, snapshot);
            }
            GFX_flip(screen);
        }
        else
        {
            GFX_sync();
        }
    }

    if (layout.cover != NULL)
        SDL_FreeSurface(layout.cover);
    DetailDoc_Free(&state.doc);
    quietly(destruct);
    return state.exit_code;
}
