#include <common/bk_include.h>
#include <components/log.h>

#if CONFIG_MEDIA
#include "media_core.h"
#endif

#include "media_ipc.h"
#include "bk_peripheral.h"

#include "audio_osi_wrapper.h"
#include "video_osi_wrapper.h"

#if (CONFIG_SOC_BK7258 && CONFIG_SYS_CPU0)
#include "media_unit_test.h"
#endif

#define TAG "ME INIT"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


int media_service_init(void)
{
	bk_err_t ret = BK_OK;

#if (CONFIG_MEDIA)
#if (CONFIG_SOC_BK7258 && CONFIG_SYS_CPU1)
	bk_peripheral_init(); // NOTE（LCD、Camera、Tp识别、初始化 注册列表 ）
#endif
#if (CONFIG_SOC_BK7256 && CONFIG_SYS_CPU0)
	bk_peripheral_init();
#endif
#endif

	/* NOTE 视频 操作函数初始化 */
	ret = bk_video_osi_funcs_init();
	if (ret != kNoErr)
	{
		LOGE("%s, bk_video_osi_funcs_init failed\n", __func__);
		return ret;
	}

	/* NOTE 音频 操作函数初始化 */
	ret = bk_audio_osi_funcs_init();
	if (ret != kNoErr)
	{
		LOGE("%s, bk_audio_osi_funcs_init failed\n", __func__);
		return ret;
	}

#ifdef CONFIG_MEDIA_IPC
	media_ipc_init(); // REVIEW  Inter-Processor Communication（核间通信）
#endif

#if (CONFIG_MEDIA && !CONFIG_SOC_BK7258)
#if (CONFIG_SYS_CPU1)
	bk_printf("media_minor_init have start\r\n"); // 未打印
	media_minor_init(); // this CPU1???
#else
	bk_printf("media_major_init have start\r\n"); // 未打印
	media_major_init(); // CPU0
	extern int media_cli_init(void);
	media_cli_init();
#endif
#endif

/// XXX
#if (CONFIG_MEDIA && CONFIG_SOC_BK7258)
#if (CONFIG_MEDIA_MAJOR) // CPU1
	bk_printf("media_major_mailbox_init have init\r\n"); // 后
	media_major_mailbox_init(); // lvgl audio 
#elif (CONFIG_MEDIA_MINOR) // CPU2
	/* REVIEW 是不是CPU2 是用不了打印 执行是肯定是有执行的 */
	bk_printf("media_minor_mailbox_init have init\r\n");
    media_minor_mailbox_init(); // ASR
#else
    // this
    bk_printf("media_app_mailbox_init ready to init\r\n"); // 先 CPU0
	media_app_mailbox_init(); // NOTE (与语音有关)  运行了这个！！！
	extern int media_cli_init(void);
	media_cli_init();
#endif
#endif

#if (CONFIG_MEDIA_MAJOR | CONFIG_SYS_CPU1)
#else
#if (CONFIG_NET_WORK_VIDEO_TRANSFER)
	// extern int cli_video_init(void);
	// cli_video_init();
#endif

#if (CONFIG_DVP_CAMERA_TEST) /* 命令行 */
	extern int cli_image_save_init(void);
	cli_image_save_init();
	extern int cli_dvp_init(void);
	cli_dvp_init();
#endif

#if (CONFIG_IDF_TEST) // NO
	extern int cli_idf_init(void);
	cli_idf_init();
#endif

#if (CONFIG_DOORBELL) // NO
	extern int cli_doorbell_init();
	cli_doorbell_init();
#endif

#if (CONFIG_AUD_INTF_TEST) // NO
	extern int cli_aud_intf_init(void);
	cli_aud_intf_init();
#endif

#if (CONFIG_SOC_BK7258 && CONFIG_SYS_CPU0)
	media_unit_test_cli_init();
#endif

#endif
	return 0;
}
