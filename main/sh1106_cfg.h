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

/* Font dimensions in pixels */
#define FONT_WIDTH_SMALL 5U
#define FONT_HEIGHT_SMALL 8U
#define FONT_WIDTH_BIG 8U
#define FONT_HEIGHT_BIG 16U
#define FONT_WIDTH_BOLD_BIG 10U
#define FONT_HEIGHT_BOLD_BIG 16U

/* Display layout */
#define HEADER_PAGE_ADDRESS 0U
#define TX_FREQ_PAGE_ADDRESS (HEADER_PAGE_ADDRESS + 2U)
#define PLAYBACK_STATE_PAGE_ADDRESS (TX_FREQ_PAGE_ADDRESS + 2U)

/*==================================================================
    Function-like macros
===================================================================*/

/*==================================================================
    Exported types
===================================================================*/

/*==================================================================
    Exported objects
===================================================================*/

extern const uint8_t sh1106_font_characters_small[95][FONT_WIDTH_SMALL];
extern const uint8_t sh1106_font_characters_big[95][FONT_WIDTH_BIG * 2];
extern const uint8_t sh1106_font_characters_bold_big[95][FONT_WIDTH_BOLD_BIG * 2];

/*==================================================================
    Function declarations
===================================================================*/

#endif /* __SH1106_CFG_H__ */