#include "image.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

const uint16_t default_image_data[] = {
#include "default_image_rgb565.h"
};

// Frame buffer in SDRAM - this will be placed first in the .sdram section
uint16_t framebuffer[IMAGE_PIXEL_COUNT] __attribute__((section(".sdram")));

// External variables
extern LTDC_HandleTypeDef hltdc;

void display_init(void)
{
    // Enable LCD backlight and display
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);

    // Set LTDC to use our framebuffer
    HAL_LTDC_SetAddress(&hltdc, (uint32_t) framebuffer, 0);

    printf("Display initialized at address 0x%08lX\r\n", (uint32_t) framebuffer);
}

void display_image(const uint16_t *image_data, size_t data_size)
{
    if (data_size != IMAGE_DATA_SIZE) {
        printf("USART6 Error: Invalid image data size %zu, expected %d\r\n", data_size, IMAGE_DATA_SIZE);
        return;
    }

    // Copy image data to framebuffer
    memcpy(framebuffer, image_data, IMAGE_DATA_SIZE);

    // Update LTDC display
    HAL_LTDC_SetAddress(&hltdc, (uint32_t) framebuffer, 0);

    printf("USART6 Display updated with new image (%lu bytes)\r\n", (unsigned long) data_size);
}

void display_default_image()
{
    // Copy default image data to framebuffer
    memcpy(framebuffer, default_image_data, IMAGE_DATA_SIZE);

    // Update display
    update_display();

    printf("Default image displayed (%lu bytes)\r\n", (unsigned long) IMAGE_DATA_SIZE);
}

uint8_t *get_image_buffer(void)
{
    return (uint8_t *) framebuffer;
}

void clear_image_buffer(void)
{
    memset(framebuffer, 0, IMAGE_DATA_SIZE);
    printf("Image buffer cleared\r\n");
}

void update_display(void)
{
    // Update LTDC display
    HAL_LTDC_SetAddress(&hltdc, (uint32_t) framebuffer, 0);
    printf("Display updated\r\n");
}