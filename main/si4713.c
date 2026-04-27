/*
 * File: si4713.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

/*==================================================================
    Includes
===================================================================*/

#include "si4713.h"
#include "freertos/FreeRTOS.h"
#include "si4713_cfg.h"

/*==================================================================
    Object-like macros
===================================================================*/

/*==================================================================
    Function-like macros
===================================================================*/

#define WAIT_MS(time)                                                                              \
    do                                                                                             \
    {                                                                                              \
        TickType_t start = xTaskGetTickCount();                                                    \
        TickType_t timeout = pdMS_TO_TICKS((time));                                                \
        while (xTaskGetTickCount() < (start + timeout))                                            \
        {                                                                                          \
        }                                                                                          \
    } while (0)

/*==================================================================
    Local types
===================================================================*/

/* FM transmitter command summary */
typedef enum
{
    POWER_UP = 0x01,        /* Power up device and mode selection. Modes include FM transmit and
                               analog/digital audio interface configuration. */
    GET_REV = 0x10,         /* Returns revision information on the device. */
    POWER_DOWN = 0x11,      /* Power down device. */
    SET_PROPERTY = 0x12,    /* Sets the value of a property. */
    GET_PROPERTY = 0x13,    /* Retrieves a property’s value. */
    GET_INT_STATUS = 0x14,  /* Read interrupt status bits. */
    PATCH_ARGS = 0x15,      /* Reserved command used for patch file downloads. */
    PATCH_DATA = 0x16,      /* Reserved command used for patch file downloads. */
    TX_TUNE_FREQ = 0x30,    /* Tunes to given transmit frequency. */
    TX_TUNE_POWER = 0x31,   /* Sets the output power level and tunes the antenna capacitor */
    TX_TUNE_MEASURE = 0x32, /* Measure the received noise level at the specified frequency. */
    TX_TUNE_STATUS = 0x33,  /* Queries the status of a previously sent TX Tune Freq, TX Tune Power,
                               or TX Tune Measure command. */
    TX_ASQ_STATUS = 0x34,   /* Queries the TX status and input audio signal metrics. */
    TX_RDS_BUFF = 0x35,     /* Si4713 Only. Queries the status of the RDS Group Buffer and loads new
                               data into buffer. */
    TX_RDS_PS = 0x36,       /* Si4713 Only. Set up default PS strings. */
    GPO_CTL = 0x80,         /* Configures GPO1, 2, and 3 as output or Hi-Z. */
    GPO_SET = 0x81,         /* Sets GPO1, 2, and 3 output level (low or high). */
} si4713_command_t;

/* Structure of Si4713 status response */
typedef union
{
    struct
    {
        uint8_t stcint : 1; /* Seek/Tune Complete Interrupt. 0 = Tune complete has not been
                               triggered. 1 = Tune complete has been triggered. */
        uint8_t asqint : 1; /* Signal Quality Interrupt. 0 = Signal quality measurement has not been
                               triggered. 1 = Signal quality measurement has been triggered. */
        uint8_t rdsint : 1; /* RDS Interrupt. 0 = RDS interrupt has not been triggered. 1 = RDS
                               interrupt has been triggered. */
        uint8_t : 3;        /* Reserved */
        uint8_t err : 1;    /* Error. 0 = No error, 1 = Error */
        uint8_t cts : 1; /* Clear to Send. 0 = Wait before sending next command. 1 = Clear to send
                            next command */
    };
    uint8_t val;
} si4713_status_response_t;

/*==================================================================
    Local objects
===================================================================*/

/*==================================================================
    Local function declarations
===================================================================*/

static void si4713_read_status(i2c_master_dev_handle_t dev_handle,
                               si4713_status_response_t *status_ptr,
                               si4713_status_response_t status_expected, uint32_t timeout_ms);
static inline void si4713_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *resp_buff,
                                        uint8_t resp_len);
static inline void si4713_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd,
                                   const uint8_t *args, uint8_t arg_cnt);
static esp_err_t si4713_tx_rds_ps(i2c_master_dev_handle_t dev_handle, const uint8_t *args);
static esp_err_t si4713_tx_rds_buff(i2c_master_dev_handle_t dev_handle, const uint8_t *data);
static inline uint16_t date_to_mjd(int y, int m, int d);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Converts date to MJD (Modified Julian Day) format
 *
 * @note The Modified Julian Day (MJD) is a simplified, continuous day count used in astronomy and
 * geodesy to track time independent of the calendar, starting at midnight on November 17, 1858.
 *
 * @param[in] y Year.
 * @param[in] m Month.
 * @param[in] d Day of month.
 * @return Date in MJD format.
 */
static inline uint16_t date_to_mjd(int y, int m, int d)
{
    if (m <= 2)
    {
        y--;
        m += 12;
    }

    long mjd = (365 * y) + (y / 4) - (y / 100) + (y / 400) + ((153 * (m - 3) + 2) / 5) + d - 678882;

    return (uint16_t)mjd;
}

/**
 * @brief Executes TX_RDS_PS command.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] args TX_RDS_PS command arguments.
 * @return
 *      - ESP_OK: TX_RDS_PS command successful.
 *      - ESP_FAIL: TX_RDS_PS command failed.
 */
static esp_err_t si4713_tx_rds_ps(i2c_master_dev_handle_t dev_handle, const uint8_t *args)
{
    esp_err_t ret_val = ESP_FAIL;

    si4713_send_cmd(dev_handle, TX_RDS_PS, &args[0], TX_RDS_PS_ARG_CNT);

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_RDS_PS status = 0x%X", status.val);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_RDS_PS status = 0x%X (expected: 0x%X)", status.val,
                 status_expected.val);
    }

    return ret_val;
}

/**
 * @brief Executes TX_RDS_BUFF command.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] args TX_RDS_BUFF command arguments.
 * @return
 *      - ESP_OK: TX_RDS_BUFF command successful.
 *      - ESP_FAIL: TX_RDS_BUFF command failed.
 */
static esp_err_t si4713_tx_rds_buff(i2c_master_dev_handle_t dev_handle, const uint8_t *args)
{
    esp_err_t ret_val = ESP_FAIL;

    si4713_send_cmd(dev_handle, TX_RDS_BUFF, &args[0], TX_RDS_BUFF_ARG_CNT);

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_RDS_BUFF status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_RDS_BUFF status = 0x%X (expected: 0x%X)", status,
                 status_expected.val);
    }

    if (ESP_OK == ret_val)
    {
        uint8_t response[TX_RDS_BUFF_RESP_LEN];
        si4713_read_response(dev_handle, response, sizeof(response));

        if (0U != (response[TX_RDS_BUFF_RESP_FIFOMT_BYTE] & (1U << TX_RDS_BUFF_RESP_FIFOMT_POS)))
        {
            ESP_LOGI(SI4713_TAG, "\tRDS Group FIFO Buffer is empty");
        }
        else if (0U !=
                 (response[TX_RDS_BUFF_RESP_CBUFWRAP_BYTE] & (1U << TX_RDS_BUFF_RESP_CBUFWRAP_POS)))
        {
            ESP_LOGI(SI4713_TAG, "\tRDS Group Circular Buffer has wrapped");
        }
        else if (0U !=
                 (response[TX_RDS_BUFF_RESP_FIFOXMIT_BYTE] & (1U << TX_RDS_BUFF_RESP_FIFOXMIT_POS)))
        {
            ESP_LOGI(SI4713_TAG, "\tRDS Group has been transmitted from the circular buffer");
        }
        else if (0U !=
                 (response[TX_RDS_BUFF_RESP_CBUFXMIT_BYTE] & (1U << TX_RDS_BUFF_RESP_CBUFXMIT_POS)))
        {
            ESP_LOGI(SI4713_TAG, "\tRDS Group has been transmitted from the FIFO buffer");
        }
        else if (0U != (response[TX_RDS_BUFF_RESP_RDSPSXMIT_BYTE] &
                        (1U << TX_RDS_BUFF_RESP_RDSPSXMIT_POS)))
        {
            ESP_LOGI(SI4713_TAG, "\tRDS PS Group has been transmitted");
        }
        ESP_LOGI(SI4713_TAG, "\tAvailable circular buffer blocks = %d",
                 response[TX_RDS_BUFF_RESP_CBAVAIL_BYTE]);
        ESP_LOGI(SI4713_TAG, "\tUsed circular buffer blocks = %d",
                 response[TX_RDS_BUFF_RESP_CBUSED_BYTE]);
        ESP_LOGI(SI4713_TAG, "\tAvailable FIFO blocks = %d",
                 response[TX_RDS_BUFF_RESP_FIFOAVAIL_BYTE]);
        ESP_LOGI(SI4713_TAG, "\tUsed FIFO blocks = %d", response[TX_RDS_BUFF_RESP_FIFOUSED_BYTE]);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "CTS timeout expired");
    }

    return ret_val;
}

/**
 * @brief Read status value from Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[out] status_ptr Status variable pointer.
 * @param[in] timeout_ms Timeout for CTS bit setting in ms.
 */
static void si4713_read_status(i2c_master_dev_handle_t dev_handle,
                               si4713_status_response_t *status_ptr,
                               si4713_status_response_t status_expected, uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    do
    {
        ESP_ERROR_CHECK(i2c_read_response(dev_handle, (uint8_t *)status_ptr, 1));
    } while ((status_expected.val != status_ptr->val) && ((xTaskGetTickCount() - start) < timeout));
}

/**
 * @brief Read multi-byte response from Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[out] resp_buff Response buffer pointer.
 * @param[in] resp_len Response buffer length.
 */
static inline void si4713_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *resp_buff,
                                        uint8_t resp_len)
{
    ESP_ERROR_CHECK(i2c_read_response(dev_handle, resp_buff, resp_len));
}

/**
 * @brief Send command with args to Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] cmd Si4713 command code.
 * @param[in] args Si4713 command arguments.
 * @param[in] arg_cnt Si4713 command argument count.
 */
static inline void si4713_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd,
                                   const uint8_t *args, uint8_t arg_cnt)
{
    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, cmd, args, arg_cnt));
}

/**
 * @brief Perform Si4713 powerup
 *
 * This function sends Powerup in Analog Mode command to Si4713 and checks the response status.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] val Powerup argument values.
 * @return
 *      - ESP_OK: Powerup command successful.
 *      - ESP_FAIL: Powerup command failed.
 */
esp_err_t si4713_powerup_analog(i2c_master_dev_handle_t dev_handle, uint16_t val)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t args[2];
    args[0] = (val >> 8U) & 0xFFU;
    args[1] = val & 0xFFU;
    si4713_send_cmd(dev_handle, POWER_UP, &args[0], sizeof(args));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_LONG_MS);

    if ((0U == status.err) && (1U == status.cts))
    {
        ret_val = ESP_OK;
    }
    ESP_LOGI(SI4713_TAG, "POWER_UP status = 0x%X", status);

    return ret_val;
}

/**
 * @brief Set property of Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] property Si4713 property.
 * @param[in] val Si4713 property value.
 * @return
 *      - ESP_OK: Set Property command successful.
 *      - ESP_FAIL: Set Property command failed.
 */
esp_err_t si4713_set_property(i2c_master_dev_handle_t dev_handle, si4713_property_t property,
                              uint16_t val)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t args[5];
    args[0] = 0x00U;
    args[1] = ((uint16_t)property >> 8U) & 0xFFU;
    args[2] = (uint16_t)property & 0xFFU;
    args[3] = (val >> 8U) & 0xFFU;
    args[4] = val & 0xFFU;
    si4713_send_cmd(dev_handle, SET_PROPERTY, &args[0], sizeof(args));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "SET_PROPERTY 0x%X status = 0x%X", property, status.val);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "SET_PROPERTY 0x%X status = 0x%X (expected: 0x%X)", property,
                 status.val, status_expected.val);
    }

    return ret_val;
}

/**
 * @brief Get Si4713 info.
 *
 * Returns the part number, chip revision, firmware revision, patch revision and component revision
 * numbers.
 *
 * @param[in] dev_handle I2C master device handle.
 * @return
 *      - ESP_OK: Get revision command successful.
 *      - ESP_FAIL: Get revision command failed.
 */
esp_err_t si4713_get_rev(i2c_master_dev_handle_t dev_handle)
{
    esp_err_t ret_val = ESP_FAIL;
    uint8_t arg = 0x00U;

    si4713_send_cmd(dev_handle, GET_REV, &arg, sizeof(arg));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "GET_REV status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "GET_REV status = 0x%X (expected: 0x%X)", status, status_expected.val);
    }

    if (ESP_OK == ret_val)
    {
        uint8_t response[9];
        si4713_read_response(dev_handle, response, sizeof(response));

        ESP_LOGI(SI4713_TAG, "\tpart number = 0x%X", response[1]); // (0x0D = Si4713)
        ESP_LOGI(SI4713_TAG, "\tfirmware revision = v%.2d.%.2d", (response[2] - 0x30U),
                 (response[3] - 0x30U)); // in ASCII 0x30 = 0
        ESP_LOGI(SI4713_TAG, "\tpatch id = 0x%X",
                 ((response[4] << 8U) & 0xFF00U) | (response[5] & 0xFFU));
        ESP_LOGI(SI4713_TAG, "\tcomponent firmware revision = v%.2d.%.2d", (response[6] - 0x30U),
                 (response[7] - 0x30U));
        ESP_LOGI(SI4713_TAG, "\tchip revision = rev%c", response[8]);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "CTS timeout expired");
    }

    return ret_val;
}

/**
 * @brief Set TX tune power of Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] val Tune power byte (MSB), antenna tuning capacitor (LSB).
 * @return
 *      - ESP_OK: TX Tune Power command successful.
 *      - ESP_FAIL: TX Tune Power command failed.
 */
esp_err_t si4713_tx_tune_power(i2c_master_dev_handle_t dev_handle, uint16_t val)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t args[4];
    args[0] = 0x00U;
    args[1] = 0x00U;
    args[2] = (val >> 8U) & 0xFFU;
    args[3] = val & 0xFFU;
    si4713_send_cmd(dev_handle, TX_TUNE_POWER, &args[0], sizeof(args));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_TUNE_POWER status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_TUNE_POWER status = 0x%X (expected: 0x%X)", status,
                 status_expected.val);
    }

    return ret_val;
}

/**
 * @brief Set TX tune frequency of Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] val Tune frequency in 10 kHz units.
 * @return
 *      - ESP_OK: TX Tune Frequency command successful.
 *      - ESP_FAIL: TX Tune Frequency command failed.
 */
esp_err_t si4713_tx_tune_freq(i2c_master_dev_handle_t dev_handle, uint16_t val)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t args[3];
    args[0] = 0x00U;
    args[1] = (val >> 8U) & 0xFFU;
    args[2] = val & 0xFFU;
    si4713_send_cmd(dev_handle, TX_TUNE_FREQ, &args[0], sizeof(args));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_TUNE_FREQUENCY status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_TUNE_FREQUENCY status = 0x%X (expected: 0x%X)", status,
                 status_expected.val);
    }

    return ret_val;
}

/**
 * @brief Update status byte of Si4713.
 *
 * @note This command should be called after any command that sets the STCINT, ASQINT, or RDSINT
 * bits.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] status_expected Expected reply status value.
 * @param[in] timeout Correct response status timeout.
 * @return
 *      - ESP_OK: Get Int Status command successful.
 *      - ESP_FAIL: Get Int Status command failed.
 */
esp_err_t si4713_get_int_status(i2c_master_dev_handle_t dev_handle, uint8_t status_expected_val,
                                uint32_t timeout_ms)
{
    esp_err_t ret_val = ESP_FAIL;
    si4713_status_response_t status;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

    do
    {
        si4713_send_cmd(dev_handle, GET_INT_STATUS, NULL, 0U);
        si4713_read_status(dev_handle, &status, (si4713_status_response_t)status_expected_val, 0U);
    } while ((status_expected_val != status.val) && ((xTaskGetTickCount() - start) < timeout));

    if (status.val == status_expected_val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "GET_INT_STATUS status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "GET_INT_STATUS status = 0x%X (expected: 0x%X)", status,
                 status_expected_val);
    }

    return ret_val;
}

/**
 * @brief Get TX tune status of Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @return
 *      - ESP_OK: TX Tune Status command successful.
 *      - ESP_FAIL: TX Tune Status command failed.
 */
esp_err_t si4713_tx_tune_status(i2c_master_dev_handle_t dev_handle)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t arg = 0x01U; // clear STC interrupt
    si4713_send_cmd(dev_handle, TX_TUNE_STATUS, &arg, sizeof(arg));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_TUNE_STATUS status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_TUNE_STATUS status = 0x%X (expected: 0x%X)", status,
                 status_expected.val);
    }

    if (ESP_OK == ret_val)
    {
        uint8_t response[8];
        si4713_read_response(dev_handle, response, sizeof(response));

        ESP_LOGI(SI4713_TAG, "\tread frequency = %u",
                 ((response[2] << 8U) & 0xFF00U) | (response[3] & 0xFFU));
        ESP_LOGI(SI4713_TAG, "\tread transmit voltage = 0x%X",
                 ((response[4] << 8U) & 0xFF00U) | (response[5] & 0xFFU));
        ESP_LOGI(SI4713_TAG, "\tread antenna tuning capacitor = 0x%X", response[6]);
        ESP_LOGI(SI4713_TAG, "\tread received noise level = 0x%X", response[7]);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "CTS timeout expired");
    }

    return ret_val;
}

/**
 * @brief Get audio signal quality and current FM transmit frequency.
 *
 * @param[in] dev_handle I2C master device handle.
 * @return
 *      - ESP_OK: TX ASQ Status command successful.
 *      - ESP_FAIL: TX ASQ Status command failed.
 */
esp_err_t si4713_tx_asq_status(i2c_master_dev_handle_t dev_handle)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t arg = 0x01U; // clear ASQINT bit
    si4713_send_cmd(dev_handle, TX_ASQ_STATUS, &arg, sizeof(arg));

    si4713_status_response_t status;
    const si4713_status_response_t status_expected = {.cts = 1U};
    si4713_read_status(dev_handle, &status, status_expected, T_CTS_SHORT_MS);

    if (status.val == status_expected.val)
    {
        ret_val = ESP_OK;
        ESP_LOGI(SI4713_TAG, "TX_ASQ_STATUS status = 0x%X", status);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "TX_ASQ_STATUS status = 0x%X (expected: 0x%X)", status,
                 status_expected.val);
    }

    if (ESP_OK == ret_val)
    {
        uint8_t response[5];
        si4713_read_response(dev_handle, response, sizeof(response));

        ESP_LOGI(SI4713_TAG, "\tovermodulation detection = %d", response[1] & 0x4U);
        ESP_LOGI(SI4713_TAG, "\tinput audio level threshold detect high = %d", response[1] & 0x2U);
        ESP_LOGI(SI4713_TAG, "\tinput audio level threshold detect low = %d", response[1] & 0x1U);
        ESP_LOGI(SI4713_TAG, "\tread frequency = %u",
                 ((response[2] << 8U) & 0xFF00U) | (response[3] & 0xFFU));
        ESP_LOGI(SI4713_TAG, "\tinput audio level in dBfs = %hhd", response[4]);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "CTS timeout expired");
    }

    return ret_val;
}

/**
 * @brief Loads or clears the program service buffer.
 *
 * @note PS text has to be max. 96 characters long.
 * The full string is divided into 4 character blocks.
 * If text length is not a multiple of 4, the remaining characters are [SPACE].
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] text String to be put in PS buffer.
 * @param[in] text_len Length of text.
 * @return
 *      - ESP_OK: TX_RDS_PS command successful.
 *      - ESP_FAIL: TX_RDS_PS command failed.
 *      - ESP_ERR_INVALID_SIZE: Too long text length.
 *      - ESP_ERR_INVALID_ARG: NULL given as char array pointer.
 */
esp_err_t si4713_set_program_service_buffer(i2c_master_dev_handle_t dev_handle, const char *text,
                                            uint8_t text_len)
{
    if (NULL == text)
    {
        ESP_LOGE(SI4713_TAG, "Invalid RDS PS NULL argument");
        return ESP_ERR_INVALID_ARG;
    }
    else if (96U < text_len)
    {
        ESP_LOGE(SI4713_TAG, "Too long RDS PS text.");
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t args[5];

    if (NULL != text)
    {
        ESP_LOGI(SI4713_TAG, "Writing %s to PS", text);
    }

    int curr_char_idx = 0; // Current character index
    int psid = 0;          // PS data id
    while (curr_char_idx < text_len)
    {
        uint8_t characters_left = text_len - curr_char_idx;
        uint8_t substring_len = ((4 <= characters_left) ? 4 : characters_left);
        args[0] = psid;

        /* Text starts at index 1 */
        for (int i = 1; i < sizeof(args); i++)
        {
            if (i < (1 + substring_len))
            {
                /* Place text into args */
                args[i] = (uint8_t)text[curr_char_idx];
            }
            else
            {
                /* Fill unused arguments with 0x20=[SPACE] */
                args[i] = 0x20U;
            }
        }

        si4713_tx_rds_ps(dev_handle, &args[0]);
        curr_char_idx += 4;
        psid++;
    }

    return ESP_OK;
}

/**
 * @brief Loads or clears the RDS group buffer FIFO or circular buffer.
 *
 * @note RDS buffer text has to be max. 64 characters long.
 * The full string is divided into 4 character blocks.
 * If text length is not a multiple of 4, the remaining characters are [SPACE].
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] fifo FIFO buffer used if set. Circular buffer used otherwise.
 * @param[in] ldbuff RDS group buffer loaded if set.
 * @param[in] mtbuff RDS group buffer emptied if set.
 * @param[in] intack RDS group buffer interrupt cleared if set.
 * @param[in] text String to be put in RDS buffer.
 * @param[in] text_len Length of text.
 * @return
 *      - ESP_OK: TX_RDS_BUFF command successful.
 *      - ESP_FAIL: TX_RDS_BUFF command failed.
 *      - ESP_ERR_INVALID_SIZE: Too long text length.
 *      - ESP_ERR_INVALID_ARG: NULL given as char array pointer and text_len > 0.
 */
esp_err_t si4713_set_rds_buffer(i2c_master_dev_handle_t dev_handle, bool fifo, bool ldbuff,
                                bool mtbuff, bool intack, const char *text, uint8_t text_len)
{
    if (64U < text_len)
    {
        ESP_LOGE(SI4713_TAG, "Too long RDS buffer text.");
        return ESP_ERR_INVALID_SIZE;
    }
    else if ((NULL == text) && (text_len > 0U))
    {
        ESP_LOGE(SI4713_TAG, "Invalid RDS buffer NULL argument");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t args[7];
    args[0] = ((true == fifo) ? (1U << 7) : 0U);
    args[0] |= ((true == ldbuff) ? (1U << 2) : 0U);
    args[0] |= ((true == mtbuff) ? (1U << 1) : 0U);
    args[0] |= ((true == intack) ? 1U : 0U);
    args[1] = RDS_GT_2A;

    if (NULL != text)
    {
        ESP_LOGI(SI4713_TAG, "Writing %s to RDS buffer", text);
    }

    int curr_char_idx = 0; // Current character index
    int location = 0;      // Text location
    do
    {
        uint8_t characters_left = text_len - curr_char_idx;
        uint8_t substring_len = ((4 <= characters_left) ? 4 : characters_left);
        args[2] = location;

        /* Text starts at index 3 */
        for (int i = 3; i < sizeof(args); i++)
        {
            if (i < (3 + substring_len))
            {
                /* Place text into args */
                args[i] = (uint8_t)text[curr_char_idx];
            }
            else
            {
                /* Fill unused arguments with 0x20=[SPACE] */
                args[i] = 0x20U;
            }
        }

        si4713_tx_rds_buff(dev_handle, &args[0]);
        curr_char_idx += 4;
        location++;

        // emptying buffer only once
        if (0U != (args[0] & (1U << 1)))
        {
            args[0] &= ~(1U << 1);
        }
    } while (curr_char_idx < text_len);

    return ESP_OK;
}

/**
 * @brief Broadcast time using RDS group 4A.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] fifo FIFO buffer used if set. Circular buffer used otherwise.
 * @param[in] ldbuff RDS group buffer loaded if set.
 * @param[in] mtbuff RDS group buffer emptied if set.
 * @param[in] intack RDS group buffer interrupt cleared if set.
 * @param[in] time Time to be broadcast.
 * @return
 *      - ESP_OK: TX_RDS_BUFF command successful.
 *      - ESP_FAIL: TX_RDS_BUFF command failed.
 */
esp_err_t si4713_set_time(i2c_master_dev_handle_t dev_handle, bool fifo, bool ldbuff, bool mtbuff,
                          bool intack, const si4713_time_t *time)
{
    uint16_t mjd = date_to_mjd(time->year, time->month, time->day);

    uint8_t args[7];
    args[0] = ((true == fifo) ? (1U << 7) : 0U);
    args[0] |= ((true == ldbuff) ? (1U << 2) : 0U);
    args[0] |= ((true == mtbuff) ? (1U << 1) : 0U);
    args[0] |= ((true == intack) ? 1U : 0U);
    args[1] = RDS_GT_4A;
    args[2] = mjd >> 15U;
    args[3] = (mjd & 0x7FFFU) >> 7U;
    args[4] = ((mjd & 0x7FU) << 1U) | ((time->hour & 0x10U) >> 4U);
    args[5] = ((time->hour & 0x0FU) << 4U) | ((time->minute & 0x3C0U) >> 2U);
    args[6] = (time->minute & 0x3U) << 6U;

    if (time->time_offset < 0)
    {
        /* 0 = + (east of Greenwich), 1 = − */
        args[6] |= (1U << 6U) | (((uint8_t)(-time->time_offset) << 1U) | 0x1FU);
    }
    else
    {
        args[6] |= (((uint8_t)time->time_offset << 1U) | 0x1FU);
    }

    esp_err_t ret_val = si4713_tx_rds_buff(dev_handle, &args[0]);

    return ret_val;
}