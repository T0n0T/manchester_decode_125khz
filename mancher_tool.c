/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-17     Administrator       the first version
 */
#include <board.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_mancher_rfid.h>

#define DBG_TAG "mancher.tool"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

rt_uint8_t buf[128];
rt_uint8_t buf1[128];
rt_uint8_t mancher_line[11];
rt_uint8_t mancher_code[5];

static rt_uint8_t check_line_even(rt_uint8_t arg)
{
    int i;
    rt_uint8_t sum = 0;

    for (i = 0; i < 5; i++) {
        sum ^=(arg>>i)&0x01;
    }

    return sum;
}


static rt_uint8_t find_stream_head(rt_uint8_t *rfbuf)
{
    rt_uint16_t cnt, idx;
    rt_uint8_t st = 0, rfid_cnt = 0;

    //先读到停止位[10] = 0,再读到第一个同步位[01] = 1，即读到1001；也可以多读一个同步位[01] = 1则读100101
    for (cnt = 0; cnt < CODE_NUM; cnt++) {
        switch (st) {
        case 0:
            if (rfbuf[rfid_cnt]) {
                //读到 [1],标记码流可能有效的起始编号
                st++;
            }
            break;
        case 1:
            if (0 == rfbuf[rfid_cnt]) {
                //读到停止位 [10],到case 2
                st++;
            }
            break;
        case 2:
            if (0 == rfbuf[rfid_cnt]) {
                //再次读到 [100],到case 3
                idx = rfid_cnt;
                st++;
            } else {
                st = 1;
            }
            break;
        case 3:
            if (rfbuf[rfid_cnt]) {
                //读到停止位 [1001],到case 4
                st++;
            } else {
                st = 0;
                idx = 0xFF;
            }
            break;
        case 4:
            if (0 == rfbuf[rfid_cnt]) {
                //读到停止位 [10010],到case 5
                st++;
            } else {
                st = 1;
            }
            break;
        case 5:
            if (rfbuf[rfid_cnt]) {
                //目前必包含[100101]
                for (cnt = 0; cnt < 128; cnt++) {
                    /* 64位曼彻斯特码例子
                     * 1  1  1  1  1  1  1  1  1/
                     * 01 01 01 01 01 01 01 01 01
                     * 0  1  0  0       1       4
                     * 10 01 10 10      01
                     * 1  0  0  0       1       8
                     * 01 10 10 10      01
                     * 0  0  0  0       0       0
                     * 10 10 10 10      10
                     * 0  0  0  0       0       0
                     * 10 10 10 10      10
                     * 0  1  1  1       1       7
                     * 10 01 01 01      01
                     * 1  0  1  0       0       A
                     * 01 10 01 10      10
                     * 0  0  1  0       1       2
                     * 10 10 01 10      01
                     * 0  1  1  1       1       7
                     * 10 01 01 01      01
                     * 0  1  0  1       0       5
                     * 10 01 10 01      10
                     * 1  1  1  0       1       E
                     * 01 01 01 10      00      
                     * 1  1  1  1       0       列校验行
                     * 01 01 01 01    **10      停止位10接01行成1001
                     */
                    buf[cnt] = rfbuf[idx];
                    idx++;
                    //保证编号小于256
                    idx &= 0xFF;
                    buf1[cnt] = rfbuf[idx];
                    idx++;
                    idx &= 0xFF;
                }
                return 1;
            } else {
                //目前必包含[1000]
                st = 0;
                idx = 0xFF;
            }
            break;
        default:
            idx = 0xFF;
            st = 0;
            break;
        }
        rfid_cnt++;
        //保证0到127
        rfid_cnt &= 0x7f;
    }
    return 0;
}

static rt_uint8_t stream_decode(rt_uint8_t* pbuf)
{
    rt_uint8_t i, j, ii, flag = 0;
    rt_uint8_t bit1;
    //同步头获取
    for (i = 0, j = 0; j < CODE_NUM;) {
        for (ii = 0; ii < 9; ii++) {
            bit1 = pbuf[i++];
            //确保i小于64
            i &= 0xFF;
            //j用于保证能够退出循环，如pbuf[127]=0,下一次循环则可以退出
            j++;
            if (0 == bit1) {
                break;
            }
        }
        if (9 == ii) {
            //获取到9个1
            flag = 1;
            break;
        }
    }
    if (0 == flag) {
        return 0;
    }
    // data
    for (ii = 0; ii < 11; ii++) {
        //一共11组值，10组为数据位，最后一组是竖校验和停止位，全放入id中
        mancher_line[ii] = 0;
        for (j = 0; j < 5; j++) {
            //取出同步头后的数据，第一次取的是同步头后第一个数据，一次取5个值
            bit1 = pbuf[i++];
            i &= 0xFF;
            //左移1位，后面给最低位赋新值
            mancher_line[ii] <<= 1;
            if (bit1)
                mancher_line[ii] |= 0x01;
            else
                mancher_line[ii] &= ~0x01;
        }
    }
    //校验
    for (ii = 0; ii < 10; ii++) {
        //行校验
        if (check_line_even(mancher_line[ii]))
            return 0;
        //将校验位数据去除
        mancher_line[ii] >>= 1;
    }
    //列校验
    flag = 0;
    for (ii = 0; ii < 11; ii++) {
        flag ^=mancher_line[ii];
    }
    if (flag!=0)
        return 0;

    return 1;
}

static void dev_rfid_analy(mancher_t dev)
{
    rt_uint8_t flag;
    rt_uint8_t rfbuf[CODE_NUM];

    /*rfid_code 全部置零
     rfbuf 全部置零*/
    rt_memset(mancher_code, 0, sizeof(mancher_code));
    rt_memset(rfbuf, 0, sizeof(rfbuf));

    // 获取码流，256电位，完整一帧128码元必然包含其中
    if (!mancher_level(dev, rfbuf, CODE_NUM))
        return;

    // 查找码流起始位置
    if (!find_stream_head(rfbuf))
        return;

    /* 码流解码 */
    flag = stream_decode(buf);
    if (0 == flag)
        flag = stream_decode(buf1);
    if (flag) {
        mancher_code[0] = (mancher_line[0] << 4) | mancher_line[1];
        mancher_code[1] = (mancher_line[2] << 4) | mancher_line[3];
        mancher_code[2] = (mancher_line[4] << 4) | mancher_line[5];
        mancher_code[3] = (mancher_line[6] << 4) | mancher_line[7];
        mancher_code[4] = (mancher_line[8] << 4) | mancher_line[9];
    }
}

rt_uint8_t* mancher_read(mancher_t dev)
{
    //RFID 算法
    dev_rfid_analy(dev);

    return mancher_code;
}
