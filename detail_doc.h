#ifndef DETAIL_DOC_H
#define DETAIL_DOC_H

#include <stddef.h>

// The screen minui-detail draws, as read from its --file:
//
//   {
//     "title":       "Final Fantasy VII",
//     "subtitle":    "Square · 1997",
//     "image":       "/mnt/SDCARD/.../co1abc.png",
//     "fields":      [ {"label": "Genre", "value": "Role-playing (RPG)"}, ... ],
//     "note":        "Already downloaded",
//     "description": "The world has fallen under the control of ..."
//   }
//
// Every key is optional. A field with no value is left out rather than drawn
// as a bare label, so the writer can pass through whatever it has without
// filtering first. Values may be strings or numbers.

#define DETAIL_TEXT_MAX 512
#define DETAIL_PATH_MAX 1024
#define DETAIL_FIELDS_MAX 24

typedef struct
{
    char label[128];
    char value[DETAIL_TEXT_MAX];
} DetailField;

typedef struct
{
    char title[DETAIL_TEXT_MAX];
    char subtitle[DETAIL_TEXT_MAX];
    char image[DETAIL_PATH_MAX];
    char note[DETAIL_TEXT_MAX];
    // heap-allocated, since a summary has no sensible fixed bound; never NULL
    // after a successful parse
    char *description;

    DetailField fields[DETAIL_FIELDS_MAX];
    int field_count;
} DetailDoc;

// DetailDoc_Parse fills *doc from the JSON text. Returns 1 on success; on
// failure returns 0 and writes a one-line reason to err. Either way *doc is
// safe to pass to DetailDoc_Free.
int DetailDoc_Parse(const char *json, DetailDoc *doc, char *err, size_t err_len);

void DetailDoc_Free(DetailDoc *doc);

#endif
