// Unit tests for text_wrap.c, measuring every UTF-8 character as 1 unit wide.

#include "text_wrap.h"

#include <string.h>

#include "check.h"

static int measure(const char *s, size_t len, void *ctx)
{
    (void)ctx;
    int n = 0;
    for (size_t i = 0; i < len; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80)
            n++;
    return n;
}

static TextLine lines[32];
static const char *src;

static int wrap(const char *text, int w, int max, int *more)
{
    src = text;
    return Text_Wrap(text, w, measure, NULL, lines, max, more);
}

static int line_is(int i, const char *want)
{
    return lines[i].len == strlen(want) && memcmp(src + lines[i].start, want, lines[i].len) == 0;
}

int main(void)
{
    int more, n;

    n = wrap("the quick brown fox jumps", 10, 32, &more);
    CHECK(n == 3 && !more);
    CHECK(line_is(0, "the quick"));
    CHECK(line_is(1, "brown fox"));
    CHECK(line_is(2, "jumps"));

    // exactly the width fits
    n = wrap("abcde fghij", 5, 32, &more);
    CHECK(n == 2 && line_is(0, "abcde") && line_is(1, "fghij"));

    // extra spaces at a break are dropped
    n = wrap("  one    two  ", 5, 32, &more);
    CHECK(n == 2 && line_is(0, "one") && line_is(1, "two"));

    // newlines end lines; a blank line between paragraphs is kept; a trailing
    // newline adds nothing
    n = wrap("one\n\ntwo\n", 20, 32, &more);
    CHECK(n == 3 && line_is(0, "one") && line_is(1, "") && line_is(2, "two"));

    // a word too long for a line is broken between characters
    n = wrap("abcdefghij xy", 4, 32, &more);
    CHECK(n == 4 && line_is(0, "abcd") && line_is(1, "efgh") && line_is(2, "ij") && line_is(3, "xy"));

    // ... but never inside a UTF-8 sequence
    n = wrap("\xc3\xa9\xc3\xa9\xc3\xa9", 2, 32, &more);
    CHECK(n == 2 && line_is(0, "\xc3\xa9\xc3\xa9") && line_is(1, "\xc3\xa9"));

    // a line narrower than one character still makes progress
    n = wrap("abc", 0, 32, &more);
    CHECK(n == 3 && line_is(0, "a") && line_is(2, "c"));

    // running out of lines says so
    n = wrap("a b c d", 1, 2, &more);
    CHECK(n == 2 && more && line_is(0, "a") && line_is(1, "b"));
    n = wrap("a b", 1, 2, &more);
    CHECK(n == 2 && !more);

    n = wrap("", 10, 32, &more);
    CHECK(n == 0 && !more);
    n = wrap(NULL, 10, 32, NULL);
    CHECK(n == 0);

    if (failures)
        return 1;
    printf("text_wrap: all tests passed\n");
    return 0;
}
