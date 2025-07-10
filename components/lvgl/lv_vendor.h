/**
 * @file lvgl_vendor.h
 */

#ifndef LVGL_VENDOR_H
#define LVGL_VENDOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "driver/media_types.h"

typedef enum{
    STATE_INIT,
    STATE_RUNNING,
    STATE_STOP
} lvgl_task_state_t;

/**
 * horizontal  ->  水平的
vertical    ->  垂直的
 */
typedef struct {
    lv_coord_t lcd_hor_res;         /**< Horizontal resolution.*/
    lv_coord_t lcd_ver_res;         /**< Vertical resolution.*/

    /*绘制缓冲区*/
    lv_color_t *draw_buf_2_1;
    lv_color_t *draw_buf_2_2;

    /*帧缓冲区，即最终图像的缓存位置*/
    lv_color_t *frame_buf_1;
    lv_color_t *frame_buf_2;
#if CONFIG_LVGL_USE_TRIPLE_BUFFERS
    lv_color_t *frame_buf_3;
#endif
    uint32_t draw_pixel_size; /*绘制的像素的大小*/
    media_rotate_t rotation; /*显示屏幕的旋转角度 media_rotate_t */
} lv_vnd_config_t;


void lv_vendor_fs_init(void);
void lv_vendor_fs_deinit(void);
void lv_vendor_init(lv_vnd_config_t *config);
void lv_vendor_deinit(void);
void lv_vendor_start(void);
void lv_vendor_stop(void);
void lv_vendor_disp_lock(void);
void lv_vendor_disp_unlock(void);
int lv_vendor_display_frame_cnt(void);
int lv_vendor_draw_buffer_cnt(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_VENDOR_H*/

