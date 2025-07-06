#include "image.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

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
    HAL_LTDC_SetAddress(&hltdc, (uint32_t)framebuffer, 0);
    
    printf("Display initialized at address 0x%08lX\r\n", (uint32_t)framebuffer);
}

void display_default_image(const uint16_t *default_image_data)
{
    // Copy the default image data to framebuffer
    memcpy(framebuffer, default_image_data, IMAGE_DATA_SIZE);
    
    // Update LTDC display
    HAL_LTDC_SetAddress(&hltdc, (uint32_t)framebuffer, 0);
    
    printf("Default image displayed (%lu bytes)\r\n", (unsigned long)IMAGE_DATA_SIZE);
}
