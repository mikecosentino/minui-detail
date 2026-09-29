#include "detail_doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

// copy_value writes a string or number item into out, and anything else
// (missing, null, an object) as empty. Numbers are allowed because a writer
// building this with jq will hand over a year as 1997 as often as "1997".
static void copy_value(const cJSON *item, char *out, size_t out_len)
{
    out[0] = '\0';
    if (cJSON_IsString(item) && item->valuestring != NULL)
    {
        snprintf(out, out_len, "%s", item->valuestring);
    }
    else if (cJSON_IsNumber(item))
    {
        double v = item->valuedouble;
        if (v == (double)(long long)v)
            snprintf(out, out_len, "%lld", (long long)v);
        else
            snprintf(out, out_len, "%g", v);
    }
}

int DetailDoc_Parse(const char *json, DetailDoc *doc, char *err, size_t err_len)
{
    memset(doc, 0, sizeof(*doc));
    if (err_len > 0)
        err[0] = '\0';

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        snprintf(err, err_len, "not valid JSON");
        return 0;
    }
    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        snprintf(err, err_len, "expected a JSON object at the top level");
        return 0;
    }

    copy_value(cJSON_GetObjectItemCaseSensitive(root, "title"), doc->title, sizeof(doc->title));
    copy_value(cJSON_GetObjectItemCaseSensitive(root, "subtitle"), doc->subtitle, sizeof(doc->subtitle));
    copy_value(cJSON_GetObjectItemCaseSensitive(root, "image"), doc->image, sizeof(doc->image));
    copy_value(cJSON_GetObjectItemCaseSensitive(root, "note"), doc->note, sizeof(doc->note));

    const cJSON *desc = cJSON_GetObjectItemCaseSensitive(root, "description");
    doc->description = strdup(cJSON_IsString(desc) && desc->valuestring ? desc->valuestring : "");

    const cJSON *fields = cJSON_GetObjectItemCaseSensitive(root, "fields");
    const cJSON *field;
    cJSON_ArrayForEach(field, fields)
    {
        if (doc->field_count >= DETAIL_FIELDS_MAX)
            break;
        DetailField *f = &doc->fields[doc->field_count];
        copy_value(cJSON_GetObjectItemCaseSensitive(field, "label"), f->label, sizeof(f->label));
        copy_value(cJSON_GetObjectItemCaseSensitive(field, "value"), f->value, sizeof(f->value));
        if (f->value[0] != '\0')
            doc->field_count++;
    }

    const cJSON *shots = cJSON_GetObjectItemCaseSensitive(root, "screenshots");
    const cJSON *shot;
    cJSON_ArrayForEach(shot, shots)
    {
        if (doc->shot_count >= DETAIL_SHOTS_MAX)
            break;
        char *out = doc->shots[doc->shot_count];
        copy_value(shot, out, DETAIL_PATH_MAX);
        if (out[0] != '\0')
            doc->shot_count++;
    }

    cJSON_Delete(root);
    if (doc->description == NULL)
    {
        snprintf(err, err_len, "out of memory");
        return 0;
    }
    return 1;
}

void DetailDoc_Free(DetailDoc *doc)
{
    free(doc->description);
    doc->description = NULL;
}
