/*
 * File: si4713_cfg.h
 * Author: Lukasz Gruchala
 * Created: 2026-03-06
 */

#ifndef __SI4713_CFG_H__
#define __SI4713_CFG_H__

/*==================================================================
    Includes
===================================================================*/

#include <inttypes.h>

/*==================================================================
    Object-like macros
===================================================================*/

#define SI4713_TAG "SI4713" /* log tag */

#define GPIO_OUTPUT_SI4713_RST CONFIG_GPIO_OUTPUT_SI4713_RST /* reset pin */
#define SI4173_SENSOR_ADDR 0x63                              /* I2C address of the SI4173 transmitter */

/* timing */
#define T_CTS_LONG_MS 110U  /* CTS bit setting delay in ms (POWER_UP) */
#define T_CTS_SHORT_US 300U /* CTS bit setting delay in us (other commands) */
#define T_STC_LONG_MS 100U  /* STC bit setting delay in ms (TX_TUNE_FREQ, TX_TUNE_MEASURE) */
#define T_STC_SHORT_MS 20U  /* STC bit setting delay in ms (TX_TUNE_POWER) */
#define T_COMP_MS 10U       /* SET_PROPERTY max. execution time in ms */
#define T_INT_US 1U         /* interrupt duration in us after command is executed  */

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

#endif /* __SI4713_CFG_H__ */