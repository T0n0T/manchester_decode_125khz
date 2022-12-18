/**
 * @file drv_mancher_rfid.c
 * @author T0n0T
 * @brief
 * @version 0.1
 * @date 2022-12-15
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <board.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_mancher_rfid.h>

#define DBG_TAG "drv.mancher"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static struct mancher_device mancher_obj[] =
    {
        MANCHER0_CONFIG,
    };

static rt_uint8_t get_level(TIM_HandleTypeDef *timer, rt_base_t pin)
{
    rt_uint32_t cnt = timer->Instance->CNT;
    int value, value_compare;
    rt_sem_release(start);
    value = rt_pin_read(pin);
    // do {
    //     value = rt_pin_read(pin);
    //     while ((timer->Instance->CNT - cnt) < 20);
    //     cnt = timer->Instance->CNT;
    //     value_compare = rt_pin_read(pin);
    // } while (value != value_compare);
    // LOG_D("%d ",value);
    return value;
}

rt_uint32_t mancher_level(mancher_t device, rt_uint8_t *buf, rt_size_t size)
{
    int value, i, len = 0;
    rt_uint8_t rfbuf[128 + 2] = {0};
    TIM_HandleTypeDef *tim = RT_NULL;

    RT_ASSERT(device != RT_NULL);

    // rt_enter_critical();

    tim = (TIM_HandleTypeDef *)device->timer->user_data;

    tim->Instance->CNT = 0;
    do {
        value = get_level(tim, device->dout);
        // 读到0电平，持续需要小于最大TTMAX，最大未跳变时间，进入下一步
        if (tim->Instance->CNT > TTMAX) {
            goto error;
        }
    } while (value == 0);

    tim->Instance->CNT = 0;
    do {
        value = get_level(tim, device->dout);
        // 读到0电平后读1电平，持续需要小于最大TTMAX，进入下一步
        if (tim->Instance->CNT > TTMAX) {
            goto error;
        }
    } while (value == 1);
    // 码值起始位，0→1（曼彻斯特逻辑1）得到，有码流输入，则进入下一步

    for (i = 0; i < 128;) {
        tim->Instance->CNT = 0;
        do {
            value = get_level(tim, device->dout);
            if (tim->Instance->CNT > TTMAX) {
                goto error;
            }
        } while (value == 0);
        // 获取一个0电平后下一步

        if (tim->Instance->CNT > TL_MIN && tim->Instance->CNT < TL_MAC) {
            // 大于曼彻斯特码1位电平持续的最小值（1电位），且小于2位电平最大持续（2电位），判定为两个电平，写00
            rfbuf[i++] = 0;
            rfbuf[i++] = 0;
        } else if (tim->Instance->CNT > TH_MIN && tim->Instance->CNT < TH_MAX) {
            // 大于曼彻斯特码半位电平持续的最小值（半电位），且小于一位电平最大持续（1电位），判定为两个电平写0
            rfbuf[i++] = 0;
        } else {
            goto error;
        }
        if (i >= 128)
            break;

        // 同0电平获取
        tim->Instance->CNT = 0;
        do {
            value = get_level(tim, device->dout);
            if (tim->Instance->CNT > TTMAX) {
                goto error;
            }
        } while (value == 1);
        if (tim->Instance->CNT > TL_MIN && tim->Instance->CNT < TL_MAC) {
            // 大于曼彻斯特码1位电平持续的最小值（1电位），且小于2位电平最大持续（2电位），判定为两个电平，写11
            rfbuf[i++] = 1;
            rfbuf[i++] = 1;
        } else if (tim->Instance->CNT > TH_MIN && tim->Instance->CNT < TH_MAX) {
            // 大于曼彻斯特码半位电平持续的最小值（半电位），且小于一位电平最大持续（1电位），判定为两个电平写1
            rfbuf[i++] = 1;
        } else {
            goto error;
        }
    }

    if (size >= (sizeof(rfbuf) - 2))
        len = (sizeof(rfbuf) - 2);
    else
        len = size;
    rt_memcpy(buf, rfbuf, len);
    LOG_D("buf: %02X", buf);

error:
    // rt_exit_critical();
    // LOG_E("len: %d", len);
    return len;
}

static rt_err_t mancher_timer(mancher_t device)
{
    rt_err_t ret = RT_EOK;
    rt_hwtimer_mode_t mode; /* 定时器模式 */

    device->timer = rt_device_find(device->timer_name);
    if (device->timer == RT_NULL) {
        LOG_E("can't find %s device!", device->timer_name);
        return RT_ERROR;
    }

    /* 定时器以默认的1000000MHz打开，一次cnt是1us */
    ret = rt_device_open(device->timer, RT_DEVICE_OFLAG_RDWR);
    if (ret != RT_EOK) {
        LOG_E("open %s device failed!", device->timer_name);
        return ret;
    }

    mode = HWTIMER_MODE_PERIOD;
    ret = rt_device_control(device->timer, HWTIMER_CTRL_MODE_SET, &mode);
    if (ret != RT_EOK) {
        LOG_E("%s set mode failed!", device->timer_name);
        return ret;
    }

    LOG_D("%s init success", device->timer_name);

    return RT_EOK;
}

rt_err_t mancher_init(mancher_t device)
{
    rt_err_t ret = RT_EOK;
    rt_pin_mode(device->dout, PIN_MODE_INPUT_PULLUP);
    ret = mancher_timer(device);
    return ret;
}

rt_err_t mancher_start(mancher_t device)
{
    rt_hwtimerval_t timeout_s;
    /* 无需超时反馈，但需要使用write开启设备 */
    timeout_s.sec = 0xFFFF;
    timeout_s.usec = 0xFFFF;
    if (rt_device_write(device->timer, 0, &timeout_s, sizeof(timeout_s)) != sizeof(timeout_s)) {
        rt_kprintf("set timeout value failed\n");
        return RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t mancher_stop(mancher_t device)
{
    return rt_device_close(device->timer);
}

struct rt_rfid_ops ops =
    {
        .init = mancher_init,
        .get_code = mancher_read,
        .start = mancher_start,
        .stop = mancher_stop
    };

static rt_err_t rt_device_mancher_register(mancher_t mancher, const char *mancher_name)
{
    rt_device_t device = RT_NULL;
    device = rt_calloc(1, sizeof(struct rt_device));
    if (device == RT_NULL) {
        LOG_E("can't allocate memory for M606 device");
        rt_free(device);
    }

    /* register device */
    device->type = RT_Device_Class_Miscellaneous;
    device->user_data = (void *)mancher;

    return rt_device_register(device, mancher_name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
}

/**
 * 用于在设备管理器中查找设备
 * @param mancher_name 名称
 * @return 返回设备句柄
 */
mancher_t mancher_device_find(const char *mancher_name)
{
    mancher_t mancher;
    rt_device_t dev = rt_device_find(mancher_name);
    if (dev == RT_NULL) {
        LOG_E("device %s is not exist", mancher_name);
        return RT_NULL;
    }

    mancher = (mancher_t)dev->user_data;
    return mancher;
}

/**
 * 初始化mancher
 * @note 设备自动不能置于板级初始化之后，如放在组件初始化中
 * @return 初始化结果，0为正常，-1为失败
 */
int rt_hw_mancher_init(void)
{
    int i;
    for (i = 0; i < sizeof(mancher_obj) / sizeof(mancher_obj[0]); i++) {
        mancher_obj[i].mancher_ops = &ops;
        if (rt_device_mancher_register(&mancher_obj[i], mancher_obj[i].name) != RT_EOK) {
            LOG_E("Macher device %s register failed.", mancher_obj[i].name);
            return -RT_ERROR;
        }
    }

    return RT_EOK;
}
INIT_APP_EXPORT(rt_hw_mancher_init);
