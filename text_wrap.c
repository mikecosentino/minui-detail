#include "text_wrap.h"

#include <string.h>

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

// next_char steps past the UTF-8 sequence starting at s[i]
static size_t next_char(const char *s, size_t i, size_t end)
{
    i++;
    while (i < end && ((unsigned char)s[i] & 0xC0) == 0x80)
        i++;
    return i;
}

static size_t skip_spaces(const char *s, size_t i, size_t end)
{
    while (i < end && is_space(s[i]))
        i++;
    return i;
}

static size_t word_end(const char *s, size_t i, size_t end)
{
    while (i < end && !is_space(s[i]))
        i++;
    return i;
}

struct Wrap
{
    const char *text;
    int max_w;
    TextMeasure measure;
    void *ctx;
    TextLine *out;
    int max_lines;
    int count;
    int full;
};

// emit adds a line, or notes that there was no room for it
static int emit(struct Wrap *w, size_t start, size_t len)
{
    if (w->count >= w->max_lines)
    {
        w->full = 1;
        return 0;
    }
    w->out[w->count].start = start;
    w->out[w->count].len = len;
    w->count++;
    return 1;
}

static int fits(struct Wrap *w, size_t start, size_t end)
{
    return w->measure(w->text + start, end - start, w->ctx) <= w->max_w;
}

// wrap_paragraph wraps [p0, p1), which holds no newline. Returns 0 once the
// output is full.
static int wrap_paragraph(struct Wrap *w, size_t p0, size_t p1)
{
    const char *s = w->text;
    size_t pos = skip_spaces(s, p0, p1);
    if (pos >= p1)
        return emit(w, p0, 0);

    while (pos < p1)
    {
        size_t start = pos;
        size_t end = word_end(s, pos, p1);

        if (!fits(w, start, end))
        {
            // one word too wide for a line by itself: take as many whole
            // characters as fit, and always at least one
            size_t k = next_char(s, start, end);
            while (k < end)
            {
                size_t next = next_char(s, k, end);
                if (!fits(w, start, next))
                    break;
                k = next;
            }
            if (!emit(w, start, k - start))
                return 0;
            pos = k;
            continue;
        }

        for (;;)
        {
            size_t ns = skip_spaces(s, end, p1);
            if (ns >= p1)
                break;
            size_t ne = word_end(s, ns, p1);
            if (!fits(w, start, ne))
                break;
            end = ne;
        }
        if (!emit(w, start, end - start))
            return 0;
        pos = skip_spaces(s, end, p1);
    }
    return 1;
}

int Text_Wrap(const char *text, int max_w, TextMeasure measure, void *ctx,
              TextLine *out, int max_lines, int *more)
{
    struct Wrap w = {text, max_w, measure, ctx, out, max_lines, 0, 0};
    size_t total = text ? strlen(text) : 0;

    size_t p0 = 0;
    while (p0 < total)
    {
        const char *nl = memchr(text + p0, '\n', total - p0);
        size_t p1 = nl ? (size_t)(nl - text) : total;
        if (!wrap_paragraph(&w, p0, p1))
            break;
        p0 = p1 + 1;
    }

    if (more)
        *more = w.full;
    return w.count;
}
