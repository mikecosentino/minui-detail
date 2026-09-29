// Unit tests for image_fit.c.

#include "image_fit.h"

#include "check.h"

int main(void)
{
    int w, h;

    Image_Fit(528, 748, 300, 600, &w, &h); // tall cover in a taller box
    CHECK(w == 300 && h == 425);
    Image_Fit(528, 748, 600, 300, &w, &h); // in a wide box
    CHECK(w == 211 && h == 300);
    Image_Fit(100, 100, 50, 50, &w, &h);
    CHECK(w == 50 && h == 50);
    Image_Fit(10, 10, 40, 30, &w, &h); // grows to fit
    CHECK(w == 30 && h == 30);
    Image_Fit(0, 10, 40, 30, &w, &h);
    CHECK(w == 1 && h == 1);

    // same size copies exactly
    uint32_t src[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};
    uint32_t dst[4] = {0};
    CHECK(Image_Resample(src, 2, 2, 2, dst, 2, 2, 2));
    CHECK(dst[0] == src[0] && dst[1] == src[1] && dst[2] == src[2] && dst[3] == src[3]);

    // 2x2 down to 1x1 averages every channel on its own
    uint32_t quad[4] = {0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c};
    CHECK(Image_Resample(quad, 2, 2, 2, dst, 1, 1, 1));
    CHECK(dst[0] == 0x06060606);

    // 3 wide down to 2: each output takes one whole pixel and half the middle
    uint32_t row3[3] = {0x00, 0x60, 0xc0};
    CHECK(Image_Resample(row3, 3, 1, 3, dst, 2, 1, 2));
    CHECK(dst[0] == 0x20 && dst[1] == 0xa0);

    // src pitch wider than the image is respected
    uint32_t padded[6] = {0x10, 0x10, 0xff, 0x30, 0x30, 0xff};
    CHECK(Image_Resample(padded, 2, 2, 3, dst, 1, 1, 1));
    CHECK(dst[0] == 0x20);

    // growing keeps a flat colour flat
    uint32_t one[1] = {0x80402010};
    uint32_t big[9];
    CHECK(Image_Resample(one, 1, 1, 1, big, 3, 3, 3));
    for (int i = 0; i < 9; i++)
        CHECK(big[i] == 0x80402010);

    if (failures)
        return 1;
    printf("image_fit: all tests passed\n");
    return 0;
}
