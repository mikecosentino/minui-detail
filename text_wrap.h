#ifndef TEXT_WRAP_H
#define TEXT_WRAP_H

#include <stddef.h>

// TextMeasure returns the drawn width of len bytes of UTF-8 starting at s.
// The app backs this with TTF_SizeUTF8; the tests with a fixed-width fake.
typedef int (*TextMeasure)(const char *s, size_t len, void *ctx);

typedef struct
{
    size_t start;
    size_t len;
} TextLine;

// Text_Wrap breaks text into lines no wider than max_w, greedily, at spaces.
// A newline always ends a line, and a blank line between paragraphs is kept
// as an empty line. A word wider than max_w on its own is broken between
// characters (never inside a UTF-8 sequence). Spaces at a break are dropped.
//
// Writes at most max_lines lines to out and returns how many it wrote. When
// the text did not fit, *more is set to 1 (it may be NULL).
int Text_Wrap(const char *text, int max_w, TextMeasure measure, void *ctx,
              TextLine *out, int max_lines, int *more);

#endif
