/*
 * File: si4713.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

/*==================================================================
    Includes
===================================================================*/

#include "si4713.h"
#include "si4713_cfg.h"
#include "freertos/FreeRTOS.h"

/*==================================================================
Object-like macros
===================================================================*/

/*==================================================================
Function-like macros
===================================================================*/

/*==================================================================
    Local types
===================================================================*/

/* FM transmitter command summary */
typedef enum
{
    POWER_UP = 0x01,        /* Power up device and mode selection. Modes include FM transmit and analog/digital audio interface configuration. */
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
    TX_TUNE_STATUS = 0x33,  /* Queries the status of a previously sent TX Tune Freq, TX Tune Power, or TX Tune Measure command. */
    TX_ASQ_STATUS = 0x34,   /* Queries the TX status and input audio signal metrics. */
    TX_RDS_BUFF = 0x35,     /* Si4713 Only. Queries the status of the RDS Group Buffer and loads new data into buffer. */
    TX_RDS_PS = 0x36,       /* Si4713 Only. Set up default PS strings. */
    GPO_CTL = 0x80,         /* Configures GPO3 as output or Hi-Z. */
    GPO_SET = 0x81,         /* Sets GPO3 output level (low or high) */
} si4713_command_t;

/* Structure of Si4713 status response */
typedef struct
{
    uint8_t stcint : 1; /* Seek/Tune Complete Interrupt. 0 = Tune complete has not been triggered. 1 = Tune complete has been triggered. */
    uint8_t asqint : 1; /* Signal Quality Interrupt. 0 = Signal quality measurement has not been triggered. 1 = Signal quality measurement has been triggered. */
    uint8_t rdsint : 1; /* RDS Interrupt. 0 = RDS interrupt has not been triggered. 1 = RDS interrupt has been triggered. */
    uint8_t : 3;        /* Reserved */
    uint8_t err : 1;    /* Error. 0 = No error, 1 = Error */
    uint8_t cts : 1;    /* Clear to Send. 0 = Wait before sending next command. 1 = Clear to send next command */
} si4713_status_response_t;

/*==================================================================
    Local objects
===================================================================*/

/*==================================================================
    Local function declarations
===================================================================*/

static void si4713_read_status(i2c_master_dev_handle_t dev_handle, si4713_status_response_t *status_ptr, uint32_t timeout_ms);
static void si4713_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *resp_ptr, uint8_t resp_len);
static inline void si4713_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd, const uint8_t *args, uint8_t arg_cnt);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Read status value from Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[out] status_ptr status variable pointer.
 * @param[in] timeout_ms Timeout for valid status response reception in ms.
 */
static void si4713_read_status(i2c_master_dev_handle_t dev_handle, si4713_status_response_t *status_ptr, uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();
    uint32_t timeout = pdMS_TO_TICKS(timeout_ms);
    do
    {
        ESP_ERROR_CHECK(i2c_read_response(dev_handle, (uint8_t *)status_ptr, 1));
    } while ((0U == status_ptr->cts) && ((xTaskGetTickCount() - start) < timeout));
}

/**
 * @brief Read multi-byte response from Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[out] resp_ptr Response buffer pointer.
 * @param[in] resp_len Response buffer length.
 */
static inline void si4713_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *resp_ptr, uint8_t resp_len)
{
    ESP_ERROR_CHECK(i2c_read_response(dev_handle, resp_ptr, resp_len));
}

/**
 * @brief Send command with args to Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] cmd Si4713 command code.
 * @param[in] args Si4713 command arguments.
 * @param[in] arg_cnt Si4713 command argument count.
 */
static inline void si4713_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd, const uint8_t *args, uint8_t arg_cnt)
{
    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, cmd, args, arg_cnt));
}

/**
 * @brief Perform Si4713 powerup
 *
 * This function sends Powerup in Analog Mode command to Si4713 and checks the response status.
 *
 * @param[in] dev_handle I2C master device handle.
 * @return
 *      - ESP_OK: Powerup succesful.
 *      - ESP_FAIL: Powerup failed.
 */
esp_err_t si4713_powerup_analog(i2c_master_dev_handle_t dev_handle)
{
    esp_err_t ret_val = ESP_FAIL;

    /*  0xC2 -> Set to FM Transmit. Enable interrupts.
        0x50 -> Set to Analog Line Input */
    uint8_t args[2] = {0xC2U, 0x50U};
    si4713_send_cmd(dev_handle, POWER_UP, args, sizeof(args));

    si4713_status_response_t status;
    si4713_read_status(dev_handle, &status, T_CTS_LONG_MS);

    if (1U == status.cts)
    {
        ret_val = ESP_OK;
    }
    ESP_LOGI(SI4713_TAG, "SI4713 powerup status = 0x%X", status);

    return ret_val;
}

/**
 * @brief Set property of Si4713.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] property Si4713 property.
 * @param[in] property_val Si4713 property value.
 * @return
 *      - ESP_OK: Set Property succesful.
 *      - ESP_FAIL: Set Property failed.
 */
esp_err_t si4713_set_property(i2c_master_dev_handle_t dev_handle, si4713_property_t property, uint16_t property_val)
{
    esp_err_t ret_val = ESP_FAIL;

    uint8_t args[5];
    args[0] = 0x00U;
    args[1] = ((uint16_t)property >> 8U) & 0xFFU;
    args[2] = (uint16_t)property & 0xFFU;
    args[3] = (property_val >> 8U) & 0xFFU;
    args[4] = property_val & 0xFFU;
    si4713_send_cmd(dev_handle, SET_PROPERTY, args, sizeof(args));

    si4713_status_response_t status;
    si4713_read_status(dev_handle, &status, T_CTS_SHORT_MS);
    if (1U == status.cts)
    {
        ret_val = ESP_OK;
    }
    ESP_LOGI(SI4713_TAG, "SI4713 set property 0x%X status = 0x%X", property, status);

    return ret_val;
}

/**
 * @brief Get Si4713 info.
 *
 * Returns the part number, chip revision, firmware revision, patch revision and component revision numbers.
 *
 * @param[in] dev_handle I2C master device handle.
 * @return
 *      - ESP_OK: Info reading succesful.
 *      - ESP_FAIL: Info reading failed.
 */
esp_err_t si4713_get_rev(i2c_master_dev_handle_t dev_handle)
{
    esp_err_t ret_val = ESP_FAIL;
    uint8_t arg = 0x00U;

    si4713_send_cmd(dev_handle, GET_REV, &arg, sizeof(arg));

    si4713_status_response_t status;
    si4713_read_status(dev_handle, &status, T_CTS_SHORT_MS);
    if (1U == status.cts)
    {
        ret_val = ESP_OK;
    }

    if (ESP_OK == ret_val)
    {
        uint8_t response[9];
        si4713_read_response(dev_handle, response, sizeof(response));

        ESP_LOGI(SI4713_TAG, "SI4713 part number = 0x%X", response[1]);                                              // (0x0D = Si4713)
        ESP_LOGI(SI4713_TAG, "SI4713 firmware revision = v%.2d.%.2d", (response[2] - 0x30U), (response[3] - 0x30U)); // in ASCII 0x30 = 0
        ESP_LOGI(SI4713_TAG, "SI4713 patch id = 0x%X", ((response[4] << 8U) & 0xFF00U) | (response[5] & 0xFFU));
        ESP_LOGI(SI4713_TAG, "SI4713 component firmware revision = v%.2d.%.2d", (response[6] - 0x30U), (response[7] - 0x30U));
        ESP_LOGI(SI4713_TAG, "SI4713 chip revision = rev%c", response[8]);
    }
    else
    {
        ESP_LOGE(SI4713_TAG, "SI4713 CTS timeout expired");
    }

    return ret_val;
}