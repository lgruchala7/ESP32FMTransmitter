/*
 * File: i2c_master_app.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

#ifndef __I2C_MASTER_APP_H__
#define __I2C_MASTER_APP_H__

/*==================================================================
    Includes
===================================================================*/

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_system.h"

/*==================================================================
    Object-like macros
===================================================================*/
#define I2C_MASTER_TAG "I2C_MASTER" /* log tag */

#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0                       /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                    /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                    /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

/*==================================================================
    Function-like macros
===================================================================*/

/*==================================================================
    Exported types
===================================================================*/

/*==================================================================
    Exported objects
===================================================================*/

/*==================================================================
    Function declarations
===================================================================*/

void i2c_init(i2c_master_bus_handle_t *bus_handle);
void i2c_add_device(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle,
                    uint16_t dev_addr);
esp_err_t i2c_write_read_response(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr,
                                  uint8_t *data, size_t len);
esp_err_t i2c_read_response(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len);
esp_err_t i2c_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd, const uint8_t *args,
                       uint8_t arg_cnt);

#endif /* __I2C_MASTER_APP_H__ */
