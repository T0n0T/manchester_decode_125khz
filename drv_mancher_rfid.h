/**
 * @file drv_mancher_rfid.h
 * @author T0n0T
 * @brief 
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#ifndef DRIVERS_INCLUDE_DRV_RFID_H_
#define DRIVERS_INCLUDE_DRV_RFID_H_

#include "drv_common.h"



/**
 * @brief 捕捉电平的数量范围
 * @details 125khz载波与2khz数据进行AM调制，每bit会使用64个载波，
 */
#define TTMAX       1536        //电平的最大值
#define TH_MIN       128        //半位电平的最小值
#define TH_MAX       352        //半位电平的最大值
#define TL_MIN       384        //一位电平的最小值
#define TL_MAC      1280        //一位电平的最大值

#define MANCHER0_CONFIG                                 \
{                                                       \
    .name           = "mancher0",                       \
    .dout           = 27,                               \
    .timer_name     = "timer3",                         \
}

// rfid直接设备
typedef struct mancher_device *mancher_t;
struct mancher_device
{
    const char* name;
    rt_base_t   dout;
    const char* timer_name;
    rt_device_t timer;

    struct rt_rfid_ops *mancher_ops;
};

struct rt_rfid_ops
{
    rt_err_t        (*init)(mancher_t mancher);
    rt_err_t        (*start)(mancher_t mancher);
    rt_err_t        (*stop)(mancher_t mancher);
    rt_uint8_t*     (*get_code)(mancher_t mancher);
};

mancher_t mancher_device_find(const char *mancher_name);

rt_uint32_t mancher_level(mancher_t device, rt_uint8_t *buf, rt_size_t size);
rt_uint8_t* mancher_read(mancher_t dev);

#endif /* DRIVERS_INCLUDE_DRV_RFID_H_ */
