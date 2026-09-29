// Unit tests for detail_doc.c.

#include "detail_doc.h"

#include <string.h>

#include "check.h"

int main(void)
{
    DetailDoc d;
    char err[128];

    CHECK(DetailDoc_Parse(
              "{\"title\":\"Final Fantasy VII\",\"subtitle\":\"Square \\u00b7 1997\","
              "\"image\":\"/tmp/c.png\",\"note\":\"Already downloaded\","
              "\"description\":\"Line one.\\n\\nLine two.\","
              "\"fields\":[{\"label\":\"Genre\",\"value\":\"RPG\"},"
              "{\"label\":\"Year\",\"value\":1997},"
              "{\"label\":\"Rating\",\"value\":8.5},"
              "{\"label\":\"Empty\",\"value\":\"\"},"
              "{\"label\":\"Null\",\"value\":null},"
              "{\"label\":\"Missing\"}]}",
              &d, err, sizeof(err)) == 1);
    CHECK(strcmp(d.title, "Final Fantasy VII") == 0);
    CHECK(strcmp(d.subtitle, "Square \xc2\xb7 1997") == 0);
    CHECK(strcmp(d.image, "/tmp/c.png") == 0);
    CHECK(strcmp(d.note, "Already downloaded") == 0);
    CHECK(strcmp(d.description, "Line one.\n\nLine two.") == 0);
    // fields without a value are dropped; numbers become text
    CHECK(d.field_count == 3);
    CHECK(strcmp(d.fields[0].label, "Genre") == 0 && strcmp(d.fields[0].value, "RPG") == 0);
    CHECK(strcmp(d.fields[1].value, "1997") == 0);
    CHECK(strcmp(d.fields[2].value, "8.5") == 0);
    DetailDoc_Free(&d);

    // everything optional
    CHECK(DetailDoc_Parse("{}", &d, err, sizeof(err)) == 1);
    CHECK(d.title[0] == '\0' && d.field_count == 0);
    CHECK(d.description != NULL && d.description[0] == '\0');
    DetailDoc_Free(&d);

    // wrong types are ignored rather than fatal
    CHECK(DetailDoc_Parse("{\"title\":{\"x\":1},\"fields\":\"nope\",\"description\":5}", &d, err, sizeof(err)) == 1);
    CHECK(d.title[0] == '\0' && d.field_count == 0 && d.description[0] == '\0');
    DetailDoc_Free(&d);

    // more fields than fit are cut off, not overflowed
    {
        char big[8192] = "{\"fields\":[";
        for (int i = 0; i < DETAIL_FIELDS_MAX + 5; i++)
            strcat(big, i ? ",{\"label\":\"L\",\"value\":\"v\"}" : "{\"label\":\"L\",\"value\":\"v\"}");
        strcat(big, "]}");
        CHECK(DetailDoc_Parse(big, &d, err, sizeof(err)) == 1);
        CHECK(d.field_count == DETAIL_FIELDS_MAX);
        DetailDoc_Free(&d);
    }

    // screenshots keep their order, skip empty paths, and stop at the limit
    {
        char big[8192] = "{\"screenshots\":[\"/a.jpg\",\"\",null,\"/b.jpg\"";
        for (int i = 0; i < DETAIL_SHOTS_MAX + 3; i++)
            strcat(big, ",\"/c.jpg\"");
        strcat(big, "]}");
        CHECK(DetailDoc_Parse(big, &d, err, sizeof(err)) == 1);
        CHECK(d.shot_count == DETAIL_SHOTS_MAX);
        CHECK(strcmp(d.shots[0], "/a.jpg") == 0 && strcmp(d.shots[1], "/b.jpg") == 0);
        DetailDoc_Free(&d);
    }
    CHECK(DetailDoc_Parse("{\"screenshots\":\"/a.jpg\"}", &d, err, sizeof(err)) == 1);
    CHECK(d.shot_count == 0);
    DetailDoc_Free(&d);

    CHECK(DetailDoc_Parse("{not json", &d, err, sizeof(err)) == 0);
    CHECK(err[0] != '\0');
    DetailDoc_Free(&d);
    CHECK(DetailDoc_Parse("[1,2]", &d, err, sizeof(err)) == 0);
    DetailDoc_Free(&d);

    if (failures)
        return 1;
    printf("detail_doc: all tests passed\n");
    return 0;
}
