#ifndef __IMAGE_H__
#define __IMAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// Image display constants
#define IMAGE_WIDTH 480
#define IMAGE_HEIGHT 272
#define IMAGE_PIXEL_COUNT (IMAGE_WIDTH * IMAGE_HEIGHT)
#define IMAGE_DATA_SIZE (IMAGE_PIXEL_COUNT * sizeof(uint16_t))

// Function declarations
void display_init(void);
void display_default_image(const uint16_t *default_image_data);

#ifdef __cplusplus
}
#endif

#endif // __IMAGE_H__ 