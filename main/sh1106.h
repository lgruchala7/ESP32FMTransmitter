/*
 * File: sh1106.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-16
 */

#ifndef __SH1106_H__
#define __SH1106_H__

/*==================================================================
    Includes
===================================================================*/

#include <inttypes.h>
#include "sh1106_cfg.h"
#include "i2c_master_app.h"
#include "esp_system.h"

/*==================================================================
    Object-like macros
===================================================================*/

/* Display dimensions */
#define DISPLAY_WIDTH 128U
#define DISPLAY_HEIGHT 64U

/* Display address ranges */
#define PAGE_ADDRESS_MIN 0U
#define PAGE_ADDRESS_MAX 7U
#define COLUMN_ADDRESS_MIN 0U
#define COLUMN_ADDRESS_MAX (DISPLAY_WIDTH - 1U)

/* Oscillator frequency */
#define OSC_FREQ_MIN_25 0x0U
#define OSC_FREQ_MIN_20 0x1U
#define OSC_FREQ_MIN_15 0x2U
#define OSC_FREQ_MIN_10 0x3U
#define OSC_FREQ_MIN_5 0x4U
#define OSC_FREQ_DEFAULT 0x5U
#define OSC_FREQ_PLUS_5 0x6U
#define OSC_FREQ_PLUS_10 0x7U
#define OSC_FREQ_PLUS_15 0x8U
#define OSC_FREQ_PLUS_20 0x9U
#define OSC_FREQ_PLUS_25 0xAU
#define OSC_FREQ_PLUS_30 0xBU
#define OSC_FREQ_PLUS_35 0xCU
#define OSC_FREQ_PLUS_40 0xDU
#define OSC_FREQ_PLUS_45 0xEU
#define OSC_FREQ_PLUS_50 0xFU

/* Display state */
#define OFF_STATE false
#define ON_STATE true

/* Segment remap */
#define ADC_NORMAL_DIR false
#define ADC_REVERSE_DIR true

/* Common output scan direction */
#define SCAN_DIR_ASCENDING 0x0U
#define SCAN_DIR_DESCENDING 0x8U

/* Common pads hw config mode */
#define HW_CONFIG_MODE_SEQUENTIAL 0x00U
#define HW_CONFIG_MODE_ALTERNATIVE 0x10U

/* Display contrast */
#define DISPLAY_CONTRAST_VERY_LOW 0x00U
#define DISPLAY_CONTRAST_LOW 0x40U
#define DISPLAY_CONTRAST_NORMAL 0x80U
#define DISPLAY_CONTRAST_HIGH 0xC0U
#define DISPLAY_CONTRAST_VERY_HIGH 0xFFU

/* Pump output voltage */
#define VPP_6V4 0x0U
#define VPP_7V4 0x1U
#define VPP_8V 0x2U
#define VPP_9V 0x3U

/* Display data reverse state */
#define DISPLAY_DATA_NORMAL false
#define DISPLAY_DATA_REVERSED true

/*==================================================================
    Function-like macros
===================================================================*/

/* Display clock frequency divider ratio*/
#define DIV_RATIO(x) (x)
/* Multiplex ratio */
#define MUX_RATIO(x) (x)
/* Display offset */
#define DISPLAY_OFFSET(x) (x)
/* Display start line */
#define DISPLAY_START_LINE(x) (x)
/* Discharge period in DCLKs */
#define DISCHARGE_PERIOD_DCLK(x) (x)
/* Discharge period in DCLKs */
#define PRECHARGE_PERIOD_DCLK(x) (x)

/*==================================================================
    Exported types
===================================================================*/

/*==================================================================
    Exported objects
===================================================================*/

/*==================================================================
    Function declarations
===================================================================*/

esp_err_t sh1106_display_off_on(i2c_master_dev_handle_t dev_handle, bool state);
esp_err_t sh1106_set_page_address(i2c_master_dev_handle_t dev_handle, uint8_t address);
esp_err_t sh1106_set_column_address(i2c_master_dev_handle_t dev_handle, uint8_t address);
esp_err_t sh1106_write_display_data(i2c_master_dev_handle_t dev_handle, const uint8_t *data, uint8_t data_cnt);
esp_err_t sh1106_set_segment_remap(i2c_master_dev_handle_t dev_handle, bool adc);
esp_err_t sh1106_set_clock_div_ratio_osc_freq(i2c_master_dev_handle_t dev_handle, uint8_t div_ratio, uint8_t osc_freq);
esp_err_t sh1106_set_multiplex_ratio(i2c_master_dev_handle_t dev_handle, uint8_t mux_ratio);
esp_err_t sh1106_set_display_offset(i2c_master_dev_handle_t dev_handle, uint8_t offset);
esp_err_t sh1106_set_display_start_line(i2c_master_dev_handle_t dev_handle, uint8_t start_line);
esp_err_t sh1106_set_dc_dc_off_on(i2c_master_dev_handle_t dev_handle, bool state);
esp_err_t sh1106_set_common_output_scan_dir(i2c_master_dev_handle_t dev_handle, uint8_t direction);
esp_err_t sh1106_set_common_pads_hw_config(i2c_master_dev_handle_t dev_handle, uint8_t mode);
esp_err_t sh1106_set_contrast_ctrl_register(i2c_master_dev_handle_t dev_handle, uint8_t contrast_val);
esp_err_t sh1106_set_discharge_precharge_period(i2c_master_dev_handle_t dev_handle, uint8_t period_discharge, uint8_t period_precharge);
esp_err_t sh1106_set_vcom_deselect_lvl(i2c_master_dev_handle_t dev_handle, uint8_t level);
esp_err_t sh1106_set_pump_voltage(i2c_master_dev_handle_t dev_handle, uint8_t voltage);
esp_err_t sh1106_set_normal_reverse_display(i2c_master_dev_handle_t dev_handle, bool reverse);
esp_err_t sh1106_set_entire_display_off_on(i2c_master_dev_handle_t dev_handle, bool state);

#endif /* __SH1106_H__ */