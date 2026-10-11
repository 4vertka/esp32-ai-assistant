#include "display_lcd.h"
#include <stdint.h>

#include <hd44780.h>

static hd44780_t lcd = {
              .write_cb = NULL,
                      .font = HD44780_FONT_5X8,
              .lines = 2,
              .pins = {
                  .rs = 19,
                  .e  = 23,
                  .d4 = 18,
                  .d5 = 17,
                .d6 = 16,
                .d7 = 15,
                .bl = HD44780_NOT_USED
            },
        };

void display_lcd_init(void) {
    /*static hd44780_t lcd = {
            .write_cb = NULL,
                    .font = HD44780_FONT_5X8,
            .lines = 2,
            .pins = {
                .rs = 19,
                .e  = 23,
                .d4 = 18,
                .d5 = 17,
              .d6 = 16,
              .d7 = 15,
              .bl = HD44780_NOT_USED
          },
      };*/
    
    ESP_ERROR_CHECK(hd44780_init(&lcd));
}

void display_lcd_write(char* buffer) {

    char *top_line_buffer = &buffer[0];
    char *bottom_line_buffer = &buffer[16];

    hd44780_puts(&lcd, top_line_buffer);
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, bottom_line_buffer);
}
