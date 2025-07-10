#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <string.h>

#include <common/bk_include.h>
#include <components/log.h>

#include "lcd_panel_devices.h"
#include "dvp_sensor_devices.h"
#include "tp_sensor_devices.h"


#define TAG "bk_peripheral"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)



/* 外设初始化 */
void bk_peripheral_init(void)
{
	/*屏幕识别*/
	lcd_panel_devices_init();

	/*摄像头识别初始化*/
#ifdef CONFIG_DVP_CAMERA
	dvp_sensor_devices_init();
#endif

	/*触摸屏型号识别初始化*/
#ifdef CONFIG_TP //（ TP ==>  Touch Panel ）
	tp_sensor_devices_init();
#endif
}
