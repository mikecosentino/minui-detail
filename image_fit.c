#include "image_fit.h"

#include <math.h>
#include <stdlib.h>

void Image_Fit(int src_w, int src_h, int box_w, int box_h, int *out_w, int *out_h)
{
    if (src_w <= 0 || src_h <= 0 || box_w <= 0 || box_h <= 0)
    {
        *out_w = 1;
        *out_h = 1;
        return;
    }
    // compare src_w/src_h with box_w/box_h without dividing
    if ((long long)src_w * box_h > (long long)box_w * src_h)
    {
        *out_w = box_w;
        *out_h = (int)((long long)src_h * box_w / src_w);
    }
    else
    {
        *out_h = box_h;
        *out_w = (int)((long long)src_w * box_h / src_h);
    }
    if (*out_w < 1)
        *out_w = 1;
    if (*out_h < 1)
        *out_h = 1;
}

// resample_line box-filters n_src samples of 4 channels (stride src_step
// floats apart) into n_dst samples (stride dst_step)
static void resample_line(const float *src, int n_src, int src_step,
                          float *dst, int n_dst, int dst_step)
{
    double scale = (double)n_src / n_dst;
    for (int d = 0; d < n_dst; d++)
    {
        double s0 = d * scale;
        double s1 = (d + 1) * scale;
        int i0 = (int)floor(s0);
        int i1 = (int)ceil(s1);
        if (i1 > n_src)
            i1 = n_src;

        float acc[4] = {0, 0, 0, 0};
        double total = 0;
        for (int i = i0; i < i1; i++)
        {
            double lo = i > s0 ? i : s0;
            double hi = i + 1 < s1 ? i + 1 : s1;
            float wgt = (float)(hi - lo);
            if (wgt <= 0)
                continue;
            const float *p = src + (long)i * src_step;
            for (int c = 0; c < 4; c++)
                acc[c] += p[c] * wgt;
            total += wgt;
        }
        float *q = dst + (long)d * dst_step;
        for (int c = 0; c < 4; c++)
            q[c] = total > 0 ? (float)(acc[c] / total) : 0;
    }
}

int Image_Resample(const uint32_t *src, int src_w, int src_h, int src_pitch,
                   uint32_t *dst, int dst_w, int dst_h, int dst_pitch)
{
    // horizontal pass into a float image dst_w x src_h, then vertical pass
    float *row = malloc(sizeof(float) * 4 * (src_w > src_h ? src_w : src_h));
    float *mid = malloc(sizeof(float) * 4 * (size_t)dst_w * src_h);
    float *col = malloc(sizeof(float) * 4 * (dst_h > src_h ? dst_h : src_h));
    if (row == NULL || mid == NULL || col == NULL)
    {
        free(row);
        free(mid);
        free(col);
        return 0;
    }

    for (int y = 0; y < src_h; y++)
    {
        const uint32_t *s = src + (long)y * src_pitch;
        for (int x = 0; x < src_w; x++)
            for (int c = 0; c < 4; c++)
                row[x * 4 + c] = (float)((s[x] >> (c * 8)) & 0xFF);
        resample_line(row, src_w, 4, mid + (size_t)y * dst_w * 4, dst_w, 4);
    }

    for (int x = 0; x < dst_w; x++)
    {
        resample_line(mid + x * 4, src_h, dst_w * 4, col, dst_h, 4);
        for (int y = 0; y < dst_h; y++)
        {
            uint32_t px = 0;
            for (int c = 0; c < 4; c++)
            {
                int v = (int)(col[y * 4 + c] + 0.5f);
                if (v < 0)
                    v = 0;
                if (v > 255)
                    v = 255;
                px |= (uint32_t)v << (c * 8);
            }
            dst[(long)y * dst_pitch + x] = px;
        }
    }

    free(row);
    free(mid);
    free(col);
    return 1;
}
