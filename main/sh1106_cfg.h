/*
 * File: sh1106_cfg.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-16
 */

#ifndef __SH1106_CFG_H__
#define __SH1106_CFG_H__

/*==================================================================
    Includes
===================================================================*/

/*==================================================================
    Object-like macros
===================================================================*/

/* Log tag */
#define SH1106_TAG "SH1106"
/* I2C address of SH1106 controller chip */
#define SH1106_SENSOR_ADDR 0x3C
/* RAM addresses does not correspond exactly to OLED columns, so a shift needs to be specified */
#define COLUMN_SHIFT 2U
/* Max. number of data bytes that SH1106 can hold in input buffer */
#define DATA_BUFFER_MAX 31U

/* Display dimensions ion pixels */
#define DISPLAY_WIDTH 128U
#define DISPLAY_HEIGHT 64U
#define PAGE_HEIGHT 8U

/* Display address ranges */
#define PAGE_ADDRESS_MIN 0U
#define PAGE_ADDRESS_MAX 7U
#define COLUMN_ADDRESS_MIN 0U
#define COLUMN_ADDRESS_MAX (DISPLAY_WIDTH - 1U)

/* Font dimensions in pixels */
#define FONT_WIDTH_SMALL 5U
#define FONT_HEIGHT_SMALL 8U

#define FONT_WIDTH_BIG 10U
#define FONT_HEIGHT_BIG 16U

#define FONT_WIDTH_VERY_BIG 16U
#define FONT_HEIGHT_VERY_BIG 24U

/* Sizes of display segments */
#define TX_FREQ_FONT_HEIGHT (FONT_HEIGHT_VERY_BIG)
#define TX_FREQ_FONT_WIDTH (FONT_WIDTH_VERY_BIG)

#define PLAYBACK_STATE_FONT_HEIGHT (FONT_HEIGHT_VERY_BIG)
#define PLAYBACK_STATE_FONT_WIDTH (FONT_WIDTH_VERY_BIG)

/* Display layout */
#define TX_FREQ_PAGE_ADDRESS 1U
#define PLAYBACK_STATE_PAGE_ADDRESS (TX_FREQ_PAGE_ADDRESS + (TX_FREQ_FONT_HEIGHT / PAGE_HEIGHT) + 1)

#define TX_FREQ_COLUMN_ADDRESS COLUMN_ADDRESS_MIN
#define PLAYBACK_STATE_COLUMN_ADDRESS ((DISPLAY_WIDTH / 2) - (FONT_WIDTH_BIG / 2))

#define ASCII_PRINTABLE_CHAR_OFFSET 0x20U // " " (space) = 0x20
#define ASCII_PRINTABLE_CHAR_COUNT 95U    // 95 printable characters: 0x20-0x7E

/*==================================================================
    Function-like macros
===================================================================*/

/*==================================================================
    Exported types
===================================================================*/

/*==================================================================
    Exported objects
===================================================================*/

extern const uint8_t sh1106_font_characters_small[ASCII_PRINTABLE_CHAR_COUNT][FONT_WIDTH_SMALL * (FONT_HEIGHT_SMALL / PAGE_HEIGHT)];
extern const uint8_t sh1106_font_characters_big[ASCII_PRINTABLE_CHAR_COUNT][FONT_WIDTH_BIG * (FONT_HEIGHT_BIG / PAGE_HEIGHT)];
extern const uint8_t sh1106_font_characters_very_big[ASCII_PRINTABLE_CHAR_COUNT][FONT_WIDTH_VERY_BIG * (FONT_HEIGHT_VERY_BIG / PAGE_HEIGHT)];

/*==================================================================
    Function declarations
===================================================================*/

#endif /* __SH1106_CFG_H__ */