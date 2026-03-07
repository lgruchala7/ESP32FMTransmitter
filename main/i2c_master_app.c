/*
 * File: i2c_master_app.c
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

/*==================================================================
Includes
===================================================================*/

#include "i2c_master_app.h"
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

/*==================================================================
    Local objects
===================================================================*/

/*==================================================================
    Local function declarations
===================================================================*/

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief I2C master initialization
 *
 * @param[out] bus_handle I2C master bus handle
 * @param[out] dev_handle I2C master device handle
 */
void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SI4173_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

/**
 * @brief Write one byte to then read a sequence of bytes from an I2C slave device.
 *
 * @note Some I2C device needs write configurations before reading data from it.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] reg_addr Register address of the slave.
 * @param[out] data Receive buffer.
 * @param[in] len Receive buffer len.
 * @return Master write-read operation result.
 */
esp_err_t i2c_write_read_response(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read a sequence of bytes from an I2C slave device.
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[out] data Receive buffer.
 * @param[in] len Receive buffer len.
 * @return Master read operation result.
 */
esp_err_t i2c_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len)
{
    return i2c_master_receive(dev_handle, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Send a command with arguments to a SI4713 sensor.
 *
 * This function
 *
 * @param[in] dev_handle I2C master device handle.
 * @param[in] cmd Command code.
 * @param[in] args Command argument array.
 * @param[in] arg_cnt Number of arguments.
 * @return Master write operation result.
 */
esp_err_t i2c_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd, uint8_t *args, uint8_t arg_cnt)
{
    uint8_t write_buf[arg_cnt + 1U];
    uint8_t write_buf_idx = 0U;

    write_buf[write_buf_idx] = cmd;
    write_buf_idx++;

    while (write_buf_idx < (arg_cnt + 1U))
    {
        write_buf[write_buf_idx] = args[write_buf_idx - 1U];
        write_buf_idx++;
    }

    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}