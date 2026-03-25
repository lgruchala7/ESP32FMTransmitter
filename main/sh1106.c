/*
 * File: sh1106.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-16
 */

/*==================================================================
    Includes
===================================================================*/

#include "sh1106.h"
#include "sh1106_cfg.h"

/*==================================================================
    Object-like macros
===================================================================*/

/*
 * Co="0" : The last control byte, only data bytes to follow,
 * Co="1" : Next two bytes are a data byte and another control byte
 */
#define CO_LAST_CTRL_BYTE 0U
#define CO_MORE_CTRL_BYTES 1U
/*
 * D/C="0" : The data byte is for command operation,
 * D/C="1" : The data byte is for RAM operation.
 */
#define DC_CMD_OPERATION 0U
#define DC_RAM_OPERATION 1U

/*==================================================================
    Function-like macros
===================================================================*/

/* Get bitmap data for a specific ASCII character */
#define GET_FONT_DATA_SMALL(c)                                                                     \
    (&sh1106_font_characters_small[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET][0])

#define GET_FONT_DATA_BIG_UPPER(c)                                                                 \
    (&sh1106_font_characters_big[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET][0])
#define GET_FONT_DATA_BIG_LOWER(c)                                                                 \
    (&sh1106_font_characters_big[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET][FONT_WIDTH_BIG])

#define GET_FONT_DATA_VERY_BIG_UPPER(c)                                                            \
    (&sh1106_font_characters_very_big[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET][0])
#define GET_FONT_DATA_VERY_BIG_MIDDLE(c)                                                           \
    (&sh1106_font_characters_very_big[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET]                  \
                                     [FONT_WIDTH_VERY_BIG])
#define GET_FONT_DATA_VERY_BIG_LOWER(c)                                                            \
    (&sh1106_font_characters_very_big[(uint8_t)(c) - ASCII_PRINTABLE_CHAR_OFFSET]                  \
                                     [FONT_WIDTH_VERY_BIG * 2])

#define PAGE_ADDRESS_RANGE_CHECK(font_height, address)                                             \
    ((PAGE_ADDRESS_MIN <= (address)) &&                                                            \
     (PAGE_ADDRESS_MAX - ((font_height) / PAGE_HEIGHT) + 1) >= (address))
#define COLUMN_ADDRESS_RANGE_CHECK(font_width, address)                                            \
    ((COLUMN_ADDRESS_MIN <= (address)) && ((COLUMN_ADDRESS_MAX - (font_width) + 1) >= (address)))

/*==================================================================
    Local types
===================================================================*/

/* Control byte internal structure. MSB -> Co bit*/
typedef union
{
    struct
    {
        uint8_t : 6;
        uint8_t dc : 1;
        uint8_t co : 1;
    };
    uint8_t value;
} sh1106_ctrl_byte_t;

typedef enum
{
    SET_COLUMN_ADDRESS_LOWER = 0x00,
    SET_COLUMN_ADDRESS_HIGHER = 0x10,
    SET_PUMP_VOLTAGE = 0x30,
    SET_DISPLAY_LINE_START = 0x40,
    SET_CONTRAST_CTRL = 0x81,
    SET_SEGMENT_REMAP = 0xA0,
    SET_ENTIRE_DISPLAY_OFF = 0xA4,
    SET_ENTIRE_DISPLAY_ON = 0xA5,
    SET_NORMAL_DISPLAY = 0xA6,
    SET_REVERSE_DISPLAY = 0xA7,
    SET_MULTIPLEX_RATIO = 0xA8,
    SET_DC_DC_OFF_ON = 0xAD,
    DISPLAY_OFF = 0xAE,
    DISPLAY_ON = 0xAF,
    SET_PAGE_ADDRESS = 0xB0,
    SET_COMMON_OUTPUT_SCAN_DIR = 0xC0,
    SET_DISPLAY_OFFSET = 0xD3,
    SET_CLOCK_DIV_OSC_FREQ = 0xD5,
    SET_DISCHARGE_PRECHARGE_PERIOD = 0xD9,
    SET_COMMON_PADS_HW_CONFIG = 0xDA,
    SET_VCOM_DESELECT_LVL = 0xDB,
    READ_MODIFY_WRITE = 0xE0,
    NOP = 0xE3,
    END = 0xEE,
} sh1106_cmd_t;

/*==================================================================
    Local objects
===================================================================*/

/*==================================================================
    Local function declarations
===================================================================*/

static inline void sh1106_send_cmd(i2c_master_dev_handle_t dev_handle, sh1106_ctrl_byte_t ctrl_byte,
                                   const uint8_t *data_bytes, uint8_t data_bytes_cnt);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Send command with data to SH1106.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] ctrl_byte SH1106 command control byte value.
 * @param[in] data_bytes SH1106 command data pointer.
 * @param[in] data_bytes_cnt SH1106 command data count.
 */
static inline void sh1106_send_cmd(i2c_master_dev_handle_t dev_handle, sh1106_ctrl_byte_t ctrl_byte,
                                   const uint8_t *data_bytes, uint8_t data_bytes_cnt)
{
    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, ctrl_byte.value, data_bytes, data_bytes_cnt));
}

/**
 * @brief Alternatively turns the display on and off.
 *
 * @note When the display OFF command is executed, power saver mode will be entered.
 * The internal status in the sleep mode is as follows:
 *   1) Stops the oscillator circuit and DC-DC circuit.
 *   2) Stops the OLED drive and outputs Hz as the segment/common driver output.
 *   3) Holds the display data and operation mode provided before the start of the sleep mode.
 *   4) The MPU can access to the built-in display RAM.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] state Display on if true, off otherwise.
 * @return
 *      - ESP_OK: Display OFF/ON command successful.
 */
esp_err_t sh1106_display_off_on(i2c_master_dev_handle_t dev_handle, bool state)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};

    uint8_t data = ((ON_STATE == state) ? DISPLAY_ON : DISPLAY_OFF);
    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Display %s", ((ON_STATE == state) ? "ON" : "OFF"));

    return ESP_OK;
}

/**
 * @brief Specifies current page address of display RAM.
 *
 * @note The display remains unchanged even when the page address is changed.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] address Page address from range <0-7>.
 * @return
 *      - ESP_OK: Set Page Address command successful.
 */
esp_err_t sh1106_set_page_address(i2c_master_dev_handle_t dev_handle, uint8_t address)
{
    if ((PAGE_ADDRESS_MIN > address) || (PAGE_ADDRESS_MAX < address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid page address: %u. Valid range: <%u-%u>.", address,
                 PAGE_ADDRESS_MIN, PAGE_ADDRESS_MAX);
    }

    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = SET_PAGE_ADDRESS | address;

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Page %u set", address);

    return ESP_OK;
}

/**
 * @brief Specifies current column address of display RAM.
 *
 * @note When the microprocessor repeats to access the display RAM, the column address counter is
 * incremented during each access until address 131 is accessed. The page address is not changed
 * during this time.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] address Column address from range <0-127>.
 * @return
 *      - ESP_OK: Set Column Address command successful.
 */
esp_err_t sh1106_set_column_address(i2c_master_dev_handle_t dev_handle, uint8_t address)
{
    if ((COLUMN_ADDRESS_MIN > address) || (COLUMN_ADDRESS_MAX < address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid column address: %u. Valid range: <%u-%u>.", address,
                 COLUMN_ADDRESS_MIN, COLUMN_ADDRESS_MAX);
    }

    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_COLUMN_ADDRESS_HIGHER | (address >> 4);
    data[1] = (SET_COLUMN_ADDRESS_LOWER | (address & 0xFU)) + COLUMN_SHIFT;

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Column %u set", address);

    return ESP_OK;
}

/**
 * @brief Write 8-bit data in display RAM.
 *
 * @note As the column address is incremental by 1 automatically after each write,
 * the microprocessor can continue to write data of multiple words.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] data Data to be sent to display RAM.
 * @param[in] data_cnt Data count in bytes. Up to DATA_BUFFER_MAX bytes allowed.
 * @return
 *      - ESP_OK: Write Display Data command successful.
 */
esp_err_t sh1106_write_display_data(i2c_master_dev_handle_t dev_handle, const uint8_t *data,
                                    uint8_t data_cnt)
{
    if (data_cnt > DATA_BUFFER_MAX)
    {
        ESP_LOGE(SH1106_TAG, "Too many data bytes: %u", data_cnt);
    }

    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_RAM_OPERATION};

    sh1106_send_cmd(dev_handle, ctrl_byte, data, data_cnt);

    ESP_LOGI(SH1106_TAG, "%u bytes written", data_cnt);

    return ESP_OK;
}

/**
 * @brief Change the relationship between RAM column address and segment driver.
 *
 * @note The order of segment driver output pads can be reversed by software.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] adc Reverse direction if true, normal direction otherwise.
 * @return
 *      - ESP_OK: Set Segment Remap command successful.
 */
esp_err_t sh1106_set_segment_remap(i2c_master_dev_handle_t dev_handle, bool adc)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = SET_SEGMENT_REMAP | ((ADC_REVERSE_DIR == adc) ? 1U : 0U);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "%s direction.", ((ADC_REVERSE_DIR == adc) ? "Reverse" : "Normal"));

    return ESP_OK;
}

/**
 * @brief Set the frequency of the internal display clocks (DCLKs).
 *
 * @note DCLK frequency is defined as the divide ratio (1 to 16) used to divide the oscillator
 * frequency. POR is 1. Frame frequency is determined by divide ratio, number of display clocks per
 * row, MUX ratio and oscillator frequency.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] div_ratio Divide ratio of display clocks (DCLKs).
 * @param[in] osc_freq Oscillator frequency.
 * @return
 *      - ESP_OK: Set Clock Divide Ratio/Oscillator Frequency command successful.
 */
esp_err_t sh1106_set_clock_div_ratio_osc_freq(i2c_master_dev_handle_t dev_handle, uint8_t div_ratio,
                                              uint8_t osc_freq)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_CLOCK_DIV_OSC_FREQ;
    data[1] = (osc_freq << 4) | ((div_ratio - 1U) & 0xFU);

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Divide ratio set to %u, oscillator frequency set to %u.", div_ratio,
             osc_freq);

    return ESP_OK;
}

/**
 * @brief Switches multiplex ratio.
 *
 * @note Multiplex ratio can be from range <1,64>.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] mux_ratio Multiplex ratio.
 * @return
 *      - ESP_OK: Set Multiplex Ratio command successful.
 */
esp_err_t sh1106_set_multiplex_ratio(i2c_master_dev_handle_t dev_handle, uint8_t mux_ratio)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_MULTIPLEX_RATIO;
    data[1] = (mux_ratio - 1U) & 0x3FU;

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Multiplex ratio set to %u.", mux_ratio);

    return ESP_OK;
}

/**
 * @brief Specifies the mapping of display start line to one of COM0-63.
 *
 * @note For example, to move the COM16 towards the COMO direction for 16 lines, the 6-bit data in
 * the second byte should be given by 010000. To move in the opposite direction by 16 lines, the
 * 6-bit data should be given by (64-16), so the second byte should be 100000.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] offset COMx offset.
 * @return
 *      - ESP_OK: Set Display Offset command successful.
 */
esp_err_t sh1106_set_display_offset(i2c_master_dev_handle_t dev_handle, uint8_t offset)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_DISPLAY_OFFSET;
    data[1] = offset & 0x3FU;

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Display offset set to %u.", offset);

    return ESP_OK;
}

/**
 * @brief Specifies the initial display line or COMO.
 *
 * @note The RAM display data becomes the top line of OLED screen. It is followed by the higher
 * number of lines in ascending order, corresponding to the duty cycle. When this command changes
 * the line address, the smooth scrolling or page change takes place.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] start_line Line number.
 * @return
 *      - ESP_OK: Set Display Start Line command successful.
 */
esp_err_t sh1106_set_display_start_line(i2c_master_dev_handle_t dev_handle, uint8_t start_line)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = SET_DISPLAY_LINE_START | (start_line & 0x3F);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Display start line set to %u.", start_line);

    return ESP_OK;
}

/**
 * @brief Controls the DC-DC voltage converter.
 *
 * @note The converter will be turned on by issuing this command then display ON command.
 * The panel display must be off while issuing this command.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] state DC-DC enabled if true, disabled otherwise.
 * @return
 *      - ESP_OK: Set DC-DC OFF/ON command successful.
 */
esp_err_t sh1106_set_dc_dc_off_on(i2c_master_dev_handle_t dev_handle, bool state)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_DC_DC_OFF_ON;
    data[1] = 0x8AU | ((ON_STATE == state) ? 1U : 0U);

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "DC-DC %s.", ((ON_STATE == state) ? "enabled" : "disabled"));

    return ESP_OK;
}

/**
 * @brief Sets the scan direction of the common output.
 *
 * @note The display will have immediate effect once this command is issued. That is,
 * if this command is sent during normal display, the graphic display will be vertically flipped.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] direction Ascending (COM0, COM1, ...) or descending scan direction.
 * @return
 *      - ESP_OK: Set Common Output Scan Direction command successful.
 */
esp_err_t sh1106_set_common_output_scan_dir(i2c_master_dev_handle_t dev_handle, uint8_t direction)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = SET_COMMON_OUTPUT_SCAN_DIR | (direction & 0x8U);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Common output scan direction set to %s.",
             ((SCAN_DIR_DESCENDING == direction) ? "descending" : "ascending"));

    return ESP_OK;
}

/**
 * @brief Sets the common signals pad configuration.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] mode Sequential or alternative mode.
 * @return
 *      - ESP_OK: Set Common Pads Hardware Configuration command successful.
 */
esp_err_t sh1106_set_common_pads_hw_config(i2c_master_dev_handle_t dev_handle, uint8_t mode)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_COMMON_PADS_HW_CONFIG;
    data[1] = 0x02U | (mode & HW_CONFIG_MODE_ALTERNATIVE);

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Signals pad configuration %s.",
             ((HW_CONFIG_MODE_ALTERNATIVE == mode) ? "alternative" : "sequential"));

    return ESP_OK;
}

/**
 * @brief Sets contrast setting of the display.
 *
 * @note The chip has 256 contrast steps from 0 to 255.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] contrast_val Contrast regiuster value.
 * @return
 *      - ESP_OK: Set Contrast Control Register command successful.
 */
esp_err_t sh1106_set_contrast_ctrl_register(i2c_master_dev_handle_t dev_handle,
                                            uint8_t contrast_val)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_CONTRAST_CTRL;
    data[1] = contrast_val;

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Contrast control register set to %u.", contrast_val);

    return ESP_OK;
}

/**
 * @brief Sets the duration of discharge/precharge period.
 *
 * @note The interval is counted in number of DCLK and has to be greater than 0.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] period_discharge Discharge period in DCLKs.
 * @param[in] period_precharge Precharge period in DCLKs.
 * @return
 *      - ESP_OK: Set Discharge/Precharge Period command successful.
 */
esp_err_t sh1106_set_discharge_precharge_period(i2c_master_dev_handle_t dev_handle,
                                                uint8_t period_discharge, uint8_t period_precharge)
{
    if ((0U == period_discharge) || (0U == period_precharge))
    {
        ESP_LOGE(SH1106_TAG, "Invalid period value: %u (discharge) or %u (precharge)",
                 period_discharge, period_precharge);
    }

    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_DISCHARGE_PRECHARGE_PERIOD;
    data[1] = (period_discharge << 4) | (period_precharge & 0xF);

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Discharge period set to %u DCLKs, precharge period set to %u DCLKs.",
             period_discharge, period_precharge);

    return ESP_OK;
}

/**
 * @brief Sets the common output pad voltage at deselect stage.
 *
 * @note V_com = beta * V_ref
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] beta V_ref voltage percentage. Valid range is <43,83>,<100>
 * @return
 *      - ESP_OK: Set V_com Deselect Level command successful.
 */
esp_err_t sh1106_set_vcom_deselect_lvl(i2c_master_dev_handle_t dev_handle, uint8_t beta)
{
    if ((43U > beta) || ((83U < beta) && (100U > beta)) || (100U < beta))
    {
        ESP_LOGE(SH1106_TAG, "Invalid beta value: %u. Valid range is <43,83>,<100>", beta);
    }

    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data[2];
    data[0] = SET_VCOM_DESELECT_LVL;
    data[1] = (uint8_t)(((uint32_t)(beta - 43U) * 1559U) / 1000U);

    sh1106_send_cmd(dev_handle, ctrl_byte, data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "V_com deselect level set to %u% of V_ref.", beta);

    return ESP_OK;
}

/**
 * @brief Specifies output voltage (V_pp) of the internal charge pump.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] voltage Output voltage. Valid range is <0,3>.
 * @return
 *      - ESP_OK: Set Pump Voltage command successful.
 */
esp_err_t sh1106_set_pump_voltage(i2c_master_dev_handle_t dev_handle, uint8_t voltage)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = SET_PUMP_VOLTAGE | (voltage & 0x3U);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Pump output voltage set to %u.", voltage);

    return ESP_OK;
}

/**
 * @brief Reverses the display ON/OFF status without rewriting RAM data.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] reverse OLED ON when RAM data low if true, when RAM data high otherwise.
 * @return
 *      - ESP_OK: Set Normal/Reverse Display command successful.
 */
esp_err_t sh1106_set_normal_reverse_display(i2c_master_dev_handle_t dev_handle, bool reverse)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = ((DISPLAY_DATA_REVERSED == reverse) ? SET_REVERSE_DISPLAY : SET_NORMAL_DISPLAY);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Display set to %s.",
             ((DISPLAY_DATA_REVERSED == reverse) ? "reverse" : "normal"));

    return ESP_OK;
}

/**
 * @brief Turns the entire display ON regardless of the display RAM content.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] state Entire display ON if true, normal display operation otherwise.
 * @return
 *      - ESP_OK: Set Entire Display OFF/ON command successful.
 */
esp_err_t sh1106_set_entire_display_off_on(i2c_master_dev_handle_t dev_handle, bool state)
{
    sh1106_ctrl_byte_t ctrl_byte = {.co = CO_LAST_CTRL_BYTE, .dc = DC_CMD_OPERATION};
    uint8_t data = ((true == state) ? SET_ENTIRE_DISPLAY_ON : SET_ENTIRE_DISPLAY_OFF);

    sh1106_send_cmd(dev_handle, ctrl_byte, &data, sizeof(data));

    ESP_LOGI(SH1106_TAG, "Entire display set %s.", ((true == state) ? "on" : "off"));

    return ESP_OK;
}

/**
 * @brief Writes a small font character to the display.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] c Character to b e written.
 * @param[in] page_address Write operation page address.
 * @param[in,out] p_column_address Write operation column address pointer. Updated after succesful
 * write.
 * @return
 *      - ESP_OK: Write Small Character command successful.
 *      - ESP_FAIL: Write Small Character command failed.
 */
esp_err_t sh1106_write_character_small(i2c_master_dev_handle_t dev_handle, char c,
                                       uint8_t page_address, uint8_t *p_column_address)
{
    esp_err_t ret_val = ESP_FAIL;

    if (!PAGE_ADDRESS_RANGE_CHECK(FONT_HEIGHT_SMALL, page_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid page address: %u. Valid range: <%u-%u>.", page_address,
                 PAGE_ADDRESS_MIN, PAGE_ADDRESS_MAX);
    }

    else if (!COLUMN_ADDRESS_RANGE_CHECK(FONT_WIDTH_SMALL, *p_column_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid column address: %u. Valid range: <%u-%u>.", *p_column_address,
                 COLUMN_ADDRESS_MIN, (COLUMN_ADDRESS_MAX - FONT_WIDTH_SMALL));
    }
    else
    {
        const uint8_t *data = GET_FONT_DATA_SMALL(c);
        sh1106_set_page_address(dev_handle, page_address);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_SMALL);
        *p_column_address += FONT_WIDTH_SMALL;

        ret_val = ESP_OK;
    }

    return ret_val;
}

/**
 * @brief Writes a big font character to the display.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] c Character to b e written.
 * @param[in] page_address Write operation page address.
 * @param[in,out] p_column_address Write operation column address pointer. Updated after succesful
 * write.
 * @return
 *      - ESP_OK: Write Big Character command successful.
 *      - ESP_FAIL: Write Big Character command failed.
 */
esp_err_t sh1106_write_character_big(i2c_master_dev_handle_t dev_handle, char c,
                                     uint8_t page_address, uint8_t *p_column_address)
{
    esp_err_t ret_val = ESP_FAIL;

    if (!PAGE_ADDRESS_RANGE_CHECK(FONT_HEIGHT_BIG, page_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid page address: %u. Valid range: <%u-%u>.", page_address,
                 PAGE_ADDRESS_MIN, (PAGE_ADDRESS_MAX - (FONT_HEIGHT_BIG / PAGE_HEIGHT) + 1));
    }
    else if (!COLUMN_ADDRESS_RANGE_CHECK(FONT_WIDTH_BIG, *p_column_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid column address: %u. Valid range: <%u-%u>.", *p_column_address,
                 COLUMN_ADDRESS_MIN, (COLUMN_ADDRESS_MAX - FONT_WIDTH_BIG + 1));
    }
    else
    {
        const uint8_t *data = GET_FONT_DATA_BIG_UPPER(c);
        sh1106_set_page_address(dev_handle, page_address);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_BIG);

        data = GET_FONT_DATA_BIG_LOWER(c);
        sh1106_set_page_address(dev_handle, page_address + 1U);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_BIG);

        sh1106_set_page_address(dev_handle, page_address);
        *p_column_address += FONT_WIDTH_BIG;

        ret_val = ESP_OK;
    }

    return ret_val;
}

/**
 * @brief Writes a very big font character to the display.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] c Character to b e written.
 * @param[in] page_address Write operation page address.
 * @param[in,out] p_column_address Write operation column address pointer. Updated after succesful
 * write.
 * @return
 *      - ESP_OK: Write Very Big Character command successful.
 *      - ESP_FAIL: Write Very Big Character command failed.
 */
esp_err_t sh1106_write_character_very_big(i2c_master_dev_handle_t dev_handle, char c,
                                          uint8_t page_address, uint8_t *p_column_address)
{
    esp_err_t ret_val = ESP_FAIL;

    if (!PAGE_ADDRESS_RANGE_CHECK(FONT_HEIGHT_VERY_BIG, page_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid page address: %u. Valid range: <%u-%u>.", page_address,
                 PAGE_ADDRESS_MIN, (PAGE_ADDRESS_MAX - (FONT_HEIGHT_VERY_BIG / PAGE_HEIGHT) + 1));
    }
    else if (!COLUMN_ADDRESS_RANGE_CHECK(FONT_WIDTH_VERY_BIG, *p_column_address))
    {
        ESP_LOGE(SH1106_TAG, "Invalid column address: %u. Valid range: <%u-%u>.", *p_column_address,
                 COLUMN_ADDRESS_MIN, (COLUMN_ADDRESS_MAX - FONT_WIDTH_VERY_BIG + 1));
    }
    else
    {
        const uint8_t *data = GET_FONT_DATA_VERY_BIG_UPPER(c);
        sh1106_set_page_address(dev_handle, page_address);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_VERY_BIG);

        data = GET_FONT_DATA_VERY_BIG_MIDDLE(c);
        sh1106_set_page_address(dev_handle, page_address + 1U);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_VERY_BIG);

        data = GET_FONT_DATA_VERY_BIG_LOWER(c);
        sh1106_set_page_address(dev_handle, page_address + 2U);
        sh1106_set_column_address(dev_handle, *p_column_address);
        sh1106_write_display_data(dev_handle, data, FONT_WIDTH_VERY_BIG);

        sh1106_set_page_address(dev_handle, page_address);
        *p_column_address += FONT_WIDTH_VERY_BIG;

        ret_val = ESP_OK;
    }

    return ret_val;
}

/**
 * @brief Clears the whole display page.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] address Page address.
 * @return
 *      - ESP_OK: Clear Page command successful.
 */
esp_err_t sh1106_clear_page(i2c_master_dev_handle_t dev_handle, uint8_t address)
{
    const uint8_t data_buff[DATA_BUFFER_MAX] = {0x00U};

    sh1106_set_page_address(dev_handle, address);
    sh1106_set_column_address(dev_handle, COLUMN_ADDRESS_MIN);

    for (int col = COLUMN_ADDRESS_MIN; col <= COLUMN_ADDRESS_MAX; col += DATA_BUFFER_MAX)
    {
        if ((COLUMN_ADDRESS_MAX - col + 1U) < sizeof(data_buff))
        {
            sh1106_write_display_data(dev_handle, data_buff, (COLUMN_ADDRESS_MAX - col + 1U));
        }
        else
        {
            sh1106_write_display_data(dev_handle, data_buff, sizeof(data_buff));
        }
    }

    return ESP_OK;
}