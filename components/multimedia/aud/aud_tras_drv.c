// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//#include <common/bk_include.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <driver/aud_adc_types.h>
#include <driver/aud_adc.h>
#include <driver/aud_dac_types.h>
#include <driver/aud_dac.h>

#include <driver/dma.h>
#include <bk_general_dma.h>
#include "sys_driver.h"
#include "aud_intf_private.h"
#include "aud_tras_drv.h"
#include <driver/psram.h>
#if CONFIG_AEC_VERSION_V3
#include <modules/aec_v3.h>
#elif CONFIG_AEC_VERSION_V2
#include <modules/aec_v2.h>
#else
#include <modules/aec.h>
#endif
#include <driver/audio_ring_buff.h>
#include <modules/g711.h>
#include "gpio_driver.h"
#include <driver/gpio.h>

#if CONFIG_AUD_INTF_SUPPORT_G722
#include <modules/g722.h>
#endif

#include "bk_misc.h"
#include <soc/mapping.h>

#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include <bk_general_dma.h>
#include <driver/uart.h>
#include "gpio_driver.h"

#include "psram_mem_slab.h"

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
#include "prompt_tone.h"
#endif
#include "ring_buffer.h"
#include "prompt_tone_play.h"
#endif

#if CONFIG_AI_ASR_MODE_CPU2
#if (CONFIG_CPU_CNT > 2)
#include "components/system.h"
#endif
#endif

#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif

#define AUD_TRAS_DRV_TAG "tras_drv"

#define LOGI(...) BK_LOGI(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
bool aec_all_data_flag = false;
#endif//CONFIG_DEBUG_DUMP

RingBufferContext *i4s_aud_rx_rb = NULL;




//#define AEC_DATA_DUMP_BY_UART

#ifdef AEC_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_aec_data_uart_util = {0};
#define AEC_DATA_DUMP_UART_ID            (1)
#define AEC_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define AEC_DATA_DUMP_BY_UART_OPEN()                        uart_util_create(&g_aec_data_uart_util, AEC_DATA_DUMP_UART_ID, AEC_DATA_DUMP_UART_BAUD_RATE)
#define AEC_DATA_DUMP_BY_UART_CLOSE()                       uart_util_destroy(&g_aec_data_uart_util)
#define AEC_DATA_DUMP_BY_UART_DATA(data_buf, len)           uart_util_tx_data(&g_aec_data_uart_util, data_buf, len)
#else
#define AEC_DATA_DUMP_BY_UART_OPEN()
#define AEC_DATA_DUMP_BY_UART_CLOSE()
#define AEC_DATA_DUMP_BY_UART_DATA(data_buf, len)
#endif  //AEC_DATA_DUMP_BY_UART


//#define SPK_DATA_DUMP_BY_UART

#ifdef SPK_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_spk_data_uart_util = {0};
#define SPK_DATA_DUMP_UART_ID            (1)
#define SPK_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define SPK_DATA_DUMP_BY_UART_OPEN()                        uart_util_create(&g_spk_data_uart_util, SPK_DATA_DUMP_UART_ID, SPK_DATA_DUMP_UART_BAUD_RATE)
#define SPK_DATA_DUMP_BY_UART_CLOSE()                       uart_util_destroy(&g_spk_data_uart_util)
#define SPK_DATA_DUMP_BY_UART_DATA(data_buf, len)           uart_util_tx_data(&g_spk_data_uart_util, data_buf, len)
#else
#define SPK_DATA_DUMP_BY_UART_OPEN()
#define SPK_DATA_DUMP_BY_UART_CLOSE()
#define SPK_DATA_DUMP_BY_UART_DATA(data_buf, len)
#endif  //SPK_DATA_DUMP_BY_UART


#define AUD_MEDIA_SEM_ENABLE    0

//#define VOICE_MODE_DEBUG   //GPIO debug

#ifdef VOICE_MODE_DEBUG
#define AUD_ADC_DMA_ISR_START()                 do { GPIO_DOWN(32); GPIO_UP(32);} while (0)
#define AUD_ADC_DMA_ISR_END()                   do { GPIO_DOWN(32); } while (0)

#define AUD_AEC_PROCESS_START()                 do { GPIO_DOWN(33); GPIO_UP(33);} while (0)
#define AUD_AEC_PROCESS_END()                   do { GPIO_DOWN(33); } while (0)

#define AUD_ENC_PROCESS_START()                 do { GPIO_DOWN(34); GPIO_UP(34);} while (0)
#define AUD_ENC_PROCESS_END()                   do { GPIO_DOWN(34); } while (0)

#define AUD_DAC_DMA_ISR_START()                 do { GPIO_DOWN(27); GPIO_UP(27);} while (0)
#define AUD_DAC_DMA_ISR_END()                   do { GPIO_DOWN(27); } while (0)

#define AUD_DEC_PROCESS_START()                 do { GPIO_DOWN(36); GPIO_UP(36);} while (0)
#define AUD_DEC_PROCESS_END()                   do { GPIO_DOWN(36); } while (0)

#define AUD_PLAY_PROMPT_TONE_START()            do { GPIO_DOWN(37); GPIO_UP(37);} while (0)
#define AUD_PLAY_PROMPT_TONE_END()              do { GPIO_DOWN(37); } while (0)

#define AUD_STOP_PROMPT_TONE_START()            do { GPIO_DOWN(38); GPIO_UP(38);} while (0)
#define AUD_STOP_PROMPT_TONE_END()              do { GPIO_DOWN(38); } while (0)

#define AUD_SET_SPK_SOURCE_START()              do { GPIO_DOWN(31); GPIO_UP(31);} while (0)
#define AUD_SET_SPK_SOURCE_END()                do { GPIO_DOWN(31); } while (0)
#else
#define AUD_ADC_DMA_ISR_START()
#define AUD_ADC_DMA_ISR_END()

#define AUD_AEC_PROCESS_START()
#define AUD_AEC_PROCESS_END()

#define AUD_ENC_PROCESS_START()
#define AUD_ENC_PROCESS_END()

#define AUD_DAC_DMA_ISR_START()
#define AUD_DAC_DMA_ISR_END()

#define AUD_DEC_PROCESS_START()
#define AUD_DEC_PROCESS_END()

#define AUD_PLAY_PROMPT_TONE_START()
#define AUD_PLAY_PROMPT_TONE_END()

#define AUD_STOP_PROMPT_TONE_START()
#define AUD_STOP_PROMPT_TONE_END()

#define AUD_SET_SPK_SOURCE_START()
#define AUD_SET_SPK_SOURCE_END()
#endif





#define CONFIG_AUD_TRAS_AEC_MIC_DELAY_POINTS   53
//#define CONFIG_AUD_RING_BUFF_SAFE_INTERVAL    20

#define CONFIG_UAC_MIC_SPK_COUNT_DEBUG
#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
#define UAC_MIC_SPK_COUNT_DEBUG_INTERVAL (1000 * 2)
#endif

#define TU_QITEM_COUNT      (120)
static beken_thread_t  aud_trs_drv_thread_hdl = NULL;
static beken_queue_t aud_trs_drv_int_msg_que = NULL;

/* NOTE REVIEW XXX */
/* ring_buffer : 是不停的接收的音频数据 而 aud_tras_drv_info负责与其交互 比如什么是否取，什么时候放 */
aud_tras_drv_info_t aud_tras_drv_info = DEFAULT_AUD_TRAS_DRV_INFO(); /* 此文件内最重要的参数 */
static bool uac_mic_read_flag = false; static bool uac_spk_write_flag = false;
static uint8_t spk_play_flag = 0;

media_mailbox_msg_t uac_connect_state_msg = {0};

media_mailbox_msg_t mic_to_media_app_msg = {0};
media_mailbox_msg_t spk_to_media_app_msg = {0};
#if AUD_MEDIA_SEM_ENABLE
beken_semaphore_t mailbox_media_aud_mic_sem = NULL;
#endif
aud_tras_drv_mic_notify_t mic_nofity = {0, 0};

static beken_semaphore_t aud_tras_drv_task_sem = NULL;

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE // NOTE 不需要语音唤醒 就将这个宏定义关掉
/* NOTE mic data AEC 后 调用函数 */
static aud_tras_drv_aec_output_callback gl_aec_output_callback = NULL; 
static void *gl_user_data = NULL;
static bool gl_dialog_running = false; /* NOTE 是否对话中的标识  */
#endif

#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
static uint8_t mic_delay_num = 0;
#endif

#if CONFIG_AUD_INTF_SUPPORT_G722
static g722_encode_state_t g722_enc = {0};
static g722_decode_state_t g722_dec = {0};
#endif

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
#define PROMPT_TONE_RB_SIZE     (1280 * 8)
static ringbuf_handle_t gl_prompt_tone_rb = NULL;
static bool gl_prompt_tone_play_flag = false;
static prompt_tone_pool_empty_notify gl_prompt_tone_empty_notify = NULL;
static void *gl_notify_user_data = NULL;
static prompt_tone_play_handle_t gl_prompt_tone_play_handle = NULL;
static url_info_t prompt_tone_info = {0};

#if CONFIG_PROMPT_TONE_SOURCE_VFS
#if CONFIG_PROMPT_TONE_CODEC_MP3
static char asr_wakeup_prompt_tone_path[] = "/asr_wakeup_16k_mono_16bit.mp3";
static char asr_standby_prompt_tone_path[] = "/asr_standby_16k_mono_16bit.mp3";
static char network_provision_prompt_tone_path[] = "/network_provision_16k_mono_16bit_en.mp3";
static char network_provision_success_prompt_tone_path[] = "/network_provision_success_16k_mono_16bit_en.mp3";
static char network_provision_fail_prompt_tone_path[] = "/network_provision_fail_16k_mono_16bit_en.mp3";
static char reconnect_network_prompt_tone_path[] = "/reconnect_network_16k_mono_16bit_en.mp3";
static char reconnect_network_success_prompt_tone_path[] = "/reconnect_network_success_16k_mono_16bit_en.mp3";
static char reconnect_network_fail_prompt_tone_path[] = "/reconnect_network_fail_16k_mono_16bit_en.mp3";
static char rtc_connection_lost_prompt_tone_path[] = "/rtc_connection_lost_16k_mono_16bit_en.mp3";
static char agent_joined_prompt_tone_path[] = "/agent_joined_16k_mono_16bit_en.mp3";
static char agent_offline_prompt_tone_path[] = "/agent_offline_16k_mono_16bit_en.mp3";
static char low_voltage_prompt_tone_path[] = "/low_voltage_16k_mono_16bit_en.mp3";
#endif

#if CONFIG_PROMPT_TONE_CODEC_WAV
/*唤醒提示音*/
static char asr_wakeup_prompt_tone_path[] = "/asr_wakeup_16k_mono_16bit_en.wav"; 
/*结束提示音*/
static char asr_standby_prompt_tone_path[] = "/asr_standby_16k_mono_16bit_en.wav";
#if I4S_RECORD_REPEAT_MODE /* 暂且只存放wav格式的 */
static char asr_start_record_prompt_tone_path[] = "start_record.wav";
static char asr_stop_record_prompt_tone_path[] = "stop_record.wav";
#endif
/*提示请使用蓝牙配网*/
static char network_provision_prompt_tone_path[] = "/network_provision_16k_mono_16bit_en.wav";
/*蓝牙配网成功*/
static char network_provision_success_prompt_tone_path[] = "/network_provision_success_16k_mono_16bit_en.wav";
/*蓝牙配网失败*/
static char network_provision_fail_prompt_tone_path[] = "/network_provision_fail_16k_mono_16bit_en.wav";
/*配网中，请稍后*/
static char reconnect_network_prompt_tone_path[] = "/reconnect_network_16k_mono_16bit_en.wav";
/*配网成功*/
static char reconnect_network_success_prompt_tone_path[] = "/reconnect_network_success_16k_mono_16bit_en.wav";
/*配网失败，请检查网络*/
static char reconnect_network_fail_prompt_tone_path[] = "/reconnect_network_fail_16k_mono_16bit_en.wav";
/*设备断网*/
static char rtc_connection_lost_prompt_tone_path[] = "/rtc_connection_lost_16k_mono_16bit_en.wav";
/*agora 已连接*/
static char agent_joined_prompt_tone_path[] = "/agent_joined_16k_mono_16bit_en.wav";
/*agora 非连接*/
static char agent_offline_prompt_tone_path[] = "/agent_offline_16k_mono_16bit_en.wav";
/*低点，请充电*/
static char low_voltage_prompt_tone_path[] = "/low_voltage_16k_mono_16bit_en.wav";
#endif

#if CONFIG_PROMPT_TONE_CODEC_PCM
static char asr_wakeup_prompt_tone_path[] = "/asr_wakeup_16k_mono_16bit.pcm";
static char asr_standby_prompt_tone_path[] = "/asr_standby_16k_mono_16bit.pcm";
static char network_provision_prompt_tone_path[] = "/network_provision_16k_mono_16bit_en.pcm";
static char network_provision_success_prompt_tone_path[] = "/network_provision_success_16k_mono_16bit_en.pcm";
static char network_provision_fail_prompt_tone_path[] = "/network_provision_fail_16k_mono_16bit_en.pcm";
static char reconnect_network_prompt_tone_path[] = "/reconnect_network_16k_mono_16bit_en.pcm";
static char reconnect_network_success_prompt_tone_path[] = "/reconnect_network_success_16k_mono_16bit_en.pcm";
static char reconnect_network_fail_prompt_tone_path[] = "/reconnect_network_fail_16k_mono_16bit_en.pcm";
static char rtc_connection_lost_prompt_tone_path[] = "/rtc_connection_lost_16k_mono_16bit_en.pcm";
static char agent_joined_prompt_tone_path[] = "/agent_joined_16k_mono_16bit_en.pcm";
static char agent_offline_prompt_tone_path[] = "/agent_offline_16k_mono_16bit_en.pcm";
static char low_voltage_prompt_tone_path[] = "/low_voltage_16k_mono_16bit_en.pcm";
#endif
#endif  //CONFIG_PROMPT_TONE_SOURCE_VFS
#endif


#if I4S_RECORD_REPEAT_MODE
 volatile static uint8_t i4s_record_flag = 0;
 #define RECORD_REPEAT_START	(3) /* NOTE 开始录音重复模式 */
 #define RECORD_REPEAT_STOP		(4) /* NOTE 结束录音模式 */
#endif

#if CONFIG_AI_ASR_MODE_CPU2
typedef struct {
    unsigned char *data;
    unsigned int size;
	unsigned char spk_play_flag;
} asr_data_t;

static asr_data_t gl_asr_data = {0};
static media_mailbox_msg_t gl_asr_data_msg = {0};
static beken_semaphore_t gl_aud_cp2_ready_sem = NULL;
#define HI_ARMINO       (1) /*唤醒*/
#define BYEBYE_ARMINO   (2) /*休眠*/
#endif  //CONFIG_AI_ASR_MODE_CPU2

#if CONFIG_AUD_INTF_SUPPORT_MULTIPLE_SPK_SOURCE_TYPE
static spk_source_type_t spk_source_type = SPK_SOURCE_TYPE_VOICE;
static bk_err_t aud_tras_drv_set_spk_source_type(spk_source_type_t type);
#endif

#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
#define A2DP_MUSIC_SAMPLE_RATE      (44100)
static int32_t *a2dp_read_buff = NULL;
static uint32_t a2dp_frame_size = 0;
#endif


#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
typedef struct {
	beken_timer_t timer;
	uint32_t mic_size;
	uint32_t spk_size;
} uac_mic_spk_count_debug_t;

static uac_mic_spk_count_debug_t uac_mic_spk_count = {0};
#endif


/* extern api */
bk_err_t aud_tras_drv_deinit(void);
static bk_err_t aud_tras_drv_voc_start(void);
static bk_err_t aud_tras_drv_set_spk_gain(uint16_t value);


void *audio_tras_drv_malloc(uint32_t size)
{
#if CONFIG_AUD_TRAS_USE_SRAM
    return os_malloc(size);
#else
    return bk_psram_frame_buffer_malloc(PSRAM_HEAP_AUDIO, size);
#endif
}

void audio_tras_drv_free(void *mem)
{
#if CONFIG_AUD_TRAS_USE_SRAM
    os_free(mem);
#else
    return bk_psram_frame_buffer_free(mem);
#endif
}

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
static void uac_mic_spk_count_dump(void *param)
{
	uac_mic_spk_count.mic_size = uac_mic_spk_count.mic_size / 1024 / (UAC_MIC_SPK_COUNT_DEBUG_INTERVAL / 1000);
	uac_mic_spk_count.spk_size = uac_mic_spk_count.spk_size / 1024 / (UAC_MIC_SPK_COUNT_DEBUG_INTERVAL / 1000);

	LOGI("[UAC] mic: %uKB/s, spk: %uKB/s \n", uac_mic_spk_count.mic_size, uac_mic_spk_count.spk_size);
	uac_mic_spk_count.mic_size  = 0;
	uac_mic_spk_count.spk_size  = 0;
}

static void uac_mic_spk_count_close(void)
{
	bk_err_t ret = BK_OK;

	if (uac_mic_spk_count.timer.handle) {
		ret = rtos_stop_timer(&uac_mic_spk_count.timer);
		if (ret != BK_OK) {
			LOGE("%s, %d, stop aud_tx_count timer fail \n", __func__, __LINE__);
		}
		ret = rtos_deinit_timer(&uac_mic_spk_count.timer);
		if (ret != BK_OK) {
			LOGE("%s, %d, deinit aud_tx_count timer fail \n", __func__, __LINE__);
		}
		uac_mic_spk_count.timer.handle = NULL;
	}
	uac_mic_spk_count.mic_size = 0;
	uac_mic_spk_count.spk_size = 0;
	LOGI("%s, %d, close uac count timer complete \n", __func__, __LINE__);
}

static void uac_mic_spk_count_open(void)
{
	bk_err_t ret = BK_OK;

	if (uac_mic_spk_count.timer.handle != NULL)
	{
		ret = rtos_deinit_timer(&uac_mic_spk_count.timer);
		if (BK_OK != ret)
		{
			LOGE("%s, %d, deinit uac_mic_spk_count time fail \n", __func__, __LINE__);
			goto exit;
		}
		uac_mic_spk_count.timer.handle = NULL;
	}

	uac_mic_spk_count.mic_size = 0;
	uac_mic_spk_count.spk_size = 0;

	ret = rtos_init_timer(&uac_mic_spk_count.timer, UAC_MIC_SPK_COUNT_DEBUG_INTERVAL, uac_mic_spk_count_dump, NULL);
	if (ret != BK_OK) {
		LOGE("%s, %d, rtos_init_timer fail \n", __func__, __LINE__);
		goto exit;
	}
	ret = rtos_start_timer(&uac_mic_spk_count.timer);
	if (ret != BK_OK) {
		LOGE("%s, %d, rtos_start_timer fail \n", __func__, __LINE__);
		goto exit;
	}
	LOGI("%s, %d, open uac count timer complete \n", __func__, __LINE__);

	return;
exit:

	uac_mic_spk_count_close();
}

static void uac_mic_spk_count_add_mic_size(uint32_t mic_size)
{
	uac_mic_spk_count.mic_size += mic_size;
}

static void uac_mic_spk_count_add_spk_size(uint32_t spk_size)
{
	uac_mic_spk_count.spk_size += spk_size;
}
#endif

static void aud_tras_dac_pa_ctrl(bool en, bool delay_flag)
{
	if (en) {
#if CONFIG_AUD_TRAS_DAC_PA_CTRL
        LOGD("%s, %d, PA turn on \n", __func__, __LINE__);
        if (delay_flag) {
            /* delay XXms to avoid po audio data, and then open pa */
            delay_ms(CONFIG_AUD_TRAS_DAC_PA_OPEN_DELAY);
        }
        /* open pa according to congfig */
        //gpio_dev_unmap(AUD_DAC_PA_CTRL_GPIO);
        //bk_gpio_enable_output(AUD_DAC_PA_CTRL_GPIO);
#if AUD_DAC_PA_ENABLE_LEVEL
        bk_gpio_set_output_high(AUD_DAC_PA_CTRL_GPIO);
#else
        bk_gpio_set_output_low(AUD_DAC_PA_CTRL_GPIO);
#endif
#endif
	} else {
#if CONFIG_AUD_TRAS_DAC_PA_CTRL
        LOGD("%s, %d, PA turn off \n", __func__, __LINE__);
#if AUD_DAC_PA_ENABLE_LEVEL
		bk_gpio_set_output_low(AUD_DAC_PA_CTRL_GPIO);
#else
		bk_gpio_set_output_high(AUD_DAC_PA_CTRL_GPIO);
#endif
        if (delay_flag) {
            delay_ms(CONFIG_AUD_TRAS_DAC_PA_CLOSE_DELAY);
        }
#endif
	}
}

bk_err_t aud_tras_drv_send_msg(aud_tras_drv_op_t op, void *param)
{
	bk_err_t ret;
	aud_tras_drv_msg_t msg;

	msg.op = op;
	msg.param = param;
	if (aud_trs_drv_int_msg_que) {
		ret = rtos_push_to_queue(&aud_trs_drv_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, aud_tras_send_int_msg fail \n", __func__, __LINE__);
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static bk_err_t aud_tras_adc_config(aud_adc_config_t *adc_config)
{
	bk_err_t ret = BK_OK;

	/* init audio driver and config adc */
	ret = bk_aud_driver_init();
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio driver fail \n", __func__, __LINE__);
		goto aud_adc_exit;
	}

	ret = bk_aud_adc_init(adc_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio adc fail \n", __func__, __LINE__);
		goto aud_adc_exit;
	}

	return BK_OK;

aud_adc_exit:
	LOGE("%s, %d, audio adc config fail \n", __func__, __LINE__);
	bk_aud_driver_deinit();
	return BK_FAIL;
}

static bk_err_t aud_tras_dac_config(aud_dac_config_t *dac_config)
{
	bk_err_t ret = BK_OK;

	/* init audio driver and config dac */
	ret = bk_aud_driver_init();
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio driver fail \n", __func__, __LINE__);
		goto aud_dac_exit;
	}

	ret = bk_aud_dac_init(dac_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio dac fail \n", __func__, __LINE__);
		goto aud_dac_exit;
	}

	return BK_OK;

aud_dac_exit:
	LOGE("%s, %d, audio dac config fail \n", __func__, __LINE__);
	bk_aud_driver_deinit();
	return BK_FAIL;
}

#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
const uint16_t EQTAB[257] =
{
  8638,10022,10662,10996,11263,11527,11804,12099,12390,12624,12825,13007,13178,13344,13510,13763,
  13981,14146,14310,14471,14629,14783,14930,15069,15199,15319,15428,15527,15614,15688,15749,15810,
  15870,15929,15986,16040,16092,16139,16183,16222,16256,16285,16309,16327,16339,16345,16345,16345,
  16345,16345,16345,16345,16344,16343,16342,16339,16336,16331,16326,16319,16310,16300,16288,16275,
  16259,16241,16221,16199,16175,16148,16118,16086,16051,16014,15973,15930,15883,15834,15782,15727,
  15669,15608,15548,15488,15428,15368,15309,15249,15190,15130,15070,15009,14948,14886,14824,14761,
  14697,14632,14566,14499,14431,14361,14291,14219,14146,14071,13994,13916,13835,13753,13670,13582,
  13489,13392,13291,13190,13089,12987,12885,12780,12673,12563,12449,12330,12205,12076,11939,11796,
  11646,11487,11321,11144,10957,10761,10554,10337,10108,9868, 9616, 9351, 9074, 8783, 8481, 8165,
  7831, 7480, 7113, 6748, 6388, 6032, 5681, 5337, 4999, 4670, 4349, 4037, 3736, 3445, 3165, 2898,
  2643, 2401, 2173, 1960, 1761, 1577, 1408, 1256, 1120, 1001, 898,  812,  744,  692,  658,  642,
  624,  606,  587,  568,  549,  531,  512,  494,  476,  459,  442,  425,  409,  393,  378,  363,
  348,  334,  321,  308,  296,  284,  273,  263,  253,  244,  236,  228,  221,  214,  208,  203,
  197,  192,  186,  180,  174,  168,  162,  156,  151,  145,  140,  135,  130,  125,  121,  116,
  112,  108,  105,  101,  98,   95,   93,   90,   88,   86,   85,   84,   83,   82,   82,   82,
  82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82,   82
 };
#endif

// NOTE voc init 中
static bk_err_t aud_tras_drv_aec_cfg(void)  
{
	uint32_t aec_context_size = 0;
	uint32_t val = 0;
	aec_info_t *temp_aec_info = aud_tras_drv_info.voc_info.aec_info;
	/* init aec context */
#if CONFIG_AEC_VERSION_V3
    LOGI("%s, %d, use AEC Version V3 %d\n", __func__, __LINE__, aec_ver());
	aec_context_size = aec_size(0);	
#elif CONFIG_AEC_VERSION_V2
    LOGI("%s, %d, use AEC Version V2 %d\n", __func__, __LINE__,aec_ver());
		aec_context_size = aec_size(2000);
#else
    LOGI("%s, %d, use AEC Version V1 \n", __func__, __LINE__);
	aec_context_size = aec_size(1000);	
#endif

#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
    LOGI("%s, %d, use AEC hardware mode \n", __func__, __LINE__);
#else
    LOGI("%s, %d, use AEC software mode \n", __func__, __LINE__);
#endif

	LOGI("%s, %d, malloc aec size: %d \n", __func__, __LINE__, aec_context_size);
	temp_aec_info->aec = (AECContext*)audio_tras_drv_malloc(aec_context_size);
	//temp_aec_info->aec = (AECContext*)psram_malloc(aec_context_size);
	LOGI("%s, %d, aec use malloc sram: %p \n", __func__, __LINE__, temp_aec_info->aec);
	if (temp_aec_info->aec == NULL) {
		LOGE("%s, %d, malloc aec fail, aec_context_size: %d \n", __func__, __LINE__, aec_context_size);
		return BK_FAIL;
	}

	/* config sample rate, default is 8K */
	if (temp_aec_info->samp_rate == 8000 || temp_aec_info->samp_rate == 16000) {
		aec_init(temp_aec_info->aec, temp_aec_info->samp_rate);
	}

	/* 获取处理帧长，16000采样率320点(640字节)，8000采样率160点(320字节)  (对应20毫秒数据) */
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_GET_FRAME_SAMPLE, (uint32_t)(&(temp_aec_info->samp_rate_points)));

	/* 获取结构体内部可以复用的ram作为每帧tx,rx,out数据的临时buffer; ram很宽裕的话也可以在外部单独申请获取 */
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_GET_TX_BUF, (uint32_t)(&val)); temp_aec_info->mic_addr = (int16_t*)val;
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_GET_RX_BUF, (uint32_t)(&val)); temp_aec_info->ref_addr = (int16_t*)val;
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_GET_OUT_BUF,(uint32_t)(&val)); temp_aec_info->out_addr = (int16_t*)val;

	/* 以下是参数调节示例,aec_init中都已经有默认值,可以直接先用默认值 */
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_FLAGS, temp_aec_info->aec_config->init_flags);							//库内各模块开关; aec_init内默认赋值0x1f;

	/* NOTE 是否需要使用 AGC 自动增益控制？ */
	/* 回声消除相关 */
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
	temp_aec_info->aec_config->mic_delay = 16;//0x0
	temp_aec_info->aec_config->ec_depth = 0x2;//0x14
	temp_aec_info->aec_config->voice_vol =0x0d;//0xe
	temp_aec_info->aec_config->ns_level = 0x5;//0x2
	temp_aec_info->aec_config->ns_para = 0x02;//0x1
	temp_aec_info->aec_config->drc = 0x0;//0xf
#endif
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, temp_aec_info->aec_config->mic_delay);						//设置参考信号延迟(采样点数，需要dump数据观察)
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_EC_DEPTH, temp_aec_info->aec_config->ec_depth);							//建议取值范围1~50; 后面几个参数建议先用aec_init内的默认值，具体需要根据实际情况调试; 总得来说回声越大需要调的越大

#if CONFIG_AEC_VERSION_V1
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_TxRxThr, temp_aec_info->aec_config->TxRxThr);							//建议取值范围10~64
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_TxRxFlr, temp_aec_info->aec_config->TxRxFlr);							//建议取值范围1~10
#endif

	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_REF_SCALE, temp_aec_info->aec_config->ref_scale);						//取值0,1,2；rx数据如果幅值太小的话适当放大
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_VOL, temp_aec_info->aec_config->voice_vol);								//通话过程中如果需要经常调节喇叭音量就设置下当前音量等级
	/* 降噪相关 */
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_NS_LEVEL, temp_aec_info->aec_config->ns_level);							//建议取值范围1~8；值越小底噪越小
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_NS_PARA, temp_aec_info->aec_config->ns_para);							//只能取值0,1,2; 降噪由弱到强，建议默认值
	/* drc(输出音量相关) */
	aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_DRC, temp_aec_info->aec_config->drc);									//建议取值范围0x10~0x1f;   越大输出声音越大

#if CONFIG_AEC_VERSION_V3
#define AEC_NS_FILTER 1
     aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_DELAY_BUFF, (uint32_t)temp_aec_info->aec->Abuf);

#if (AEC_NS_FILTER)
if(temp_aec_info->aec_config->init_flags & AEC_NS_FLAG_MSK)
{
        const uint32_t ex_size=93380;
        uint8_t * gtbuff = (uint8_t*)psram_malloc(ex_size);
        memset(gtbuff, 0 , ex_size);
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_GTBUFF, (uint32_t)gtbuff);
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_GTPROC, (uint32_t)gtcrn_proc);
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_GTTEMP, (uint32_t)temp_aec_info->out_addr);
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_EC_FILTER, 0x03);
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_NS_FILTER, 0x80);
}	
else
{
        aec_ctrl(temp_aec_info->aec, AEC_CTRL_CMD_SET_NS_FILTER, 0x0);
}

	{
	    temp_aec_info->aec->SPthr[1] = 20;//(aec_vad_get_start_threshold()/20)-4;//1:20ms    default:20
	    temp_aec_info->aec->SPthr[2] = 50;//(aec_vad_get_stop_threshold()/20)+2;//20ms     default:50
	    //silence
	    temp_aec_info->aec->SPthr[6] = 160;//aec_vad_get_silence_threshold();
	    temp_aec_info->aec->SPthr[5] = 10;//16 - aec_vad_get_silence_threshold()/24;
		if(temp_aec_info->aec->SPthr[5] < 0)    
		{    temp_aec_info->aec->SPthr[5] = 0;    }
		if(temp_aec_info->aec->SPthr[5] > 16)     
		{    temp_aec_info->aec->SPthr[5] = 16;    }
	}

#endif
#endif
	LOGI("aec config:0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\r\n",
		temp_aec_info->aec_config->init_flags, //0x1f
		temp_aec_info->aec_config->mic_delay,//0x0
		temp_aec_info->aec_config->ec_depth,//0x14
		temp_aec_info->aec_config->ref_scale,//0x0
		temp_aec_info->aec_config->voice_vol,//0xe
		temp_aec_info->aec_config->ns_level,//0x2
		temp_aec_info->aec_config->ns_para,//0x1
		temp_aec_info->aec_config->drc//0xf
		 );
	return BK_OK;
}

/* NOTE voc init 中函数 */
static bk_err_t aud_tras_drv_aec_buff_cfg(aec_info_t *aec_info)
{
	uint16_t samp_rate_points = aec_info->samp_rate_points;

	/* malloc aec ref ring buffer to save ref data */
	LOGI("%s, %d, ref_ring_buff size: %d \n", __func__, __LINE__, samp_rate_points*2*2);
	aec_info->aec_ref_ring_buff = (int16_t *)audio_tras_drv_malloc(samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aec_info->aec_ref_ring_buff == NULL) {
		LOGE("%s, %d, malloc ref ring buffer fail \n", __func__, __LINE__);
		goto aud_tras_drv_aec_buff_cfg_exit;
	}

	/* malloc aec out ring buffer to save mic data has been aec processed */
	aec_info->aec_out_ring_buff = (int16_t *)audio_tras_drv_malloc(samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aec_info->aec_out_ring_buff == NULL) {
		LOGE("%s, %d, malloc aec out ring buffer fail \n", __func__, __LINE__);
		goto aud_tras_drv_aec_buff_cfg_exit;
	}

	/* init ref_ring_buff */
	ring_buffer_init(&(aec_info->ref_rb), (uint8_t*)aec_info->aec_ref_ring_buff, samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

	/* init aec_ring_buff */
	ring_buffer_init(&(aec_info->aec_rb), (uint8_t*)aec_info->aec_out_ring_buff, samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

	return BK_OK;

aud_tras_drv_aec_buff_cfg_exit:
	if (aec_info->aec_ref_ring_buff != NULL) {
		audio_tras_drv_free(aec_info->aec_ref_ring_buff);
		aec_info->aec_ref_ring_buff = NULL;
	}
	if (aec_info->aec_out_ring_buff != NULL) {
		audio_tras_drv_free(aec_info->aec_out_ring_buff);
		aec_info->aec_out_ring_buff = NULL;
	}
	return BK_FAIL;
}

static void aud_tras_drv_aec_decfg(void)
{
	if (aud_tras_drv_info.voc_info.aec_enable) {
		if (aud_tras_drv_info.voc_info.aec_info->aec) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.aec_info->aec);
			aud_tras_drv_info.voc_info.aec_info->aec = NULL;
		}

		if (aud_tras_drv_info.voc_info.aec_info->aec_config) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.aec_info->aec_config);
			aud_tras_drv_info.voc_info.aec_info->aec_config = NULL;
		}

		ring_buffer_clear(&(aud_tras_drv_info.voc_info.aec_info->ref_rb));
		ring_buffer_clear(&(aud_tras_drv_info.voc_info.aec_info->aec_rb));

		if (aud_tras_drv_info.voc_info.aec_info->aec_ref_ring_buff) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.aec_info->aec_ref_ring_buff);
			aud_tras_drv_info.voc_info.aec_info->aec_ref_ring_buff = NULL;
		}

		if (aud_tras_drv_info.voc_info.aec_info->aec_out_ring_buff) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.aec_info->aec_out_ring_buff);
			aud_tras_drv_info.voc_info.aec_info->aec_out_ring_buff = NULL;
		}

		if (aud_tras_drv_info.voc_info.aec_info) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.aec_info);
			aud_tras_drv_info.voc_info.aec_info = NULL;
		}
	} else {
		aud_tras_drv_info.voc_info.aec_info = NULL;
	}

	aud_tras_drv_info.voc_info.aec_enable = false;
}

/* 搬运audio adc 采集到的一帧mic和ref信号后，触发中断通知AEC处理数据 */
static void aud_tras_adc_dma_finish_isr(void)
{
	bk_err_t ret = BK_OK;
    AUD_ADC_DMA_ISR_START();

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_TX_DATA, NULL);
	} else {
		/* send msg to AEC or ENCODER to process mic data */
		if (aud_tras_drv_info.voc_info.aec_enable) // NOTE 传给 ASR 
			ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_AEC, NULL);
		else { /* 通知编码 */
			ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
		}
	}
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_AEC fail \n", __func__, __LINE__);
	}

    AUD_ADC_DMA_ISR_END();
}
// NOTE
static bk_err_t aud_tras_adc_dma_config(dma_id_t dma_id, int32_t *ring_buff_addr, uint32_t ring_buff_size, uint32_t transfer_len, aud_intf_mic_chl_t mic_chl)
{
	bk_err_t ret = BK_OK;
	dma_config_t dma_config = {0};
	uint32_t adc_port_addr;

    os_memset(&dma_config, 0, sizeof(dma_config));

	dma_config.mode = DMA_WORK_MODE_REPEAT;
	dma_config.chan_prio = 1;
	dma_config.src.dev = DMA_DEV_AUDIO_RX;
	dma_config.dst.dev = DMA_DEV_DTCM;  /* DTCM（Data Tightly Coupled Memory） */
	switch (mic_chl) {
		case AUD_INTF_MIC_CHL_MIC1:
			dma_config.src.width = DMA_DATA_WIDTH_16BITS;
			break;

		case AUD_INTF_MIC_CHL_DUAL:
			dma_config.src.width = DMA_DATA_WIDTH_32BITS;
			break;

		default:
			break;
	}
	/* BUG 这里为什么还需要强制配置呢？ */
	dma_config.dst.width = DMA_DATA_WIDTH_32BITS;

	/* get adc fifo address */
	if (bk_aud_adc_get_fifo_addr(&adc_port_addr) != BK_OK) {
		LOGE("%s, %d, get adc fifo address fail \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_ADC;
	} else {
		dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
		dma_config.src.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
		dma_config.src.start_addr = adc_port_addr;
		dma_config.src.end_addr = adc_port_addr + 4;
	}

	/* NOTE 配置 dma 目的 地址自动递增、循环、起始地址、终止地址 */
	dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;
	dma_config.dst.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
	dma_config.dst.start_addr = (uint32_t)ring_buff_addr;
	dma_config.dst.end_addr = (uint32_t)ring_buff_addr + ring_buff_size;

	/* init dma channel */
	ret = bk_dma_init(dma_id, &dma_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, audio adc dma channel init fail \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_DMA;
	}

	/* set dma transfer length */
	bk_dma_set_transfer_len(dma_id, transfer_len);

	//register isr NOTE
	bk_dma_register_isr(dma_id, NULL, (void *)aud_tras_adc_dma_finish_isr);

	bk_dma_enable_finish_interrupt(dma_id);
#if (CONFIG_SPE)
	BK_LOG_ON_ERR(bk_dma_set_dest_sec_attr(dma_id, DMA_ATTR_SEC));
	BK_LOG_ON_ERR(bk_dma_set_src_sec_attr(dma_id, DMA_ATTR_SEC));
#endif

	return BK_ERR_AUD_INTF_OK;
}

/* 搬运audio dac 一帧dac信号后，触发中断通知 decoder 处理数据 */
static void aud_tras_dac_dma_finish_isr(void)
{
	bk_err_t ret = BK_OK;
    AUD_DAC_DMA_ISR_START();
    // bk_printf("speak dma dac isr\r\n");

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL)
		/* send msg to notify app to write speaker data */
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_REQ_DATA, NULL);
	else
		/* send msg to decoder to decoding recevied data  还在交互中，通知接着解压 */
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_DECODER, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, dac send msg: AUD_TRAS_DRV_DECODER fail \n", __func__, __LINE__);
	}

    AUD_DAC_DMA_ISR_END();
}

static bk_err_t aud_tras_dac_dma_config(dma_id_t dma_id, int32_t *ring_buff_addr, uint32_t ring_buff_size, uint32_t transfer_len, aud_intf_spk_chl_t spk_chl)
{
	bk_err_t ret = BK_OK;
	dma_config_t dma_config = {0};
	uint32_t dac_port_addr;

    os_memset(&dma_config, 0, sizeof(dma_config));

	dma_config.mode = DMA_WORK_MODE_REPEAT;
	dma_config.chan_prio = 1;
	dma_config.src.dev = DMA_DEV_DTCM;
	dma_config.dst.dev = DMA_DEV_AUDIO;

	dma_config.src.width = DMA_DATA_WIDTH_32BITS;
	switch (spk_chl) {
		case AUD_INTF_SPK_CHL_LEFT:
			dma_config.dst.width = DMA_DATA_WIDTH_16BITS;
			break;

		case AUD_INTF_SPK_CHL_DUAL:
			dma_config.dst.width = DMA_DATA_WIDTH_32BITS;
			break;

		default:
			break;
	}

	/* get dac fifo address */
	if (bk_aud_dac_get_fifo_addr(&dac_port_addr) != BK_OK) {
		LOGE("%s, %d, get dac fifo address fail \n", __func__, __LINE__);
		return BK_FAIL;
	} else {
		dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;
		dma_config.dst.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
		dma_config.dst.start_addr = dac_port_addr;
		dma_config.dst.end_addr = dac_port_addr + 4;
	}
	dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
	dma_config.src.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
	dma_config.src.start_addr = (uint32_t)ring_buff_addr;
	dma_config.src.end_addr = (uint32_t)(ring_buff_addr) + ring_buff_size;

	/* init dma channel */
	ret = bk_dma_init(dma_id, &dma_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, audio dac dma channel init fail \n", __func__, __LINE__);
		return BK_FAIL;
	}

	/* set dma transfer length */
	bk_dma_set_transfer_len(dma_id, transfer_len);

	//register isr
	/* NOTE 注册 dma 完成一般/全部 触发的 中断函数 */
	bk_dma_register_isr(dma_id, NULL, (void *)aud_tras_dac_dma_finish_isr);
	bk_dma_enable_finish_interrupt(dma_id);

#if (CONFIG_SPE)
	BK_LOG_ON_ERR(bk_dma_set_dest_sec_attr(dma_id, DMA_ATTR_SEC));
	BK_LOG_ON_ERR(bk_dma_set_src_sec_attr(dma_id, DMA_ATTR_SEC));
#endif

	return BK_OK;
}

#if CONFIG_AEC_VERSION_V3
static void aud_aec_vad_process(void)
{
#if CONFIG_SYS_CPU1
    aec_info_t *aec_info_pr = aud_tras_drv_info.voc_info.aec_info;
    //extern int aec_vad_record_flag_get(void);
    // extern void aec_vad_status_send(int val);
	
    //if (!!aec_vad_record_flag_get())
    //if(aec_info_pr->aec->test)
    //if(!aud_get_production_mode())
    {
        static int aec_vad_flag=0;
        //static int aec_vad_flag_mem=0;
        static int aec_vad_mem=0;
		       
        static int badframe = 0;
        int dc = aec_info_pr->aec->dc >> 14;
        if (dc<0)
		{
			dc = -dc;
		}
        if ( (dc>800) && (aec_info_pr->aec->mic_max>10000) )
        {
            badframe = 10;
        }
        else
        {
            badframe--;
            if (badframe < 0)
            {
                badframe = 0;
			}
        }
        if (badframe)
        {
            aec_info_pr->aec->spcnt >>= 1;
        }
				
        if(aec_info_pr->aec->test)
        {
            if(aec_vad_mem==0)
            {
                aec_vad_flag = 1;
            }
            else
            {
                if(aec_info_pr->aec->dcnt*20 == aec_info_pr->aec->SPthr[6])
    			{
    				aec_vad_flag = 3;
    			}
    			else
    			{
    				aec_vad_flag = 0;
    			}
            }
            aec_vad_mem = aec_info_pr->aec->test;
        }
        else
        {
            if(0 == aec_vad_mem)
            {
                aec_vad_flag = 0;
            }
            if(aec_vad_mem > 0)
            {
                aec_vad_flag = 2;  // vad end
            }
            aec_vad_mem = aec_info_pr->aec->test;
        }

        bk_printf("aec_vad_flag is %d\r\n", aec_vad_flag);
        
        //if(aec_vad_flag-aec_vad_flag_mem)
        //{
        //  bk_printf("a.v.flag:%d,%d\n",aec_vad_flag,aec_vad_flag_mem);
        //}
        //aec_vad_flag_mem = aec_vad_flag;
        
       // aec_vad_status_send(aec_vad_flag);
	   __maybe_unused_var(aec_vad_flag);
    }
#endif
}
#endif


static uint32_t audio_silence_frame_cnt = 0;

#define SILENCE_FRAME_THR 50*5

uint8_t check_rx_spk_data_silence(int16_t *data, uint16_t size) {
    uint8_t is_silence = 1;
    for (uint16_t i = 0; i < size; i++) {
	if ((data[i] > 64)||(data[i] < -64)) {
		is_silence = 0;
		break;
	}

    }

    if (is_silence) {
        audio_silence_frame_cnt++;
    } else {
		audio_silence_frame_cnt = 0;
	}

	if(audio_silence_frame_cnt >= SILENCE_FRAME_THR)
	{
		audio_silence_frame_cnt = SILENCE_FRAME_THR;
		return 1;
	}
	else
	{
		return 0;
	}
}

#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
int16_t temp_buf[640] = {0};
//int16_t temp_ref_buf[640] = {0};
#endif

static bk_err_t aud_tras_aec(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL) /* voice is not init,then returns */
		return BK_OK;

    AUD_AEC_PROCESS_START();

	aec_info_t *aec_info_pr = aud_tras_drv_info.voc_info.aec_info;

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE // XXX this
	if (ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.mic_rb)) >= aec_info_pr->samp_rate_points*4) { /*NOTE 双路音频数据 一个16bit*/
		/* NOTE 读取 麦克风数据 存入 mic_addr */
		// bk_printf("read mic data 1\r\n");  /* NOTE 循环执行 */
		size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)temp_buf, aec_info_pr->samp_rate_points*4);
		if (size != aec_info_pr->samp_rate_points*4) {
			LOGE("%s, %d, read mic_ring_buff fail, size:%d \n", __func__, __LINE__, size);
			//return BK_FAIL;
		}

		/* mic data 是 采用的 远近交替采集 */
		for(uint32_t i = 0; i < 320; i++)
		{
			aec_info_pr->mic_addr[i] = temp_buf[2 *i]; /*near-end data*/  /* 这里获取的 */
			aec_info_pr->ref_addr[i] = temp_buf[2*i+1]; /*remote data*/
		}
	}
#else
	/* get a fram mic data from mic_ring_buff */
	if (ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.mic_rb)) >= aec_info_pr->samp_rate_points*2) {
		// bk_printf("read mic data 2"); // 无
		size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, read mic_ring_buff fail, size:%d \n", __func__, __LINE__, size);
			//return BK_FAIL;
		}
	}
#endif
    else
    {
		LOGD("%s, %d, do not have mic data need to aec \n", __func__, __LINE__);
		return BK_OK;
	}

	if (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_CLOSE || aud_tras_drv_info.voc_info.aec_enable == false) {
		/* save mic data after aec processed to aec_ring_buffer */
		/* NOTE 支持 AEC 就是从 mic_rb 中读取 然后放入到 aec_info 中的 */
		if (ring_buffer_get_free_size(&(aec_info_pr->aec_rb)) >= aec_info_pr->samp_rate_points*2) {
			size = ring_buffer_write(&(aec_info_pr->aec_rb), (uint8_t*)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
			if (size != aec_info_pr->samp_rate_points*2) {
				LOGE("%s, %d, the data write to aec_ring_buff is not a frame \n", __func__, __LINE__);
				//return BK_FAIL;
			}
		}

		/* send msg to encoder to encoding data */
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
		if (ret != kNoErr) {
			LOGE("%s, %d, send msg: AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
			return BK_FAIL;
		}

		return BK_OK;
	}

#if CONFIG_AEC_ECHO_COLLECT_MODE_SOFTWARE
	/* read ref data from ref_ring_buff */
	if (ring_buffer_get_fill_size(&(aec_info_pr->ref_rb)) >= aec_info_pr->samp_rate_points*2) {
		size = ring_buffer_read(&(aec_info_pr->ref_rb), (uint8_t*)aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, the ref data readed from ref_ring_buff is not a frame \n", __func__, __LINE__);
			//return BK_FAIL;
			//os_memset(ref_addr, 0, frame_sample*2);
		}
	} else {
		//LOGE("no ref data \n");
		os_memset((void *)aec_info_pr->ref_addr, 0, aec_info_pr->samp_rate_points*2);
	}
	//os_printf("ref_addr: ref_addr[0]= %02x, ref_addr[0]= %02x \r\n", ref_addr[0], ref_addr[1]);
#endif

	/* NOTE 追踪得知这个 -> aud_tras_drv_voc_aec_debug 函数注册的 ，但是似乎没有注册函数*/
	if (aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb) { // debug
		aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb((uint8_t *)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
		aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb((uint8_t *)aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
	}
    #if CONFIG_DEBUG_DUMP
    if(aec_all_data_flag)
    {
        #if 0
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_NUM(DUMP_TYPE_AEC_MIC_DATA,3);//mic/ref/aec out

        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_AEC_MIC_DATA,0,DUMP_FILE_TYPE_PCM,aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_AEC_REF_DATA,1,DUMP_FILE_TYPE_PCM,aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW(DUMP_TYPE_AEC_OUT_DATA,2,DUMP_FILE_TYPE_PCM,aec_info_pr->samp_rate_points*2);
        #else
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_AEC_MIC_DATA,0,aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_AEC_REF_DATA,1,aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_AEC_OUT_DATA,2,aec_info_pr->samp_rate_points*2);
        #endif

        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_AEC_MIC_DATA);
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_AEC_MIC_DATA);
        //AEC_DATA_DUMP_BY_UART_DATA((void *)&dump_header,sizeof(dump_header));
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_AEC_MIC_DATA);

        //AEC_DATA_DUMP_BY_UART_DATA(aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
        //AEC_DATA_DUMP_BY_UART_DATA(aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_BY_UART_DATA(aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_BY_UART_DATA(aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
    }
    #endif



#if CONFIG_AUD_TRAS_AEC_DUMP_DEBUG // no
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_TF
	os_memcpy((void *)aud_tras_drv_info.voc_info.aec_dump.mic_dump_addr, aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
	os_memcpy((void *)aud_tras_drv_info.voc_info.aec_dump.ref_dump_addr, aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
	//os_printf("memcopy complete \r\n");
#endif //CONFIG_AUD_TRAS_AEC_DUMP_MODE_TF
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_UART 
	bk_uart_write_bytes(CONFIG_AUD_TRAS_AEC_DUMP_UART_ID, aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
	bk_uart_write_bytes(CONFIG_AUD_TRAS_AEC_DUMP_UART_ID, aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
#endif
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_UDP 
	size = bk_aud_debug_voc_udp_send_packet((unsigned char *)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, udp dump mic fail, all:%d, complete:%d \n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
	size = bk_aud_debug_voc_udp_send_packet((unsigned char *)aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, udp dump ref fail, all:%d, complete:%d \n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
#endif
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_TCP 
	size = bk_aud_debug_voc_tcp_send_packet((unsigned char *)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, tcp dump mic fail, all:%d, complete:%d \n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
	size = bk_aud_debug_voc_tcp_send_packet((unsigned char *)aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, tcp dump ref fail, all:%d, complete:%d \n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
#endif
#endif //CONFIG_AUD_TRAS_AEC_DUMP_DEBUG



#if CONFIG_AEC_VERSION_V3
    aec_info_pr->aec->flags = 0x1f;
#elif CONFIG_AEC_VERSION_V2
    aec_info_pr->aec->flags = 0x1d;
#else

#endif
    /* Return 1:判断为远端(喇叭)为静音数据  */
	spk_play_flag = check_rx_spk_data_silence(aec_info_pr->ref_addr, 32);
	/* aec process data */
	//os_printf("ref_addr:%p, mic_addr:%p, out_addr:%p \r\n", aec_context_pr->ref_addr, aec_context_pr->mic_addr, aec_context_pr->out_addr);
	/* NOTE 获取 最终的音频数据 (由 mic_addr 得到  out_addr)  */
	aec_proc(aec_info_pr->aec, aec_info_pr->ref_addr, aec_info_pr->mic_addr, aec_info_pr->out_addr);
#if CONFIG_AEC_VERSION_V3 // CONFIG_AEC_VERSION_V2
	aud_aec_vad_process(); 
#endif

	/* NOTE 追踪得知这个 -> aud_tras_drv_voc_aec_debug 函数注册的 ，但是似乎没有注册函数*/
	if (aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb) {
		aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb((uint8_t *)aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
	}

#if CONFIG_AI_ASR_MODE_CPU2
    gl_asr_data.data = (unsigned char *)aec_info_pr->out_addr;
    gl_asr_data.size = (unsigned int)aec_info_pr->samp_rate_points*2; /* NOTE *2 reason => 16bit数据 => 2个字节 */
	gl_asr_data.spk_play_flag = spk_play_flag;	
    gl_asr_data_msg.event = EVENT_ASR_DATA_NOTIFY;
    gl_asr_data_msg.param = (uint32_t)&gl_asr_data;
    /* TODO 将录制的音频aec后发送给 ASR 识别 */
    msg_send_notify_to_media_major_mailbox(&gl_asr_data_msg, MINOR_MODULE);
#endif

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
    /* check and send aec output data to other cpu1 task */
    /* NOTE 这里函数有一个 CPU1 处的假注册（ amino_asr.c 文件中 ） ，这里是运行在 CPU2 上的 */
    if (gl_aec_output_callback) // aec_output_callback
    {
        /* gl_aec_output_callback can not block task */
        int out_size = gl_aec_output_callback((unsigned char *)aec_info_pr->out_addr, (unsigned int)aec_info_pr->samp_rate_points*2, gl_user_data);
        if (out_size != aec_info_pr->samp_rate_points*2)
        {
            LOGE("%s, %d, aec output size: %d != %d\n", __func__, __LINE__, out_size, aec_info_pr->samp_rate_points*2);
        }
    } else {
    	// bk_printf("gl_aec_output_callback is not register\r\n");
    }
#endif
    #if CONFIG_DEBUG_DUMP
    if(aec_all_data_flag)
    {
        //AEC_DATA_DUMP_BY_UART_DATA(aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
        DEBUG_DATA_DUMP_BY_UART_DATA(aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
    }
    #endif

#if CONFIG_AUD_TRAS_AEC_DUMP_DEBUG
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_TF
	os_memcpy((void *)aud_tras_drv_info.voc_info.aec_dump.out_dump_addr, aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
#endif //CONFIG_AUD_TRAS_AEC_DUMP_MODE_TF

#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_UART
	bk_uart_write_bytes(CONFIG_AUD_TRAS_AEC_DUMP_UART_ID, aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
#endif

#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_UDP
	size = bk_aud_debug_voc_udp_send_packet((unsigned char *)aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, udp dump aec out fail, all:%d, complete:%d \r\n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
#endif
#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_TCP
	size = bk_aud_debug_voc_tcp_send_packet((unsigned char *)aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
	if (size != aec_info_pr->samp_rate_points*2)
		os_printf("%s, %d, tcp dump aec out fail, all:%d, complete:%d \r\n", __func__, __LINE__, aec_info_pr->samp_rate_points*2, size);
#endif
#endif //CONFIG_AUD_TRAS_AEC_DUMP_DEBUG

	/* save mic data after aec processed to aec_ring_buffer */
	if (ring_buffer_get_free_size(&(aec_info_pr->aec_rb)) >= aec_info_pr->samp_rate_points*2) {
		size = ring_buffer_write(&(aec_info_pr->aec_rb), (uint8_t*)aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to aec_ring_buff is not a frame \n", __func__, __LINE__);
			//return BK_FAIL;
		}
	}

	/* send msg to encoder to encoding data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
		return BK_FAIL;
	}

    AUD_AEC_PROCESS_END();

	return ret;
}

static audio_packet_t *aud_uac_malloc_packet(uint32_t dev)
{
	audio_packet_t *packet = NULL;

	switch(dev)
	{
		case USB_UAC_SPEAKER_DEVICE:
			aud_tras_drv_info.spk_info.uac_spk_packet.dev = USB_UAC_MIC_DEVICE;
			aud_tras_drv_info.spk_info.uac_spk_packet.data_buffer = NULL;
			aud_tras_drv_info.spk_info.uac_spk_packet.num_packets = 2;
			aud_tras_drv_info.spk_info.uac_spk_packet.data_buffer_size = aud_tras_drv_info.spk_info.frame_size * 2;
			aud_tras_drv_info.spk_info.uac_spk_packet.state = NULL;
			aud_tras_drv_info.spk_info.uac_spk_packet.num_byte = NULL;
			aud_tras_drv_info.spk_info.uac_spk_packet.actual_num_byte = NULL;
			packet = &aud_tras_drv_info.spk_info.uac_spk_packet;
			break;
		case USB_UAC_MIC_DEVICE:
			aud_tras_drv_info.mic_info.uac_mic_packet.dev = USB_UAC_MIC_DEVICE;
			aud_tras_drv_info.mic_info.uac_mic_packet.data_buffer = NULL;
			aud_tras_drv_info.mic_info.uac_mic_packet.num_packets = 2;
			aud_tras_drv_info.mic_info.uac_mic_packet.data_buffer_size = aud_tras_drv_info.mic_info.frame_size * 2;
			aud_tras_drv_info.mic_info.uac_mic_packet.state = NULL;
			aud_tras_drv_info.mic_info.uac_mic_packet.num_byte = NULL;
			aud_tras_drv_info.mic_info.uac_mic_packet.actual_num_byte = NULL;
			packet = &aud_tras_drv_info.mic_info.uac_mic_packet;
			break;
		default:
			break;
	}

	return packet;
}

/* NOTE 麦克风数据 写入 环形缓冲区 */
static void aud_uac_push_packet(audio_packet_t *packet)
{
	bk_err_t ret = BK_OK;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
    uac_mic_spk_count_add_mic_size(packet->data_buffer_size);
#endif

	if (aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_START && packet->data_buffer_size > 0) {
		if (ring_buffer_get_free_size(&aud_tras_drv_info.mic_info.mic_rb) >= packet->data_buffer_size) {
			// bk_printf("write mic data 1\r\n"); // 没运行
			ring_buffer_write(&aud_tras_drv_info.mic_info.mic_rb, (uint8_t *)packet->data_buffer, packet->data_buffer_size);
		}
	}

	/* send msg to TX_DATA to process mic data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_TX_DATA, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_MIC_TX_DATA fail \n", __func__, __LINE__);
	}

	return;
}

static void aud_uac_free_packet(audio_packet_t *packet)
{
	bk_err_t ret = BK_OK;

	if (packet->data_buffer_size == aud_tras_drv_info.spk_info.frame_size) {
		os_memcpy(packet->data_buffer, aud_tras_drv_info.spk_info.uac_spk_buff, packet->data_buffer_size);
		os_memset(aud_tras_drv_info.spk_info.uac_spk_buff, 0x00, aud_tras_drv_info.spk_info.frame_size);
	}

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
    uac_mic_spk_count_add_spk_size(packet->data_buffer_size);
#endif

	/* send msg to notify app to write speaker data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_REQ_DATA, (void *)packet);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_SPK_REQ_DATA fail \n", __func__, __LINE__);
	}

	return;
}

static const struct audio_packet_control_t aud_uac_buffer_ops_funcs = {
    ._uac_malloc = aud_uac_malloc_packet,
    ._uac_push = aud_uac_push_packet,
    ._uac_pop = NULL,
    ._uac_free = aud_uac_free_packet,
};

static audio_packet_t *voc_uac_malloc_packet(uint32_t dev)
{
	audio_packet_t *packet = NULL;

	switch(dev)
	{
		case USB_UAC_SPEAKER_DEVICE:
			aud_tras_drv_info.voc_info.uac_spk_packet.dev = USB_UAC_MIC_DEVICE;
			aud_tras_drv_info.voc_info.uac_spk_packet.data_buffer = NULL;
			aud_tras_drv_info.voc_info.uac_spk_packet.num_packets = 2;
			aud_tras_drv_info.voc_info.uac_spk_packet.data_buffer_size = aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2 * 2;
			aud_tras_drv_info.voc_info.uac_spk_packet.state = NULL;
			aud_tras_drv_info.voc_info.uac_spk_packet.num_byte = NULL;
			aud_tras_drv_info.voc_info.uac_spk_packet.actual_num_byte = NULL;
			packet = &aud_tras_drv_info.voc_info.uac_spk_packet;
			break;
		case USB_UAC_MIC_DEVICE:
			aud_tras_drv_info.voc_info.uac_mic_packet.dev = USB_UAC_MIC_DEVICE;
			aud_tras_drv_info.voc_info.uac_mic_packet.data_buffer = NULL;
			aud_tras_drv_info.voc_info.uac_mic_packet.num_packets = 2;
			aud_tras_drv_info.voc_info.uac_mic_packet.data_buffer_size = aud_tras_drv_info.voc_info.mic_samp_rate_points * 2 * 2;
			aud_tras_drv_info.voc_info.uac_mic_packet.state = NULL;
			aud_tras_drv_info.voc_info.uac_mic_packet.num_byte = NULL;
			aud_tras_drv_info.voc_info.uac_mic_packet.actual_num_byte = NULL;
			packet = &aud_tras_drv_info.voc_info.uac_mic_packet;
			break;
		default:
			break;
	}

	return packet;
}

static void voc_uac_push_packet(audio_packet_t *packet)
{
//	LOGI("%s size %x\r\n",__func__, packet->data_buffer_size);
	bk_err_t ret = BK_OK;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	uac_mic_spk_count_add_mic_size(packet->data_buffer_size);
#endif

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START && packet->data_buffer_size > 0) {
		if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.mic_rb) >= packet->data_buffer_size) {
			// bk_printf("write mic data 2\r\n"); // 没运行
			ring_buffer_write(&aud_tras_drv_info.voc_info.mic_rb, (uint8_t *)packet->data_buffer, packet->data_buffer_size);
		}
	}

	/* send msg to TX_DATA to process mic data */
	if (aud_tras_drv_info.voc_info.aec_enable) {
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_AEC, NULL);
		//LOGI("%s AUD_TRAS_DRV_AEC\r\n",__func__);
	} else {
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
		//LOGI("%s AUD_TRAS_DRV_ENCODER\r\n",__func__);
	}
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
	}

	return;
}

static void voc_uac_free_packet(audio_packet_t *packet)  /* NOTE 看其函数名称 其中有 uac 的不是 */
{
	bk_err_t ret = BK_OK;
//	LOGI("%s size %x\r\n",__func__, packet->data_buffer_size);

	/* check status and size */
	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START && aud_tras_drv_info.voc_info.speaker_samp_rate_points*2 == packet->data_buffer_size) {
		os_memcpy(packet->data_buffer, aud_tras_drv_info.voc_info.uac_spk_buff, packet->data_buffer_size);

#if 0
		bk_uart_write_bytes(UART_ID_2, packet->data_buffer, packet->data_buffer_size);
#endif

		os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
		uac_mic_spk_count_add_spk_size(packet->data_buffer_size);
#endif
	}

	/* send msg to notify app to write speaker data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_DECODER, (void *)packet);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_DECODER fails \n", __func__, __LINE__);
	}

//	memset(packet->data_buffer, 0x11, packet->data_buffer_size);
	return;
}

static const struct audio_packet_control_t voc_uac_buffer_ops_funcs = {
    ._uac_malloc = voc_uac_malloc_packet,
    ._uac_push = voc_uac_push_packet, // this?
    ._uac_pop = NULL,
    ._uac_free = voc_uac_free_packet,
};

static bk_err_t aud_tras_uac_auto_connect_ctrl(bool en)
{
	aud_tras_drv_info.uac_auto_connect = en;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_uac_disconnect_handle(void)
{
//	LOGI("enter %s \n", __func__);

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist && aud_tras_drv_info.uac_status == AUD_INTF_UAC_ABNORMAL_DISCONNECTED) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_ABNORMAL_DISCONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* reset mic and spk current status */
	aud_tras_drv_info.uac_mic_open_current = false;
	aud_tras_drv_info.uac_spk_open_current = false;

	return BK_OK;
}

static void aud_tras_uac_disconnect_cb(void)
{
	uint8_t count = 6;
	bk_err_t ret = BK_OK;

	LOGI("%s \n", __func__);

	if (aud_tras_drv_info.uac_status != AUD_INTF_UAC_NORMAL_DISCONNECTED) {
		aud_tras_drv_info.uac_status = AUD_INTF_UAC_ABNORMAL_DISCONNECTED;
		do {
			if (count == 0)
				break;
			count--;
			ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_DISCONT, NULL);
			if (ret != BK_OK) {
				LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_DISCONT fail: %d \n", __func__, __LINE__, count);
				rtos_delay_milliseconds(20);
			}
		} while (ret != BK_OK);
	}
}

static bk_err_t aud_tras_uac_connect_handle(void)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_OK;

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_CONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* check Automatic recover uac connect */
	if (aud_tras_drv_info.uac_status == AUD_INTF_UAC_ABNORMAL_DISCONNECTED) {
		/* uac automatically connect */
		if (!aud_tras_drv_info.uac_auto_connect) {
			LOGI("%s, %d, uac not automatically connect, need user todo \n", __func__, __LINE__);
			return BK_OK;
		}
	}

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL && aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
		return BK_OK;
	}

	/* config uac */
	LOGI("%s, %d, config uac \n", __func__, __LINE__);
	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
		if (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_NULL && aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
			ret = bk_aud_uac_mic_set_param(aud_tras_drv_info.mic_info.uac_mic_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, uac set mic param fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto fail;
			}
		}

		if (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_NULL && aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
			ret = bk_aud_uac_spk_set_param(aud_tras_drv_info.spk_info.uac_spk_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, uac set spk param fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto fail;
			}
		}
	} else {
		ret = bk_aud_uac_set_param(aud_tras_drv_info.voc_info.uac_config);
		if (ret != BK_OK) {
			LOGE("%s, %d, uac set param fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto fail;
		}
	}

	LOGI("%s, %d, voc_ops: %p, malloc: %p, push: %p, free: %p \n", __func__, __LINE__, &voc_uac_buffer_ops_funcs, voc_uac_buffer_ops_funcs._uac_malloc, voc_uac_buffer_ops_funcs._uac_push, voc_uac_buffer_ops_funcs._uac_free);

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
		/*  */
		ret = bk_aud_uac_register_transfer_buffer_ops((void *)&aud_uac_buffer_ops_funcs);
	} else {
		ret = bk_aud_uac_register_transfer_buffer_ops((void *)&voc_uac_buffer_ops_funcs);
	}
	if (ret != BK_OK) {
		LOGE("%s, %d, register transfer buffer ops fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_UAC_DRV;
		goto fail;
	}

	if (aud_tras_drv_info.uac_mic_open_status == true && aud_tras_drv_info.uac_mic_open_current == false) {
		ret = bk_aud_uac_start_mic();
		if (ret != BK_OK) {
			LOGE("%s, %d, start uac mic fail, ret:%d \n", __func__, __LINE__, ret);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		} else {
			aud_tras_drv_info.uac_mic_open_current = true;
		}
	}

	if (aud_tras_drv_info.uac_spk_open_status == true && aud_tras_drv_info.uac_spk_open_current == false) {
		ret = bk_aud_uac_start_spk();
		if (ret != BK_OK) {
			LOGE("%s, %d, start uac spk fail, ret:%d \n", __func__, __LINE__, ret);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto fail;
		} else {
			aud_tras_drv_info.uac_spk_open_current = true;
		}
	}

	return ret;

fail:

	return err;
}


static void aud_tras_uac_connect_cb(void)
{
	uint8_t count = 6;
	bk_err_t ret = BK_OK;

	LOGI("%s \n", __func__);

	if (aud_tras_drv_info.uac_status != AUD_INTF_UAC_CONNECTED) {
		aud_tras_drv_info.uac_status = AUD_INTF_UAC_CONNECTED;
		do {
			if (count == 0)
				break;
			count--;
			ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_CONT, NULL);
			if (ret != BK_OK) {
				LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_CONT fail: %d \n", __func__, __LINE__, count);
				rtos_delay_milliseconds(20);
			}
		} while (ret != BK_OK);
	}
}

#if I4S_USE_VAD_DEPLOY
#include "stdlib.h"

#if I4S_USE_VAD_LIB
#define VAD_ENERGY_THRESHOLD 500
#define VAD_ZCR_THRESHOLD 20
#define FRAME_LEN 160

#define VAD_KEEP_TIMES	(6)
static uint8_t keep_unit = 0;
typedef enum {
    VAD_SILENCE = 0,
    VAD_SPEECH,
    VAD_NOISE
} vad_result_t;

// 计算过零率
int calculate_zcr(int16_t *data, int len) {
    int zcr = 0;
    for (int i = 1; i < len; i++) {
        if ((data[i - 1] >= 0 && data[i] < 0) || (data[i - 1] < 0 && data[i] >= 0)) {
            zcr++;
        }
    }
    return zcr;
}

vad_result_t simple_vad_with_noise(int16_t *pcm_data, int len) {
    uint64_t energy = 0;
    for (int i = 0; i < len; i++) {
        energy += abs(pcm_data[i]);
    }
    uint32_t avg_energy = energy / len;
    int zcr = calculate_zcr(pcm_data, len);

    if (avg_energy < VAD_ENERGY_THRESHOLD) {
        return VAD_SILENCE;  // 静音
    } else {
        if (zcr < VAD_ZCR_THRESHOLD) {
            return VAD_SPEECH;  // 说话，低过零率且有能量
        } else {
            return VAD_NOISE;   // 吵杂声，高过零率且有能量
        }
    }
}
#endif // end I4S_USE_VAD_LIB
#endif

static bk_err_t aud_tras_enc(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;
	uint32_t i = 0;
	uint16_t temp_mic_samp_rate_points = aud_tras_drv_info.voc_info.mic_samp_rate_points;
	tx_info_t temp_tx_info;

//	uint32_t fill_size = 0;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
		return BK_OK;

    AUD_ENC_PROCESS_START();

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		if (aud_tras_drv_info.voc_info.aec_enable) {
			/* get data from aec_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.aec_info->aec_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);

			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read aec_rb :%d \n", __func__, __LINE__, size);
				os_memset(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, 0, temp_mic_samp_rate_points*2);
				//goto encoder_exit;
			} else {
#if I4S_USE_VAD_DEPLOY
#if I4S_USE_VAD_LIB
				/* NOTE 准度一般等级 */
				vad_result_t result = simple_vad_with_noise(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, FRAME_LEN);
				if (result == VAD_SPEECH) {
					keep_unit = VAD_KEEP_TIMES;
					// AUD_PLAY_PROMPT_TONE_START();
				}
				keep_unit = VAD_KEEP_TIMES;  // 想不使用VAD隔断就放出此行代码
				if (keep_unit > 0) { /* NOTE 说话中 将数据提取出来 */
					if (keep_unit >= 1) {
						keep_unit--;
					}
				    // bk_printf("talking...\r\n");
#if I4S_RECORD_REPEAT_MODE
					if (i4s_record_flag && !i4s_get_tone_state()) {
						// if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) >= temp_mic_samp_rate_points*2) {
						// 	ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
						// 	aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
						// }
						/* REVIEW */
						/* NOTE 注意这里默认传入、接收的是 PCM 格式的数据 其余格式需要改变存储缓冲区的数据 */
						/* NOTE 上面是将麦克风数据直接发送到喇叭 下面是直接麦克风数据发送到 i4s_aud_tx_rb 缓冲区努中 */
						if (aud_tras_drv_info.voc_info.i4s_aud_tx_rb) {
				    		int free_size = ring_buffer_get_free_size(aud_tras_drv_info.voc_info.i4s_aud_tx_rb);
				    		if (free_size > temp_mic_samp_rate_points*2) {
				    			/* REVIEW 这个 i4s_aud_tx_rb 才是 发送给远端服务器的 rb */
				    			/* NOTE  同指向 由 i4s_aud_tras_get_tx_rb() 改名而来 */
				    			if (ring_buffer_write(aud_tras_drv_info.voc_info.i4s_aud_tx_rb, (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2) == temp_mic_samp_rate_points*2) {
				    				aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
				    			}
				    		} else {
				    			//LOGE("i4s_aud_tx_rb free_size: %d \n", free_size);
				    		}
				    	}
						return BK_OK;
					}
#endif
#endif					
				} else if (result == VAD_NOISE || result == VAD_SILENCE) {
				    // bk_printf("noisy...\r\n");
				    // NOTE 可以让喇叭在播放提示音时很模糊
				    // uint8_t pcm_data_temp[320] = {0};
				    // memset(pcm_data_temp, 0x2, 320);
				    // if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) >= 320) {
					// 	ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data_temp, 320);
					// }
					return BK_OK;
				}             
#endif	
			}
		} else {
#if (CONFIG_CACHE_ENABLE)
			flush_all_dcache();
#endif
			/* get data from mic_ring_buff */
			// bk_printf("read mic data 3\r\n"); // 无
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read mic_rb :%d \n", __func__, __LINE__, size);
				os_memset(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, 0, temp_mic_samp_rate_points*2);
				//goto encoder_exit;
			}
		}
	} else {
		/* NOTE 支持 aec 就是放在 voc_info.aec_info->aec_rb 中，否则就是 voc_info.mic_rb */
		if (aud_tras_drv_info.voc_info.aec_enable) {
			/* get data from aec_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.aec_info->aec_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read aec_rb :%d \n", __func__, __LINE__, size);
				goto encoder_exit;
			}
		} else {
			/* get data from mic_ring_buff */
			// bk_printf("read mic data 4"); // 无
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, the data readed from mic_ring_buff is not a frame, size:%d \n", __func__, __LINE__, size);
				goto encoder_exit;
			}
		}
	}

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
			/* G711A encoding pcm data to a-law data*/
			for (i=0; i<temp_mic_samp_rate_points; i++) {
				aud_tras_drv_info.voc_info.encoder_temp.law_data[i] = linear2alaw(aud_tras_drv_info.voc_info.encoder_temp.pcm_data[i]);
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_G711U:
			/* G711U encoding pcm data to u-law data*/
			for (i=0; i<temp_mic_samp_rate_points; i++) {
				aud_tras_drv_info.voc_info.encoder_temp.law_data[i] = linear2ulaw(aud_tras_drv_info.voc_info.encoder_temp.pcm_data[i]);
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			break;

#if CONFIG_AUD_INTF_SUPPORT_G722
		case AUD_INTF_VOC_DATA_TYPE_G722:
        {
			/* G722 encoding pcm data to G722 data*/
            int enc_size = g722_encode(&g722_enc, aud_tras_drv_info.voc_info.encoder_temp.law_data, aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points);
            LOGD("%s, %d, len: %d, enc_size:%d \n", __func__, __LINE__, temp_mic_samp_rate_points, enc_size);
			break;
        }
#endif

		default:
			break;
	}

	temp_tx_info = aud_tras_drv_info.voc_info.tx_info;
	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			os_memcpy(temp_tx_info.ping.buff_addr, aud_tras_drv_info.voc_info.encoder_temp.law_data, temp_mic_samp_rate_points);
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			os_memcpy(temp_tx_info.ping.buff_addr, aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points * 2);
			break;

#if CONFIG_AUD_INTF_SUPPORT_G722
		case AUD_INTF_VOC_DATA_TYPE_G722:
			os_memcpy(temp_tx_info.ping.buff_addr, aud_tras_drv_info.voc_info.encoder_temp.law_data, temp_mic_samp_rate_points / 2);
			break;
#endif

		default:
			break;
	}

	/* dump tx data debug 使用*/
	if (aud_tras_drv_info.voc_info.aud_tras_dump_tx_cb) {
		aud_tras_drv_info.voc_info.aud_tras_dump_tx_cb((uint8_t *)temp_tx_info.ping.buff_addr, (uint32_t)temp_tx_info.buff_length);
	}

#if (CONFIG_CACHE_ENABLE)
		flush_all_dcache();
#endif

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
    if (gl_dialog_running) {
#endif
    	if (aud_tras_drv_info.voc_info.aud_tx_rb) {
    		int free_size = ring_buffer_get_free_size(aud_tras_drv_info.voc_info.aud_tx_rb);
    		if (free_size > temp_tx_info.buff_length) {
    			//GPIO_UP(4);
    			/* REVIEW 这个 aud_tx_rb 才是 发送给远端服务器的 rb */
    			/* NOTE  同指向 由 aud_tras_get_tx_rb() 改名而来 */
    			ring_buffer_write(aud_tras_drv_info.voc_info.aud_tx_rb, (uint8_t *)temp_tx_info.ping.buff_addr, temp_tx_info.buff_length);
    			//GPIO_DOWN(4);
    		} else {
    			//LOGE("aud_tx_rb free_size: %d \n", free_size);
    		}
    	}
#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
    }
#endif

#if 0
	/* send mic notify mailbox msg to media app */
	if (aud_tras_drv_info.aud_tras_tx_mic_data != NULL) {
		uint32_t result = BK_OK;
		mic_nofity.ptr_data = (uint32_t)temp_tx_info.ping.buff_addr;
		mic_nofity.length = (uint32_t)temp_tx_info.buff_length;
		mic_to_media_app_msg.event = EVENT_AUD_MIC_DATA_NOTIFY;
		mic_to_media_app_msg.param = (uint32_t)&mic_nofity;
#if AUD_MEDIA_SEM_ENABLE
		mic_to_media_app_msg.sem = mailbox_media_aud_mic_sem;
#else
		mic_to_media_app_msg.sem = NULL;
#endif
		mic_to_media_app_msg.result = (uint32_t)&result;
		//msg_send_to_media_major_mailbox(&mic_to_media_app_msg, (uint32_t)&result, APP_MODULE);

#if AUD_MEDIA_SEM_ENABLE
		ret = rtos_get_semaphore(&mailbox_media_aud_mic_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore \n", __func__, __LINE__);
			ret = BK_FAIL;
		}
		else
		{
			ret = result;
		}
#endif
	}
#endif

    AUD_ENC_PROCESS_END();

	return ret;

encoder_exit:

	return BK_FAIL;
}


#if CONFIG_AUD_TRAS_DAC_DEBUG
static bk_err_t aud_tras_voc_dac_debug(bool enable)
{
	os_printf("%s, enable:%d \r\n", __func__, enable);
	if (enable == aud_voc_dac_debug_flag)
		return BK_FAIL;

	//open dac debug
	FRESULT fr;
	if (enable) {
		/*open file to save data write to speaker */
		fr = f_open(&dac_debug_file, dac_debug_file_name, FA_CREATE_ALWAYS | FA_WRITE);
		if (fr != FR_OK) {
			LOGE("open %s fail.\r\n", dac_debug_file_name);
			return BK_FAIL;
		}
		aud_voc_dac_debug_flag = true;
		os_printf("start dac debug \r\n");
	} else {
		/*open file to save data write to speaker */
		fr = f_close(&dac_debug_file);
		if (fr != FR_OK) {
			LOGE("open %s fail.\r\n", dac_debug_file_name);
			return BK_FAIL;
		}
		aud_voc_dac_debug_flag = false;
		os_printf("stop dac debug \r\n");
	}
	return BK_OK;
}
#endif

/* NOTE rx_info.decoder_rb 中数据从 agora_rtc_user_audio_rx_data_handle 而来的 */
/* NOTE 从 rx_info.decoder_rb 中读取，解码后，放入到 voc_info.speaker_rb 或 voc_info.aec_info->ref_rb */
static bk_err_t aud_tras_dec(void) 
{
	uint32_t size = 0;
	uint32_t i = 0;

    bool fill_slience_flag = false;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
		return BK_OK;

    AUD_DEC_PROCESS_START();

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

	uint8_t data_type_temp = aud_tras_drv_info.voc_info.data_type;
	if (i4s_record_flag && !i4s_get_tone_state()) {
		aud_tras_drv_info.voc_info.data_type = AUD_INTF_VOC_DATA_TYPE_PCM;
	}

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			/* check the frame number in decoder_ring_buffer */
			if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
				if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
					//os_printf("decoder process \r\n", size);
					/* get G711A data from decoder_ring_buff */
					size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
						LOGE("%s, %d, read decoder_ring_buff G711A data fail \n", __func__, __LINE__);
						if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						else
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					}
				} else {
					if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					else
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
				}

				/* dump rx data */
				if (aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb) {
					aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb(aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
				}

				if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U) {
					/* G711U decoding u-law data to pcm data*/
					for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
						aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = ulaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
					}
				} else {
					/* G711A decoding a-law data to pcm data*/
					for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
						aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = alaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
					}
				}
			} else { // end spk_type = BOARD
				if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.speaker_rb) > aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					/* check the frame number in decoder_ring_buffer */
					if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
						//os_printf("decoder process \r\n", size);
						/* get G711A data from decoder_ring_buff */
						//addAON_GPIO_Reg0x9 = 2;
						size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
							LOGE("%s, %d, read decoder_ring_buff G711A data fail \n", __func__, __LINE__);
							if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
								os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
							else
								os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						}
						//addAON_GPIO_Reg0x9 = 0;
					} else {
						if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						else
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					}

					/* dump rx data */
					if (aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb) {
						aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb(aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					}

					if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U) { /* NOTE 解码 */
						/* G711U decoding u-law data to pcm data*/
						for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
							aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = ulaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
						}
					} else {
						/* G711A decoding a-law data to pcm data*/
						for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
							aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = alaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
						}
					}
				}
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
				if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					//os_printf("decoder process \r\n", size);
					/* get pcm data from decoder_ring_buff */
					size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
						LOGE("%s, %d, read decoder_ring_buff pcm data fail \n", __func__, __LINE__);
                        fill_slience_flag = true;
						//os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					}
				} else {
				    fill_slience_flag = true;
					//os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
				}

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
                /* force fill slience when dialog not running */
                if (!gl_dialog_running) {
                    fill_slience_flag = true;
                }
#endif
				/* dump rx data debug */
				if (aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb) {
					aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb((uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
				}

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                /* Check whether play prompt tone */
                if (gl_prompt_tone_play_flag) {
                    int r_size = aud_tras_drv_read_prompt_tone_data((char *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2, 0);
                    if (r_size <= 0 && gl_prompt_tone_empty_notify) {
                        /* prompt tone pool empty */
                        gl_prompt_tone_empty_notify(gl_notify_user_data);
                        os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
                        /* send message to aud_tras_drv_main to stop prompt_tone play */
                        if (aud_tras_drv_send_msg(AUD_TRAS_STOP_PROMPT_TONE, NULL) != BK_OK)
                        {
                            LOGE("%s, %d, send tras stop prompt tone fail\n", __func__, __LINE__);
                        }
                    } else {
                        if (r_size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
                            os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data + r_size, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2 - r_size);
                        }
                    }
                } else {
#endif
                    if (fill_slience_flag) {
                    	// bk_printf("slience now\r\n");
                        os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
                    }
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                }
#endif

			} else {
				if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.speaker_rb) > aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					/* check the frame number in decoder_ring_buffer */
					if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
						//os_printf("decoder process \r\n", size);
						/* get pcm data from decoder_ring_buff */
						size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
						if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
							LOGE("%s, %d, read decoder_ring_buff pcm data fail \n", __func__, __LINE__);
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
						}
					} else {
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					}

					/* dump rx data */
					if (aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb) {
						aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb((uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					}
				}
			}
			break;

#if CONFIG_AUD_INTF_SUPPORT_G722
		case AUD_INTF_VOC_DATA_TYPE_G722: /* NOTE 直冲这里 */
        {
			/* check the frame number in decoder_ring_buffer */
			if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
				if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2) {
					//os_printf("decoder process \r\n", size);
					/* get G711A data from decoder_ring_buff */
					/* NOTE 这里得到  */
					size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
					if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2) {
						LOGE("%s, %d, read decoder_ring_buff G722 data fail \n", __func__, __LINE__);
                        fill_slience_flag = true;
						//os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
					}
				} else {
				    fill_slience_flag = true;
					//os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
				}

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
                /* force fill slience when dialog not running */
                if (!gl_dialog_running) {
                    fill_slience_flag = true;
                }
#endif

				/* dump rx data */
				if (aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb) {
					aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb(aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
				}

#if CONFIG_AUD_INTF_SUPPORT_MULTIPLE_SPK_SOURCE_TYPE
                int r_size = 0;

                switch (spk_source_type)
                {
                    case SPK_SOURCE_TYPE_PROMPT_TONE:
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                        //LOGI("%s, %d, read prompt tone data\n", __func__, __LINE__);
                        r_size = aud_tras_drv_read_prompt_tone_data((char *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2, 0);
                        if (r_size <= 0 && gl_prompt_tone_empty_notify) {
                            /* prompt tone pool empty */
                            gl_prompt_tone_empty_notify(gl_notify_user_data);
                            os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
                            /* send message to aud_tras_drv_main to stop prompt_tone play */
                            if (aud_tras_drv_send_msg(AUD_TRAS_STOP_PROMPT_TONE, NULL) != BK_OK)
                            {
                                LOGE("%s, %d, send tras stop prompt tone fail\n", __func__, __LINE__);
                            }
                        } else {
                            if (r_size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
                                os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data + r_size, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2 - r_size);
                            }
                        }
                        SPK_DATA_DUMP_BY_UART_DATA(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
#else
                        LOGW("%s, SPK_SOURCE_TYPE_PROMPT_TONE not support, please enable CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE\n", __func__);
#endif
                        break;

                    case SPK_SOURCE_TYPE_A2DP:
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
                        /* play a2dp music */
                        //LOGI("%s, %d, read a2dp music data\n", __func__, __LINE__);
                        r_size = aud_tras_drv_read_prompt_tone_data((char *)a2dp_read_buff, a2dp_frame_size, 0);
                        if (r_size <= 0) {
                            /* prompt tone pool empty */
                            if (r_size != a2dp_frame_size) {
                                os_memset(a2dp_read_buff, 0, a2dp_frame_size);
                            }
                        }
                        SPK_DATA_DUMP_BY_UART_DATA(a2dp_read_buff, a2dp_frame_size);
#else
                        LOGW("%s, SPK_SOURCE_TYPE_A2DP not support, please enable CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP\n", __func__);
#endif
                        break;

                    case SPK_SOURCE_TYPE_VOICE: /* REVIEW this */
                        if (fill_slience_flag) {
                            os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
                        } else {
                        	/* NOTE 解码 */
                            /* G722 decoding g722 data to pcm data*/
                            int dec_size = g722_decode(&g722_dec, aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
                            LOGD("%s, %d, len: %d, dec_size: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2, dec_size * 2);
                        }
                        break;

                    default:
                        break;
                }
#else
                if (fill_slience_flag) {
                    os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
                } else {
                    /* G722 decoding g722 data to pcm data*/
                    int dec_size = g722_decode(&g722_dec, aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
                    LOGD("%s, %d, len: %d, dec_size: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2, dec_size * 2);
                }
#endif
			} else {
			    LOGE("%s, %d, not support uac, need TODO\n", __func__, __LINE__);
                //TODO
            }
        }
			break;
#endif

		default:
			break;
	}

#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
	mic_delay_num++;
	os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	if (mic_delay_num == 50) {
		aud_tras_drv_info.voc_info.decoder_temp.pcm_data[0] = 0x2FFF;
		mic_delay_num = 0;
		LOGI("%s, %d, mic_delay_num \n", __func__, __LINE__);
	}
#endif

#if CONFIG_AEC_ECHO_COLLECT_MODE_SOFTWARE
	if (aud_tras_drv_info.voc_info.aec_enable) {
		/* read mic fill data size */
		uint32_t mic_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.mic_rb));
		//os_printf("mic_rb: fill_size=%d \r\n", mic_fill_size);
		uint32_t speaker_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.speaker_rb));
		//os_printf("speaker_rb: fill_size=%d \r\n", speaker_fill_size);
		uint32_t ref_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.aec_info->ref_rb));
		//os_printf("ref_rb: fill_size=%d \r\n", ref_fill_size);
		/* 设置参考信号延迟(采样点数，需要dump数据观察) */
#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
		os_printf("%s, %d, MIC_DELAY: %d \n", __func__, __LINE__, (mic_fill_size + speaker_fill_size - ref_fill_size)/2);
#endif
		if ((mic_fill_size + speaker_fill_size - ref_fill_size)/2 < 0) {
			LOGE("%s, %d, MIC_DELAY is error, ref_fill_size: %d \n", __func__, __LINE__, ref_fill_size);
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, 0);
		} else {
			//aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, (mic_fill_size + speaker_fill_size - ref_fill_size)/2 + CONFIG_AUD_TRAS_AEC_MIC_DELAY_POINTS);
		}

		if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.aec_info->ref_rb)) > aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2) {
			size = ring_buffer_write(&(aud_tras_drv_info.voc_info.aec_info->ref_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2);
			if (size != aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2) {
				LOGE("%s, %d, write data to ref_ring_buff fail, size=%d \n", __func__, __LINE__, size);
				goto decoder_exit;
			}
		}
	}
#endif


#if CONFIG_AUD_TRAS_DAC_DEBUG
	if (aud_voc_dac_debug_flag) {
		//dump the data write to speaker
		FRESULT fr;
		uint32 uiTemp = 0;
		uint32_t i = 0, j = 0;
		/* write data to file */
		fr = f_write(&dac_debug_file, (uint32_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2, &uiTemp);
		if (fr != FR_OK) {
			LOGE("%s, %d, write %s fail \n", __func__, __LINE__, dac_debug_file_name);
			return BK_FAIL;
		}

		//write 8K sin data
		for (i = 0; i < aud_tras_drv_info.voc_info.speaker_samp_rate_points*2; i++) {
			for (j = 0; j < 8; j++) {
				*(uint32_t *)0x47800048 = PCM_8000[j];
			}
			i += 8;
		}
	} else
#endif
	{
	/* save the data after G711A processed to encoder_ring_buffer */
		if ((ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) == aud_tras_drv_info.voc_info.speaker_rb.capacity))
		{
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
            if (spk_source_type == SPK_SOURCE_TYPE_A2DP)
            {
            	// bk_printf("write speaker 3\r\n"); // 无
				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)a2dp_read_buff, a2dp_frame_size);
				if (size != a2dp_frame_size) {
					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
					goto decoder_exit;
				}
                //SPK_DATA_DUMP_BY_UART_DATA(a2dp_read_buff, a2dp_frame_size);
				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)a2dp_read_buff, a2dp_frame_size);
				if (size != a2dp_frame_size) {
					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
					goto decoder_exit;
				}
                //SPK_DATA_DUMP_BY_UART_DATA(a2dp_read_buff, a2dp_frame_size);
			}
            else
#endif
			{
				// bk_printf("write speaker 4\r\n"); // 无
				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
				if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
					goto decoder_exit;
				}
                //SPK_DATA_DUMP_BY_UART_DATA(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);

			#if 1
				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
				if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
					goto decoder_exit;
				}
                //SPK_DATA_DUMP_BY_UART_DATA(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			#else
				os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
				if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
					goto decoder_exit;
				}
			#endif
			}
			aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
		} else {
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP // NOTE 不执行
            if (spk_source_type == SPK_SOURCE_TYPE_A2DP) {
                if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) > a2dp_frame_size) {
                	// bk_printf("write speaker 5\r\n"); // 无
                    size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)a2dp_read_buff, a2dp_frame_size);
                    if (size != a2dp_frame_size) {
                        LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
                        goto decoder_exit;
                    }
                    //SPK_DATA_DUMP_BY_UART_DATA(a2dp_read_buff, a2dp_frame_size);
                    aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
                    LOGD("write a2dp data to audio dac\n");
                }
            }
            else
#endif
            {   /* NOTE 循环执行下面的 */
    			if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) > aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
    				// bk_printf("write speaker 6\r\n");
    				// BUG 暂且不知道这个实际作用 但是这个关闭会导致发声很嘶哑
    				// NOTE   数据内容就是 speaker_ring_buff ，这里没有采用什么ring_buffer_read ，而是直接靠 dma 和 speaker_ring_buff 来控制
    				//  这里接收的 远端来的数据 放入了 voc_info.speaker_rb 和 voc_info.aec_info->ref_rb
#if I4S_RECORD_REPEAT_MODE
    				if (i4s_record_flag) {
    					aud_tras_drv_info.voc_info.data_type = data_type_temp;
    					ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    					return BK_OK;
    				} else {
    					size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    				}
#else
    				size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
#endif    				    				
    				if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
    					LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
    					goto decoder_exit;
    				}
                    //SPK_DATA_DUMP_BY_UART_DATA(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    				aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
    			}
            }
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
			size = ring_buffer_read(&aud_tras_drv_info.voc_info.speaker_rb, (uint8_t *)aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
				LOGE("%s, %d, read one frame pcm speaker data fail \n", __func__, __LINE__);
				os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			}
		}
    }

	/* call callback to notify app */
	if (aud_tras_drv_info.aud_tras_rx_spk_data)
		aud_tras_drv_info.aud_tras_rx_spk_data((unsigned int)aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);

    AUD_DEC_PROCESS_END();

    aud_tras_drv_info.voc_info.data_type = data_type_temp;

	return BK_OK;

decoder_exit:

	return BK_FAIL;
}


static bk_err_t aud_tras_drv_mic_tx_data(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
#if (CONFIG_CACHE_ENABLE)
		flush_all_dcache();
#endif
	}

	/* get a fram mic data from mic_ring_buff */
	if (ring_buffer_get_fill_size(&(aud_tras_drv_info.mic_info.mic_rb)) >= aud_tras_drv_info.mic_info.frame_size) {
		size = ring_buffer_read(&(aud_tras_drv_info.mic_info.mic_rb), (uint8_t*)aud_tras_drv_info.mic_info.temp_mic_addr, aud_tras_drv_info.mic_info.frame_size);
		if (size != aud_tras_drv_info.mic_info.frame_size) {
			LOGE("%s, %d, read mic_ring_buff fail, size:%d \n", __func__, __LINE__, size);
		}
	}

	/* call callback to notify app */
	if (aud_tras_drv_info.aud_tras_tx_mic_data) /* NOTE 通知那边 可以接收 mic data */
	{
		mic_nofity.ptr_data = (uint32_t)aud_tras_drv_info.mic_info.temp_mic_addr;
		mic_nofity.length = (uint32_t)aud_tras_drv_info.mic_info.frame_size;
		mic_to_media_app_msg.event = EVENT_AUD_MIC_DATA_NOTIFY;
		mic_to_media_app_msg.param = (uint32_t)&mic_nofity;
//		LOGE("%s, size:%d \n", __func__, aud_tras_drv_info.mic_info.frame_size);
		msg_send_notify_to_media_major_mailbox(&mic_to_media_app_msg, APP_MODULE);
	}

	return ret;
}

static bk_err_t aud_tras_drv_spk_req_data(audio_packet_t * packet)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

	/* NOTE 从 spk_rx_rb 读取，然后写到 spk_rb 中去 */
	/* NOTE  spk_rx_rb 中的数据来源 是 agora_rtc_user_audio_rx_data_handle 函数 */
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* get speaker data from spk_rx_ring_buff */
		// bk_printf("spk_rx_rb write again 1\r\n"); /* NOTE 无 */
		if (ring_buffer_get_fill_size(aud_tras_drv_info.spk_info.spk_rx_rb) >= aud_tras_drv_info.spk_info.frame_size) {
			size = ring_buffer_read(aud_tras_drv_info.spk_info.spk_rx_rb, (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
			if (size != aud_tras_drv_info.spk_info.frame_size) {
				LOGE("%s, %d, read spk_rx_ring_buff fail, size:%d \n", __func__, __LINE__, size);
				os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
			}
		} else {
			LOGD("%s, %d, spk_rx_rb is empty \n", __func__, __LINE__);
			os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
		}

		/* write spk_rx_data to audio dac */
		size = ring_buffer_write(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
		if (size != aud_tras_drv_info.spk_info.frame_size) {
			LOGE("%s, %d, write spk_data to audio dac fail, size:%d \n", __func__, __LINE__, size);
			//TODO
			//add handle code
		}

		/* call callback to notify app */
		if (aud_tras_drv_info.aud_tras_rx_spk_data)  // NOTE 这个处理在 major_mailbox 中被移除了
		{
			spk_to_media_app_msg.event = EVENT_AUD_SPK_DATA_NOTIFY;
			spk_to_media_app_msg.param = aud_tras_drv_info.spk_info.frame_size;
			msg_send_notify_to_media_major_mailbox(&spk_to_media_app_msg, APP_MODULE);
			//ret = aud_tras_drv_info.aud_tras_rx_spk_data((unsigned int)aud_tras_drv_info.spk_info.frame_size);
		}
	} else {
		if (ring_buffer_get_fill_size(&aud_tras_drv_info.spk_info.spk_rb) >= aud_tras_drv_info.spk_info.uac_spk_buff_size) {
			/* get pcm data from spk_rb */
			size = ring_buffer_read(&aud_tras_drv_info.spk_info.spk_rb, aud_tras_drv_info.spk_info.uac_spk_buff, aud_tras_drv_info.spk_info.uac_spk_buff_size);
			if (size != aud_tras_drv_info.spk_info.uac_spk_buff_size) {
				LOGE("%s, %d, read spk_rb uac spk data fail \n", __func__, __LINE__);
				os_memset(aud_tras_drv_info.spk_info.uac_spk_buff, 0x00, aud_tras_drv_info.spk_info.uac_spk_buff_size);
			}
		} else {
			os_memset(aud_tras_drv_info.spk_info.uac_spk_buff, 0x00, aud_tras_drv_info.spk_info.uac_spk_buff_size);
		}

		/* save the data after G711A processed to encoder_ring_buffer */
		if (ring_buffer_get_free_size(&(aud_tras_drv_info.spk_info.spk_rb)) > aud_tras_drv_info.spk_info.frame_size) {
			/* get speaker data from spk_rx_ring_buff */
			if (ring_buffer_get_fill_size(aud_tras_drv_info.spk_info.spk_rx_rb) >= aud_tras_drv_info.spk_info.frame_size) {
				size = ring_buffer_read(aud_tras_drv_info.spk_info.spk_rx_rb, (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
				if (size != aud_tras_drv_info.spk_info.frame_size) {
					LOGE("%s, %d, read spk_rx_ring_buff fail, size:%d \n", __func__, __LINE__, size);
					os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
				}
			} else {
				LOGD("%s, %d, spk_rx_rb is empty \n", __func__, __LINE__);
				os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
			}

			/* write spk_rx_data to audio dac */
			size = ring_buffer_write(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
			if (size != aud_tras_drv_info.spk_info.frame_size) {
				LOGE("%s, %d, write spk_data to audio dac fail, size:%d \n", __func__, __LINE__, size);
				//TODO
				//add handle code
			}

			/* call callback to notify app */
			if (aud_tras_drv_info.aud_tras_rx_spk_data)
			{
				spk_to_media_app_msg.event = EVENT_AUD_SPK_DATA_NOTIFY;
				spk_to_media_app_msg.param = aud_tras_drv_info.spk_info.frame_size;
				msg_send_notify_to_media_major_mailbox(&spk_to_media_app_msg, APP_MODULE);
				//ret = aud_tras_drv_info.aud_tras_rx_spk_data((unsigned int)aud_tras_drv_info.spk_info.frame_size);
			}
		}
	}

	return ret;
}


static bk_err_t aud_tras_drv_spk_init(aud_intf_spk_config_t *spk_cfg)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	LOGI("%s \n", __func__);

	aud_tras_drv_info.spk_info.spk_type = spk_cfg->spk_type;
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* get audio dac config */
		aud_tras_drv_info.spk_info.dac_config = audio_tras_drv_malloc(sizeof(aud_dac_config_t));
		if (aud_tras_drv_info.spk_info.dac_config == NULL) {
			LOGE("%s, %d, adc_config os_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_spk_init_exit;
		} else {
			aud_tras_drv_info.spk_info.dac_config->samp_rate = spk_cfg->samp_rate;
			if (spk_cfg->spk_chl == AUD_INTF_SPK_CHL_LEFT)
				aud_tras_drv_info.spk_info.dac_config->dac_chl = AUD_DAC_CHL_L;
			else
				aud_tras_drv_info.spk_info.dac_config->dac_chl = AUD_DAC_CHL_LR;
			aud_tras_drv_info.spk_info.dac_config->work_mode = spk_cfg->work_mode;
			aud_tras_drv_info.spk_info.dac_config->dac_gain = spk_cfg->spk_gain;	//default 2D  3F  15
			aud_tras_drv_info.spk_info.dac_config->dac_clk_invert = AUD_DAC_CLK_INVERT_RISING;
			aud_tras_drv_info.spk_info.dac_config->clk_src = AUD_CLK_XTAL;
		}
		aud_tras_drv_info.spk_info.spk_en = true;
	} else {
		/* set audio uac config */
		if (aud_tras_drv_info.spk_info.uac_spk_config == NULL) {
			aud_tras_drv_info.spk_info.uac_spk_config = audio_tras_drv_malloc(sizeof(aud_uac_spk_config_t));
			if (aud_tras_drv_info.spk_info.uac_spk_config == NULL) {
				LOGE("%s, %d, uac_config os_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_spk_init_exit;
			} else {
				aud_tras_drv_info.spk_info.uac_spk_config->spk_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.spk_info.uac_spk_config->spk_samp_rate = 8000;
				aud_tras_drv_info.spk_info.uac_spk_config->spk_volume = 0;
			}
			aud_tras_drv_info.spk_info.uac_spk_config->spk_samp_rate = spk_cfg->samp_rate;
			aud_tras_drv_info.spk_info.uac_spk_config->spk_volume = spk_cfg->spk_gain;
		}
	}

	aud_tras_drv_info.spk_info.spk_chl = spk_cfg->spk_chl;
	aud_tras_drv_info.spk_info.frame_size = spk_cfg->frame_size;
	aud_tras_drv_info.spk_info.fifo_frame_num = spk_cfg->fifo_frame_num;
	aud_tras_drv_info.spk_info.spk_rx_rb = spk_cfg->spk_rx_rb;
	aud_tras_drv_info.spk_info.spk_rx_ring_buff = spk_cfg->spk_rx_ring_buff;

	/* malloc spk_ring_buff to save audio data */
	aud_tras_drv_info.spk_info.spk_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.spk_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.spk_info.spk_ring_buff == NULL) {
		LOGE("%s, %d, malloc speaker ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_spk_init_exit;
	}

	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* audio dac config */
		ret = aud_tras_dac_config(aud_tras_drv_info.spk_info.dac_config);
		if (ret != BK_OK) {
			LOGE("%s, %d, audio dac init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DAC;
			goto aud_tras_drv_spk_init_exit;
		}
		LOGI("step1: init audio and config DAC complete \n");

		/* init dma driver */
		ret = bk_dma_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_spk_init_exit;
		}

		/* allocate free DMA channel */
		aud_tras_drv_info.spk_info.spk_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		if ((aud_tras_drv_info.spk_info.spk_dma_id < DMA_ID_0) || (aud_tras_drv_info.spk_info.spk_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc dac dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_spk_init_exit;
		}

		/* config audio dac dma to carry speaker data to "spk_ring_buff" */
		ret = aud_tras_dac_dma_config(aud_tras_drv_info.spk_info.spk_dma_id, aud_tras_drv_info.spk_info.spk_ring_buff, aud_tras_drv_info.spk_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.spk_info.frame_size, aud_tras_drv_info.spk_info.spk_chl);
		if (ret != BK_ERR_AUD_INTF_OK) {
			LOGE("%s, %d, config audio dac dma fail \n", __func__, __LINE__);
			ret = ret;
			goto aud_tras_drv_spk_init_exit;
		}

		/* init spk_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.spk_ring_buff, aud_tras_drv_info.spk_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.spk_info.spk_dma_id, RB_DMA_TYPE_READ);
		LOGI("step2: init dma:%d, and spk ring buff:%p, size:%d complete \n", aud_tras_drv_info.spk_info.spk_dma_id, aud_tras_drv_info.spk_info.spk_ring_buff, aud_tras_drv_info.spk_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	} else if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		/* init spk_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.spk_ring_buff, aud_tras_drv_info.spk_info.frame_size*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

		/* close spk */
		aud_tras_drv_info.spk_info.spk_en = false;
		aud_tras_drv_info.spk_info.uac_spk_buff_size = aud_tras_drv_info.spk_info.frame_size;
		aud_tras_drv_info.spk_info.uac_spk_buff = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.spk_info.uac_spk_buff_size);

		/* register uac connect and disconnect callback */
		/* NOTE USB Audio Class 断开、连接 回调 */
		bk_aud_uac_register_disconnect_cb(aud_tras_uac_disconnect_cb);
		bk_aud_uac_register_connect_cb(aud_tras_uac_connect_cb);

		LOGI("%s, %d, init uac driver \n", __func__, __LINE__);
		ret = bk_aud_uac_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, init uac driver fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto aud_tras_drv_spk_init_exit;
		}

		/* set uac speaker volume */
		//aud_tras_drv_set_spk_gain(aud_tras_drv_info.spk_info.uac_spk_config->spk_volume);

//		aud_tras_drv_info.uac_status = AUD_INTF_UAC_CONNECTED;

		LOGI("step2: init spk ring buff:%p, size:%d complete \n", aud_tras_drv_info.spk_info.spk_ring_buff, aud_tras_drv_info.spk_info.frame_size*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto aud_tras_drv_spk_init_exit;
	}

	/* init spk temp buffer */
	aud_tras_drv_info.spk_info.temp_spk_addr = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.spk_info.frame_size);
	if (aud_tras_drv_info.spk_info.temp_spk_addr == NULL) {
		LOGE("%s, %d, malloc temp spk ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_spk_init_exit;
	}
	LOGI("step3: init temp spk ring buff complete \n");

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		uac_mic_spk_count_open();
	}
#endif

	/* change status: AUD_TRAS_DRV_SPK_NULL --> AUD_TRAS_DRV_SPK_IDLE */
	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_SPK_STA_IDLE;

	LOGI("step4: init spk complete \n");

	return BK_ERR_AUD_INTF_OK;

aud_tras_drv_spk_init_exit:
	aud_tras_drv_info.spk_info.spk_en = false;
	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_MIC_STA_NULL;
	aud_tras_drv_info.spk_info.spk_chl = AUD_INTF_MIC_CHL_MIC1;

	if (aud_tras_drv_info.spk_info.dac_config)
		audio_tras_drv_free(aud_tras_drv_info.spk_info.dac_config);
	aud_tras_drv_info.spk_info.dac_config = NULL;

	aud_tras_drv_info.spk_info.frame_size = 0;
	if (aud_tras_drv_info.spk_info.spk_dma_id != DMA_ID_MAX) {
		bk_dma_deinit(aud_tras_drv_info.spk_info.spk_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.spk_info.spk_dma_id);
	}
	aud_tras_drv_info.spk_info.spk_dma_id = DMA_ID_MAX;

	if (aud_tras_drv_info.spk_info.spk_ring_buff != NULL) {
		ring_buffer_clear(&(aud_tras_drv_info.spk_info.spk_rb));
		audio_tras_drv_free(aud_tras_drv_info.spk_info.spk_ring_buff);
	}
	aud_tras_drv_info.spk_info.spk_ring_buff = NULL;

	if (aud_tras_drv_info.spk_info.temp_spk_addr != NULL)
		audio_tras_drv_free(aud_tras_drv_info.spk_info.temp_spk_addr);
	aud_tras_drv_info.spk_info.temp_spk_addr = NULL;

	if (aud_tras_drv_info.spk_info.uac_spk_config)
		audio_tras_drv_free(aud_tras_drv_info.spk_info.uac_spk_config);
	aud_tras_drv_info.spk_info.uac_spk_config = NULL;

	LOGE("%s, %d, init spk fail \n", __func__, __LINE__);

	return err;
}

static bk_err_t aud_tras_drv_spk_deinit(void)
{
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* disable audio dac */
		aud_tras_dac_pa_ctrl(false, true);
		bk_aud_dac_stop();
		bk_dma_stop(aud_tras_drv_info.spk_info.spk_dma_id);
		bk_dma_deinit(aud_tras_drv_info.spk_info.spk_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.spk_info.spk_dma_id);
		bk_aud_dac_deinit();
		if (aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_NULL)
			bk_aud_driver_deinit();
		audio_tras_drv_free(aud_tras_drv_info.spk_info.dac_config);
		aud_tras_drv_info.spk_info.dac_config = NULL;
	} else {
		aud_tras_drv_info.uac_status = AUD_INTF_UAC_NORMAL_DISCONNECTED;
		bk_aud_uac_stop_spk();
		bk_aud_uac_driver_deinit();

		audio_tras_drv_free(aud_tras_drv_info.spk_info.uac_spk_buff);
		aud_tras_drv_info.spk_info.uac_spk_buff = NULL;
		aud_tras_drv_info.spk_info.uac_spk_buff_size = 0;
	}

	aud_tras_drv_info.spk_info.spk_en = false;
	aud_tras_drv_info.spk_info.spk_chl = AUD_INTF_MIC_CHL_MIC1;
	aud_tras_drv_info.spk_info.frame_size = 0;
	aud_tras_drv_info.spk_info.spk_dma_id = DMA_ID_MAX;

	if (aud_tras_drv_info.spk_info.uac_spk_config)
		audio_tras_drv_free(aud_tras_drv_info.spk_info.uac_spk_config);
	aud_tras_drv_info.spk_info.uac_spk_config = NULL;

	ring_buffer_clear(&(aud_tras_drv_info.spk_info.spk_rb));
	audio_tras_drv_free(aud_tras_drv_info.spk_info.spk_ring_buff);
	aud_tras_drv_info.spk_info.spk_ring_buff = NULL;

	audio_tras_drv_free(aud_tras_drv_info.spk_info.temp_spk_addr);
	aud_tras_drv_info.spk_info.temp_spk_addr = NULL;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		uac_mic_spk_count_close();
	}
#endif

	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_SPK_STA_NULL;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_spk_start(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;

	if ((aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_IDLE) && (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_PAUSE)) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* enable dac */
		bk_aud_dac_start();
		bk_aud_dac_start();

		aud_tras_dac_pa_ctrl(true, true);
		if (!bk_dma_get_enable_status(aud_tras_drv_info.spk_info.spk_dma_id)) {
			ret = bk_dma_start(aud_tras_drv_info.spk_info.spk_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start speaker dma fail \n", __func__, __LINE__);
				return BK_ERR_AUD_INTF_DMA;
			}
		}
	} else {
		LOGI("%s, %d, start uac spk \n", __func__, __LINE__);
		if (aud_tras_drv_info.uac_status == AUD_INTF_UAC_CONNECTED) {
			ret = bk_aud_uac_start_spk();
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac spk fail, ret: %d \n", __func__, __LINE__, ret);
				return BK_ERR_AUD_INTF_UAC_SPK;
			} else {
				aud_tras_drv_info.uac_spk_open_status = true;
				aud_tras_drv_info.uac_spk_open_current = true;
			}
		} else {
			aud_tras_drv_info.uac_spk_open_status = true;
		}
	}

	if (aud_tras_drv_info.spk_info.status == AUD_TRAS_DRV_SPK_STA_IDLE) {
		/* get speaker data from spk_rx_ring_buff */
		if (ring_buffer_get_fill_size(aud_tras_drv_info.spk_info.spk_rx_rb) >= aud_tras_drv_info.spk_info.frame_size) {
			size = ring_buffer_read(aud_tras_drv_info.spk_info.spk_rx_rb, (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
			if (size != aud_tras_drv_info.spk_info.frame_size) {
				LOGE("%s, %d, read spk_rx_ring_buff fail, size: %d \n", __func__, __LINE__, size);
			}
		} else {
			LOGD("%s, %d, spk_rx_rb is empty \n", __func__, __LINE__);
			os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
		}

		/* write spk_rx_data to audio dac */
		size = ring_buffer_write(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
		if (size != aud_tras_drv_info.spk_info.frame_size) {
			LOGE("%s, %d, write spk_data to audio dac fail, size: %d \n", __func__, __LINE__, size);
			//return BK_FAIL;
		}

		if (ring_buffer_get_fill_size(aud_tras_drv_info.spk_info.spk_rx_rb) >= aud_tras_drv_info.spk_info.frame_size) {
			size = ring_buffer_read(aud_tras_drv_info.spk_info.spk_rx_rb, (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
			if (size != aud_tras_drv_info.spk_info.frame_size) {
				LOGE("%s, %d, read spk_rx_ring_buff fail, size:%d \n", __func__, __LINE__, size);
			}
		} else {
			LOGD("%s, %d, spk_rx_rb is empty \n", __func__, __LINE__);
			os_memset(aud_tras_drv_info.spk_info.temp_spk_addr, 0, aud_tras_drv_info.spk_info.frame_size);
		}

		/* write spk_rx_data to audio dac */
		size = ring_buffer_write(&(aud_tras_drv_info.spk_info.spk_rb), (uint8_t*)aud_tras_drv_info.spk_info.temp_spk_addr, aud_tras_drv_info.spk_info.frame_size);
		if (size != aud_tras_drv_info.spk_info.frame_size) {
			LOGE("%s, %d, write spk_data to audio dac fail, size: %d \n", __func__, __LINE__, size);
			//return BK_FAIL;
		}
	}

	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_SPK_STA_START;

	return BK_ERR_AUD_INTF_OK;
}

/* not stop dma, only disable adc */
static bk_err_t aud_tras_drv_spk_pause(void)
{
	if (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_START) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* disable adc */
		aud_tras_dac_pa_ctrl(false, true);
		bk_aud_dac_stop();
//		bk_dma_stop(aud_tras_drv_info.spk_info.spk_dma_id);
	} else {
		bk_aud_uac_stop_spk();
	}

	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_SPK_STA_PAUSE;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_spk_stop(void)
{
	bk_err_t ret = BK_OK;

	if ((aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_START) && (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_PAUSE)) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* disable adc */
		aud_tras_dac_pa_ctrl(false, true);
		bk_aud_dac_stop();
		bk_dma_stop(aud_tras_drv_info.spk_info.spk_dma_id);
	} else {
		ret = bk_aud_uac_stop_spk();
		if (ret != BK_OK) {
			LOGE("%s, %d, stop uac spk fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_UAC_SPK;
		} else {
			aud_tras_drv_info.uac_spk_open_status = false;
			aud_tras_drv_info.uac_spk_open_current = false;
		}
	}

	ring_buffer_clear(&(aud_tras_drv_info.spk_info.spk_rb));
	aud_tras_drv_info.spk_info.status = AUD_TRAS_DRV_SPK_STA_IDLE;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_spk_set_chl(aud_intf_spk_chl_t spk_chl)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;

	switch (spk_chl) {
		case AUD_INTF_SPK_CHL_LEFT:
			ret = bk_aud_dac_set_chl(AUD_DAC_CHL_L);
			if (ret == BK_OK)
				ret = bk_dma_set_dest_data_width(aud_tras_drv_info.mic_info.mic_dma_id, DMA_DATA_WIDTH_16BITS);
			break;

		case AUD_INTF_SPK_CHL_DUAL:
			ret = bk_aud_dac_set_chl(AUD_DAC_CHL_LR);
			if (ret == BK_OK)
				ret = bk_dma_set_dest_data_width(aud_tras_drv_info.mic_info.mic_dma_id, DMA_DATA_WIDTH_32BITS);
			break;

		default:
			break;
	}

	return ret;
}

/* NOTE 这个函数未使用 */
static void aud_tras_drv_mic_uac_mic_cb(uint8_t *buff, uint32_t count)
{
	bk_err_t ret = BK_OK;
	uint32_t write_size = 0;
	uint32_t data = 0;

	if (aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_START && count > 0) {
		if (ring_buffer_get_free_size(&aud_tras_drv_info.mic_info.mic_rb) >= count) {
			for (write_size = 0; write_size < count/4; write_size++) {
				data = *((uint32_t *)buff);
				// bk_printf("write mic data 3\r\n"); // 没运行
				ring_buffer_write(&aud_tras_drv_info.mic_info.mic_rb, (uint8_t *)&data, 4);
			}
		}
	}

	if (uac_mic_read_flag && (ring_buffer_get_fill_size(&aud_tras_drv_info.mic_info.mic_rb) < aud_tras_drv_info.mic_info.frame_size))
		uac_mic_read_flag = false;

	if ((ring_buffer_get_fill_size(&aud_tras_drv_info.mic_info.mic_rb) > aud_tras_drv_info.mic_info.frame_size) && (uac_mic_read_flag == false)) {
		uac_mic_read_flag = true; 
		/* send msg to TX_DATA to process mic data */
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_TX_DATA, NULL);
		if (ret != kNoErr) {
			LOGE("%s, %d, send msg: AUD_TRAS_DRV_MIC_TX_DATA fail \n", __func__, __LINE__);
			uac_mic_read_flag = false;
		}
	}
}

// NOTE 
static bk_err_t aud_tras_drv_mic_init(aud_intf_mic_config_t *mic_cfg)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	LOGD("%s, %d, mic_cfg->frame_size: %d \n", __func__, __LINE__, mic_cfg->frame_size);

//	aud_tras_drv_info.mic_info.aud_tras_drv_mic_event_cb = mic_cfg->aud_tras_drv_mic_event_cb;
	aud_tras_drv_info.mic_info.mic_type = mic_cfg->mic_type;
	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* get audio adc config */
		aud_tras_drv_info.mic_info.adc_config = audio_tras_drv_malloc(sizeof(aud_adc_config_t));
		if (aud_tras_drv_info.mic_info.adc_config == NULL) {
			LOGE("%s, %d, adc_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_mic_init_exit;
		} else {
			if (mic_cfg->mic_chl == AUD_INTF_MIC_CHL_MIC1) {
				aud_tras_drv_info.mic_info.adc_config->adc_chl = AUD_ADC_CHL_L;
			} else if (mic_cfg->mic_chl == AUD_INTF_MIC_CHL_DUAL) {
				aud_tras_drv_info.mic_info.adc_config->adc_chl = AUD_ADC_CHL_LR;
			} else {
				LOGW("%s, %d, mic chl is error, set default: AUD_MIC_MIC1_ENABLE \n", __func__, __LINE__);
				aud_tras_drv_info.mic_info.adc_config->adc_chl = AUD_ADC_CHL_L;
			}
			aud_tras_drv_info.mic_info.adc_config->samp_rate = mic_cfg->samp_rate;
			aud_tras_drv_info.mic_info.adc_config->adc_gain = mic_cfg->mic_gain;	//default: 0x2d
			aud_tras_drv_info.mic_info.adc_config->adc_samp_edge = AUD_ADC_SAMP_EDGE_RISING;
			aud_tras_drv_info.mic_info.adc_config->adc_mode = AUD_ADC_MODE_DIFFEN;
			aud_tras_drv_info.mic_info.adc_config->clk_src = AUD_CLK_XTAL;
		}
		aud_tras_drv_info.mic_info.mic_en = true;
	} else {
		/* set audio uac config */
		if (aud_tras_drv_info.mic_info.uac_mic_config == NULL) {
			aud_tras_drv_info.mic_info.uac_mic_config = audio_tras_drv_malloc(sizeof(aud_uac_mic_config_t));
			if (aud_tras_drv_info.mic_info.uac_mic_config == NULL) {
				LOGE("%s, %d, uac_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_mic_init_exit;
			} else {
				aud_tras_drv_info.mic_info.uac_mic_config->mic_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.mic_info.uac_mic_config->mic_samp_rate = 8000;
			}
			aud_tras_drv_info.mic_info.uac_mic_config->mic_samp_rate = mic_cfg->samp_rate;
		}
	}

	aud_tras_drv_info.mic_info.mic_chl = mic_cfg->mic_chl;

	/* TODO 细看是如何计算 单帧字节 */
	aud_tras_drv_info.mic_info.frame_size = mic_cfg->frame_size;

	/* config audio adc or uac and dma */
	LOGD("%s, %d, mic_ring_buff size: %d \n", __func__, __LINE__, aud_tras_drv_info.mic_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	aud_tras_drv_info.mic_info.mic_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.mic_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.mic_info.mic_ring_buff == NULL) {
		LOGE("%s, %d, malloc mic ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_mic_init_exit;
	}

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* audio adc config */
		ret = aud_tras_adc_config(aud_tras_drv_info.mic_info.adc_config);
		if (ret != BK_OK) {
			LOGE("%s, %d, audio adc init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_ADC;
			goto aud_tras_drv_mic_init_exit;
		}
		LOGI("step1: init audio and config ADC complete \n");

		/* init dma driver */
		ret = bk_dma_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_mic_init_exit;
		}

		/* allocate free DMA channel */
		aud_tras_drv_info.mic_info.mic_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		if ((aud_tras_drv_info.mic_info.mic_dma_id < DMA_ID_0) || (aud_tras_drv_info.mic_info.mic_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc adc dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_mic_init_exit;
		}

		/* config audio adc dma to carry mic data to "mic_ring_buff" */
		ret = aud_tras_adc_dma_config(aud_tras_drv_info.mic_info.mic_dma_id, aud_tras_drv_info.mic_info.mic_ring_buff, aud_tras_drv_info.mic_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.mic_info.frame_size, aud_tras_drv_info.mic_info.mic_chl);
		if (ret != BK_OK) {
			LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
			err = ret;
			goto aud_tras_drv_mic_init_exit;
		}

		/* init mic_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.mic_info.mic_rb), (uint8_t*)aud_tras_drv_info.mic_info.mic_ring_buff, aud_tras_drv_info.mic_info.frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.mic_info.mic_dma_id, RB_DMA_TYPE_WRITE);
		LOGI("step2: init dma:%d, and mic ring buff:%p, size:%d complete \n", aud_tras_drv_info.mic_info.mic_dma_id, aud_tras_drv_info.mic_info.mic_ring_buff, aud_tras_drv_info.mic_info.frame_size * 2);
	} else if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		/* init mic_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.mic_info.mic_rb), (uint8_t*)aud_tras_drv_info.mic_info.mic_ring_buff, aud_tras_drv_info.mic_info.frame_size*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

		/* register uac connect and disconnect callback */
		bk_aud_uac_register_disconnect_cb(aud_tras_uac_disconnect_cb);
		bk_aud_uac_register_connect_cb(aud_tras_uac_connect_cb);

		LOGI("%s, %d, init uac driver \n", __func__, __LINE__);
		ret = bk_aud_uac_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, init uac driver fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto aud_tras_drv_mic_init_exit;
		}
		LOGI("step2: init mic ring buff:%p, size:%d complete \n", aud_tras_drv_info.mic_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto aud_tras_drv_mic_init_exit;
	}

	/* init mic temp buffer */
	aud_tras_drv_info.mic_info.temp_mic_addr = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.mic_info.frame_size);
	if (aud_tras_drv_info.mic_info.temp_mic_addr == NULL) {
		LOGE("%s, %d, malloc temp mic ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_mic_init_exit;
	}
	LOGI("step3: init temp mic ring buff complete \n");

	/* change status: AUD_TRAS_DRV_MIC_NULL --> AUD_TRAS_DRV_MIC_IDLE */
	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_IDLE;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		uac_mic_spk_count_open();
	}
#endif

	LOGI("step4: init mic complete \n");

	return BK_ERR_AUD_INTF_OK;

aud_tras_drv_mic_init_exit:
	aud_tras_drv_info.mic_info.mic_en = false;
	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_NULL;
	aud_tras_drv_info.mic_info.mic_chl = AUD_INTF_MIC_CHL_MIC1;

	if (aud_tras_drv_info.mic_info.adc_config)
		audio_tras_drv_free(aud_tras_drv_info.mic_info.adc_config);
	aud_tras_drv_info.mic_info.adc_config = NULL;

	if (aud_tras_drv_info.mic_info.uac_mic_config)
		audio_tras_drv_free(aud_tras_drv_info.mic_info.uac_mic_config);
	aud_tras_drv_info.mic_info.uac_mic_config = NULL;

	aud_tras_drv_info.mic_info.frame_size = 0;
	if (aud_tras_drv_info.mic_info.mic_dma_id != DMA_ID_MAX) {
		bk_dma_stop(aud_tras_drv_info.mic_info.mic_dma_id);
		bk_dma_deinit(aud_tras_drv_info.mic_info.mic_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.mic_info.mic_dma_id);
	}
	aud_tras_drv_info.mic_info.mic_dma_id = DMA_ID_MAX;

	if (aud_tras_drv_info.mic_info.mic_ring_buff != NULL) {
		ring_buffer_clear(&(aud_tras_drv_info.mic_info.mic_rb));
		audio_tras_drv_free(aud_tras_drv_info.mic_info.mic_ring_buff);
	}
	aud_tras_drv_info.mic_info.mic_ring_buff = NULL;

	if (aud_tras_drv_info.mic_info.temp_mic_addr != NULL)
		audio_tras_drv_free(aud_tras_drv_info.mic_info.temp_mic_addr);
	aud_tras_drv_info.mic_info.temp_mic_addr = NULL;

	aud_tras_drv_info.mic_info.mic_type = AUD_INTF_MIC_TYPE_MAX;

	return err;
}

static bk_err_t aud_tras_drv_mic_deinit(void)
{
	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* disable audio adc */
		bk_aud_adc_stop();
		bk_dma_stop(aud_tras_drv_info.mic_info.mic_dma_id);
		bk_dma_deinit(aud_tras_drv_info.mic_info.mic_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.mic_info.mic_dma_id);
		bk_aud_adc_deinit();
		if (aud_tras_drv_info.spk_info.status == AUD_TRAS_DRV_SPK_STA_NULL)
			bk_aud_driver_deinit();
		audio_tras_drv_free(aud_tras_drv_info.mic_info.adc_config);
		aud_tras_drv_info.mic_info.adc_config = NULL;
	} else {
		aud_tras_drv_info.uac_status = AUD_INTF_UAC_NORMAL_DISCONNECTED;
		bk_aud_uac_stop_mic();
//		bk_aud_uac_register_mic_callback(NULL);
		bk_aud_uac_driver_deinit();
	}

	aud_tras_drv_info.mic_info.mic_en = false;
	aud_tras_drv_info.mic_info.mic_chl = AUD_INTF_MIC_CHL_MIC1;
	aud_tras_drv_info.mic_info.frame_size = 0;
	aud_tras_drv_info.mic_info.mic_dma_id = DMA_ID_MAX;

	if (aud_tras_drv_info.mic_info.uac_mic_config)
		audio_tras_drv_free(aud_tras_drv_info.mic_info.uac_mic_config);
	aud_tras_drv_info.mic_info.uac_mic_config = NULL;

	ring_buffer_clear(&(aud_tras_drv_info.mic_info.mic_rb));
	audio_tras_drv_free(aud_tras_drv_info.mic_info.mic_ring_buff);
	aud_tras_drv_info.mic_info.mic_ring_buff = NULL;

	audio_tras_drv_free(aud_tras_drv_info.mic_info.temp_mic_addr);
	aud_tras_drv_info.mic_info.temp_mic_addr = NULL;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		uac_mic_spk_count_close();
	}
#endif

	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_NULL;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_mic_start(void)
{
	bk_printf("aud_tras_drv_mic_start start\r\n");
	bk_err_t ret = BK_OK;
	if ((aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_IDLE) && (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_PAUSE)) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		ret = bk_dma_start(aud_tras_drv_info.mic_info.mic_dma_id);
		if (ret != BK_OK) {
			LOGE("%s, %d, start mic dma fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_DMA;
		}

		/* enable adc */
		if (aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_IDLE)
			bk_aud_adc_start();
	} else {
		LOGI("%s, %d, start uac mic \n", __func__, __LINE__);
		if (aud_tras_drv_info.uac_status == AUD_INTF_UAC_CONNECTED) {
			ret = bk_aud_uac_start_mic();
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac mic fail, ret:%d \n", __func__, __LINE__, ret);
				return BK_ERR_AUD_INTF_UAC_MIC;
			} else {
				aud_tras_drv_info.uac_mic_open_status = true;
				aud_tras_drv_info.uac_mic_open_current = true;
			}
		} else {
			aud_tras_drv_info.uac_mic_open_status = true;
		}
	}

	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_START;

	return BK_ERR_AUD_INTF_OK;
}

/* not stop dma, only disable adc */
static bk_err_t aud_tras_drv_mic_pause(void)
{
	bk_err_t ret = BK_OK;
	if (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_START) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* stop adc dma */
		//bk_aud_stop_adc();
		bk_dma_stop(aud_tras_drv_info.mic_info.mic_dma_id);
	} else {
		LOGI("%s, %d, stop uac mic \n", __func__, __LINE__);
		ret = bk_aud_uac_stop_mic();
		if (ret != BK_OK) {
			LOGE("%s, %d, stop uac mic fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_UAC_MIC;
		}
	}

	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_PAUSE;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_mic_stop(void)
{
	bk_err_t ret = BK_OK;
	if ((aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_START) && (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_PAUSE)) {
		return BK_ERR_AUD_INTF_STA;
	}

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* disable adc */
		bk_aud_adc_stop();
		bk_dma_stop(aud_tras_drv_info.mic_info.mic_dma_id);
		ring_buffer_clear(&(aud_tras_drv_info.mic_info.mic_rb));
	} else {
		LOGI("%s, %d, stop uac mic \n", __func__, __LINE__);
		ret = bk_aud_uac_stop_mic();
		if (ret != BK_OK) {
			LOGE("%s, %d, stop uac mic fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_UAC_MIC;
		} else {
			aud_tras_drv_info.uac_mic_open_status = false;
			aud_tras_drv_info.uac_mic_open_current = false;
		}
	}

	aud_tras_drv_info.mic_info.status = AUD_TRAS_DRV_MIC_STA_IDLE;

	return BK_ERR_AUD_INTF_OK;
}

/*
static bk_err_t aud_tras_drv_mic_get_gain(uint8_t *value)
{
	*value = aud_tras_drv_info.mic_info.adc_config->adc_set_gain;

	return BK_OK;
}
*/
/*
 * 单次采样数据位宽
 * 单麦克风： 16位     双麦克风: 32位
 */
static bk_err_t aud_tras_drv_mic_set_chl(aud_intf_mic_chl_t mic_chl)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;
	switch (mic_chl) {
		case AUD_INTF_MIC_CHL_MIC1:
			ret = bk_aud_adc_set_chl(AUD_MIC_MIC1);
			if (ret == BK_OK)
				ret = bk_dma_set_src_data_width(aud_tras_drv_info.mic_info.mic_dma_id, DMA_DATA_WIDTH_16BITS);
			break;

		case AUD_INTF_MIC_CHL_DUAL:
			ret = bk_aud_adc_set_chl(AUD_MIC_BOTH);
			if (ret == BK_OK)
				ret = bk_dma_set_src_data_width(aud_tras_drv_info.mic_info.mic_dma_id, DMA_DATA_WIDTH_32BITS);
			break;

		default:
			LOGE("%s, %d, not support, mic_chl: %d \n", __func__, __LINE__, mic_chl);
			break;
	}

	return ret;
}

static bk_err_t aud_tras_drv_mic_set_samp_rate(uint32_t samp_rate)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;
	ret = bk_aud_adc_set_samp_rate(samp_rate);

	return ret;
}

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
/* NOTE voc init 唯一调用 传的 NULL */
static bk_err_t aud_tras_drv_prompt_tone_play_open(url_info_t *prompt_tone)
{
    LOGI("%s\n", __func__);

#if CONFIG_PROMPT_TONE_SOURCE_VFS
#if CONFIG_PROMPT_TONE_CODEC_MP3
    //TODO
#endif
#if CONFIG_PROMPT_TONE_CODEC_WAV
    prompt_tone_play_cfg_t config = DEFAULT_VFS_WAV_PROMPT_TONE_PLAY_CONFIG();
#endif
#if CONFIG_PROMPT_TONE_CODEC_PCM
    prompt_tone_play_cfg_t config = DEFAULT_VFS_PCM_PROMPT_TONE_PLAY_CONFIG();
#endif
#else   //array
#if CONFIG_PROMPT_TONE_CODEC_MP3
    //TODO
#endif
#if CONFIG_PROMPT_TONE_CODEC_WAV
    //TODO
#endif
#if CONFIG_PROMPT_TONE_CODEC_PCM
    prompt_tone_play_cfg_t config = DEFAULT_ARRAY_PCM_PROMPT_TONE_PLAY_CONFIG();
#endif
#endif

    if (prompt_tone)
    {
        config.source_cfg.url = (char *)prompt_tone->url;
        config.source_cfg.total_size = prompt_tone->total_len;
    }

    /* stop play */
    if (gl_prompt_tone_play_handle)
    {
        prompt_tone_play_destroy(gl_prompt_tone_play_handle);
        gl_prompt_tone_play_handle = NULL;
    }
    gl_prompt_tone_play_handle = prompt_tone_play_create(&config);
    if (!gl_prompt_tone_play_handle)
    {
        LOGE("%s, %d, prompt_tone_play_create fail\n", __func__, __LINE__);
        return BK_FAIL;
    }
    /* BUG 这里没有指明为哪一个 没有url */
    prompt_tone_play_open(gl_prompt_tone_play_handle);

    return BK_OK;
}

static bk_err_t aud_tras_drv_prompt_tone_play_close(void)
{
    /* stop play */
    if (gl_prompt_tone_play_handle)
    {
        prompt_tone_play_close(gl_prompt_tone_play_handle, 0);
        prompt_tone_play_destroy(gl_prompt_tone_play_handle);
        gl_prompt_tone_play_handle = NULL;
    }

    return BK_OK;
}
#endif

static bk_err_t aud_tras_drv_voc_deinit(void)
{
//	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
//		return BK_ERR_AUD_INTF_OK;

	/* disable mic */
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_stop();
		bk_aud_adc_deinit();
		bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
		bk_dma_deinit(aud_tras_drv_info.voc_info.adc_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.voc_info.adc_dma_id);
		if (aud_tras_drv_info.voc_info.adc_config) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.adc_config);
			aud_tras_drv_info.voc_info.adc_config = NULL;
		}
	} else {
		bk_aud_uac_stop_mic();
//		bk_aud_uac_unregister_mic_callback();
	}

	/* disable spk */
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		aud_tras_dac_pa_ctrl(false, true);
		bk_aud_dac_stop();
		bk_aud_dac_deinit();
		if (aud_tras_drv_info.voc_info.dac_config) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.dac_config);
			aud_tras_drv_info.voc_info.dac_config = NULL;
		}
		/* stop dma */
		bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		bk_dma_deinit(aud_tras_drv_info.voc_info.dac_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.voc_info.dac_dma_id);
	} else {
		bk_aud_uac_stop_spk();
//		bk_aud_uac_unregister_spk_callback();
//		bk_aud_uac_register_spk_buff_ptr(NULL, 0);
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
		bk_aud_driver_deinit();

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		aud_tras_drv_info.uac_status = AUD_INTF_UAC_NORMAL_DISCONNECTED;
		//bk_aud_uac_register_disconnect_cb(aud_tras_uac_disconnect_cb);
		bk_aud_uac_register_connect_cb(NULL);
		bk_aud_uac_driver_deinit();
	}

	/* disable AEC */
	aud_tras_drv_aec_decfg();

	if (aud_tras_drv_info.voc_info.uac_config) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_config);
		aud_tras_drv_info.voc_info.uac_config = NULL;
	}

	/* free audio ring buffer */
	//mic deconfig
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));
	if (aud_tras_drv_info.voc_info.mic_ring_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.mic_ring_buff);
		aud_tras_drv_info.voc_info.mic_ring_buff = NULL;
	}
	aud_tras_drv_info.voc_info.mic_samp_rate_points = 0;
	aud_tras_drv_info.voc_info.mic_frame_number = 0;
	aud_tras_drv_info.voc_info.adc_dma_id = DMA_ID_MAX;

	//speaker deconfig
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	if (aud_tras_drv_info.voc_info.speaker_ring_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.speaker_ring_buff);
		aud_tras_drv_info.voc_info.speaker_ring_buff = NULL;
	}
	aud_tras_drv_info.voc_info.speaker_samp_rate_points = 0;
	aud_tras_drv_info.voc_info.speaker_frame_number = 0;
	aud_tras_drv_info.voc_info.dac_dma_id = DMA_ID_MAX;

	/* tx and rx deconfig */
	//tx deconfig
	aud_tras_drv_info.voc_info.tx_info.tx_buff_status = false;
	aud_tras_drv_info.voc_info.tx_info.buff_length = 0;
	aud_tras_drv_info.voc_info.tx_info.ping.busy_status = false;
	aud_tras_drv_info.voc_info.tx_info.ping.buff_addr = NULL;
	//rx deconfig
	aud_tras_drv_info.voc_info.rx_info.rx_buff_status = false;
	aud_tras_drv_info.voc_info.rx_info.decoder_ring_buff = NULL;
	aud_tras_drv_info.voc_info.rx_info.decoder_rb = NULL;
	aud_tras_drv_info.voc_info.rx_info.frame_size = 0;
	aud_tras_drv_info.voc_info.rx_info.frame_num = 0;
	aud_tras_drv_info.voc_info.rx_info.rx_buff_seq_tail = 0;
	aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq = 0;
	aud_tras_drv_info.voc_info.rx_info.fifo_frame_num = 0;

	/* uac spk buffer */
	if (aud_tras_drv_info.voc_info.uac_spk_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_spk_buff);
		aud_tras_drv_info.voc_info.uac_spk_buff = NULL;
		aud_tras_drv_info.voc_info.uac_spk_buff_size = 0;
	}

#if CONFIG_AUD_INTF_SUPPORT_G722
	if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G722) {
        g722_encode_release(&g722_enc);
        g722_decode_release(&g722_dec);
	}
#endif

	/* encoder_temp and decoder_temp deconfig*/
	if (aud_tras_drv_info.voc_info.encoder_temp.law_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.encoder_temp.law_data);
		aud_tras_drv_info.voc_info.encoder_temp.law_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.decoder_temp.law_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.decoder_temp.law_data);
		aud_tras_drv_info.voc_info.decoder_temp.law_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.encoder_temp.pcm_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.encoder_temp.pcm_data);
		aud_tras_drv_info.voc_info.encoder_temp.pcm_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.decoder_temp.pcm_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.decoder_temp.pcm_data);
		aud_tras_drv_info.voc_info.decoder_temp.pcm_data = NULL;
	}
	aud_tras_drv_info.voc_info.aud_tx_rb = NULL;
	aud_tras_drv_info.voc_info.i4s_aud_tx_rb = NULL;
	aud_tras_drv_info.voc_info.data_type = AUD_INTF_VOC_DATA_TYPE_PCM;

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		uac_mic_spk_count_close();
	}
#endif

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    /* close prompt tone */
    aud_tras_drv_prompt_tone_play_close();

    /* free ringbuffer */
    if (gl_prompt_tone_rb)
    {
        rb_destroy(gl_prompt_tone_rb);
        gl_prompt_tone_rb = NULL;
    }
#endif


#if CONFIG_AI_ASR_MODE_CPU2
    msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_DEINIT_REQ, MINOR_MODULE, 0, NULL);

    if (gl_aud_cp2_ready_sem) {
        rtos_deinit_semaphore(&gl_aud_cp2_ready_sem);
        gl_aud_cp2_ready_sem = NULL;
    }

    vote_stop_cpu2_core(CPU2_USER_ASR);

#endif

	/* change status:
				AUD_TRAS_DRV_VOC_IDLE --> AUD_TRAS_DRV_VOC_NULL
				AUD_TRAS_DRV_VOC_START --> AUD_TRAS_DRV_VOC_NULL
				AUD_TRAS_DRV_VOC_STOP --> AUD_TRAS_DRV_VOC_NULL
	*/
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_NULL;

#if CONFIG_AUD_TRAS_AEC_DUMP_MODE_UART
	bk_uart_deinit(CONFIG_AUD_TRAS_AEC_DUMP_UART_ID);
#endif

    AEC_DATA_DUMP_BY_UART_CLOSE();
    SPK_DATA_DUMP_BY_UART_CLOSE();

	LOGI("%s, %d, voc deinit complete \n", __func__, __LINE__);
	return BK_ERR_AUD_INTF_OK;
}

static void uart_dump_mic_data(uart_id_t id, uint32_t baud_rate)
{
	uart_config_t config = {0};
	os_memset(&config, 0, sizeof(uart_config_t));
	if (id == 0) {
		gpio_dev_unmap(GPIO_10);
		gpio_dev_map(GPIO_10, GPIO_DEV_UART1_RXD);
		gpio_dev_unmap(GPIO_11);
		gpio_dev_map(GPIO_11, GPIO_DEV_UART1_TXD);
	} else if (id == 2) {
		gpio_dev_unmap(GPIO_40);
		gpio_dev_map(GPIO_40, GPIO_DEV_UART3_RXD);
		gpio_dev_unmap(GPIO_41);
		gpio_dev_map(GPIO_41, GPIO_DEV_UART3_TXD);
	} else {
		gpio_dev_unmap(GPIO_0);
		gpio_dev_map(GPIO_0, GPIO_DEV_UART2_TXD);
		gpio_dev_unmap(GPIO_1);
		gpio_dev_map(GPIO_1, GPIO_DEV_UART2_RXD);
	}

	config.baud_rate = baud_rate;
	config.data_bits = UART_DATA_8_BITS;
	config.parity = UART_PARITY_NONE;
	config.stop_bits = UART_STOP_BITS_1;
	config.flow_ctrl = UART_FLOWCTRL_DISABLE;
	config.src_clk = UART_SCLK_XTAL_26M;

	if (bk_uart_init(id, &config) != BK_OK) {
		LOGE("%s, %d, init uart fail \n", __func__, __LINE__);
	} else {
		LOGE("%s, %d, init uart ok \n", __func__, __LINE__);
	}
}

/* audio transfer driver voice mode init */
static bk_err_t aud_tras_drv_voc_init(aud_intf_voc_config_t* voc_cfg)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	/* callback config */
//	aud_tras_drv_info.voc_info.aud_tras_drv_voc_event_cb = voc_cfg->aud_tras_drv_voc_event_cb;

	/* get aec config */
	aud_tras_drv_info.voc_info.aec_enable = voc_cfg->aec_enable;
	if (aud_tras_drv_info.voc_info.aec_enable) {
		aud_tras_drv_info.voc_info.aec_info = audio_tras_drv_malloc(sizeof(aec_info_t));
		if (aud_tras_drv_info.voc_info.aec_info == NULL) {
			LOGE("%s, %d, aec_info audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			aud_tras_drv_info.voc_info.aec_info->aec = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_config = NULL;
			aud_tras_drv_info.voc_info.aec_info->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.aec_info->samp_rate_points = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->mic_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->out_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_ref_ring_buff = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_out_ring_buff = NULL;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.address = NULL;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.capacity = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.wp= 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.rp = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.dma_id = DMA_ID_MAX;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.dma_type = RB_DMA_TYPE_NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.address = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.capacity = 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.wp= 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.rp = 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.dma_id = DMA_ID_MAX;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.dma_type = RB_DMA_TYPE_NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_config = audio_tras_drv_malloc(sizeof(aec_config_t));
			if (aud_tras_drv_info.voc_info.aec_info->aec_config == NULL) {
				LOGE("%s, %d, aec_config_t audio_tras_drv_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			} else {
				aud_tras_drv_info.voc_info.aec_info->aec_config->init_flags = voc_cfg->aec_setup->init_flags;
				aud_tras_drv_info.voc_info.aec_info->aec_config->mic_delay = voc_cfg->aec_setup->mic_delay;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ec_depth = voc_cfg->aec_setup->ec_depth;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ref_scale = voc_cfg->aec_setup->ref_scale;
				aud_tras_drv_info.voc_info.aec_info->aec_config->voice_vol = voc_cfg->aec_setup->voice_vol;
				aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxThr = voc_cfg->aec_setup->TxRxThr;
				aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxFlr = voc_cfg->aec_setup->TxRxFlr;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ns_level = voc_cfg->aec_setup->ns_level;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ns_para = voc_cfg->aec_setup->ns_para;
				aud_tras_drv_info.voc_info.aec_info->aec_config->drc = voc_cfg->aec_setup->drc;
			}
		}
	} else { // else aec_enable
		aud_tras_drv_info.voc_info.aec_info = NULL;
	}

	/* NOTE add mic rb */
	aud_tras_drv_info.voc_info.aud_tx_rb = voc_cfg->aud_tx_rb;
	aud_tras_drv_info.voc_info.i4s_aud_tx_rb = voc_cfg->i4s_aud_tx_rb;
	aud_tras_drv_info.voc_info.data_type = voc_cfg->data_type;
	LOGI("%s, %d, aud_tras_drv_info.voc_info.data_type:%d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.data_type);
	aud_tras_drv_info.voc_info.mic_en = voc_cfg->mic_en;
	aud_tras_drv_info.voc_info.spk_en = voc_cfg->spk_en;
	aud_tras_drv_info.voc_info.mic_type = voc_cfg->mic_type;
	aud_tras_drv_info.voc_info.spk_type = voc_cfg->spk_type;

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* get audio adc config */
		aud_tras_drv_info.voc_info.adc_config = audio_tras_drv_malloc(sizeof(aud_adc_config_t));
		if (aud_tras_drv_info.voc_info.adc_config == NULL) {
			LOGE("%s, %d, adc_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
			aud_tras_drv_info.voc_info.adc_config->adc_chl = AUD_ADC_CHL_LR;
#else
			aud_tras_drv_info.voc_info.adc_config->adc_chl = AUD_ADC_CHL_L;
#endif
			aud_tras_drv_info.voc_info.adc_config->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.adc_config->adc_gain = voc_cfg->aud_setup.adc_gain;	//default: 0x2d
			aud_tras_drv_info.voc_info.adc_config->adc_samp_edge = AUD_ADC_SAMP_EDGE_RISING;
			aud_tras_drv_info.voc_info.adc_config->adc_mode = AUD_ADC_MODE_DIFFEN;
			aud_tras_drv_info.voc_info.adc_config->clk_src = AUD_CLK_XTAL;
		}
	} else { // end mic_type = BOARD
		/* set audio uac config */
		if (aud_tras_drv_info.voc_info.uac_config == NULL) {
			aud_tras_drv_info.voc_info.uac_config = audio_tras_drv_malloc(sizeof(aud_uac_config_t));
			if (aud_tras_drv_info.voc_info.uac_config == NULL) {
				LOGE("%s, %d, uac_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			} else {
				aud_tras_drv_info.voc_info.uac_config->mic_config.mic_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate = 8000;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate = 8000;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_volume = 0;
			}
			aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.uac_config->spk_config.spk_volume = voc_cfg->aud_setup.dac_gain;
		}
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* get audio dac config */
		aud_tras_drv_info.voc_info.dac_config = audio_tras_drv_malloc(sizeof(aud_dac_config_t));
		if (aud_tras_drv_info.voc_info.adc_config == NULL) {
			LOGE("%s, %d, dac_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			aud_tras_drv_info.voc_info.dac_config->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.dac_config->dac_chl = AUD_DAC_CHL_L;
			aud_tras_drv_info.voc_info.dac_config->work_mode = voc_cfg->aud_setup.spk_mode;
			aud_tras_drv_info.voc_info.dac_config->dac_gain = voc_cfg->aud_setup.dac_gain;	//default 2D  3F  15
			aud_tras_drv_info.voc_info.dac_config->dac_clk_invert = AUD_DAC_CLK_INVERT_RISING;
			aud_tras_drv_info.voc_info.dac_config->clk_src = AUD_CLK_XTAL;
		}
	} // BUG 这里不处理 喇叭是 UAC 的情况

	/* get ring buffer config */
	//aud_tras_drv_info.voc_info.mode = setup->aud_trs_mode;
	switch (voc_cfg->samp_rate) {
		case 8000:
			aud_tras_drv_info.voc_info.mic_samp_rate_points = 160;
			aud_tras_drv_info.voc_info.speaker_samp_rate_points = 160;
			break;

		case 16000:
			aud_tras_drv_info.voc_info.mic_samp_rate_points = 320; // NOTE this 
			aud_tras_drv_info.voc_info.speaker_samp_rate_points = 320;
			break;

		default:
			break;
	}

	aud_tras_drv_info.voc_info.mic_frame_number = voc_cfg->aud_setup.mic_frame_number;
	aud_tras_drv_info.voc_info.speaker_frame_number = voc_cfg->aud_setup.speaker_frame_number;

	// Print : 4 , 2
	// bk_printf("the mic max frame_num is %d, the speaker max frame_num is %d\r\n", aud_tras_drv_info.voc_info.mic_frame_number, 
	// 						aud_tras_drv_info.voc_info.speaker_frame_number);

	/* get tx and rx context config */
	/* 这里是共享地址的部分 将这个指向了就可以使用了 */
	aud_tras_drv_info.voc_info.tx_info = voc_cfg->tx_info;
	aud_tras_drv_info.voc_info.rx_info = voc_cfg->rx_info;


	/*  -------------------------step0: init audio and config ADC and DAC -------------------------------- */
	/* config mailbox according audio transfer work mode */

	/*  -------------------------step2: init AEC and malloc two ring buffers -------------------------------- */
	/* init aec and config aec according AEC_enable*/
	if (aud_tras_drv_info.voc_info.aec_enable) {
		ret = aud_tras_drv_aec_cfg();
		if (ret != BK_OK) {
			err = BK_ERR_AUD_INTF_AEC;
			goto aud_tras_drv_voc_init_exit;
		}
		LOGI("%s, %d, aec samp_rate_points: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.aec_info->samp_rate_points);
		ret = aud_tras_drv_aec_buff_cfg(aud_tras_drv_info.voc_info.aec_info);
		if (ret != BK_OK) {
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}
		LOGI("step2: init AEC and malloc two ring buffers complete \n");
	}

	/* -------------------step3: init and config DMA to carry mic and ref data ----------------------------- */
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
	// NOTE 注意这里用的 voc_info ，不是 mic_info 或 spk_info
	aud_tras_drv_info.voc_info.mic_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points*4*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
#else
	aud_tras_drv_info.voc_info.mic_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
#endif

	if (aud_tras_drv_info.voc_info.mic_ring_buff == NULL) {
		LOGE("%s, %d, malloc mic ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* init dma driver */
		ret = bk_dma_driver_init(); /* NOTE 线程安全函数 */
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* NOTE allocate free DMA channel  这里硬件挂钩 adc 对麦克风 采样 */
		aud_tras_drv_info.voc_info.adc_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		bk_printf("adc dma id is %d\r\n", aud_tras_drv_info.voc_info.adc_dma_id);
		if ((aud_tras_drv_info.voc_info.adc_dma_id < DMA_ID_0) || (aud_tras_drv_info.voc_info.adc_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc adc dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* config audio adc dma to carry mic data to "mic_ring_buff" */
		/* NOTE mic 是 adc 模拟 -> 数字 */
#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
		// REVIEW this 16位的采样点 (化为字节需要乘以2 有远、近之分再乘以2)
		ret = aud_tras_adc_dma_config(aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*4)*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.mic_samp_rate_points*4, AUD_INTF_MIC_CHL_DUAL);
#else
		ret = aud_tras_adc_dma_config(aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*2)*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.mic_samp_rate_points*2, AUD_INTF_MIC_CHL_MIC1);
#endif
		if (ret != BK_OK) {
			LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}

#if CONFIG_AEC_ECHO_COLLECT_MODE_HARDWARE
		// 这里将 voc_info.mic_rb 与 voc_info.mic_ring_buff 绑定了 然后 后面aec才是直接从 mic_rb 中读取
		// NOTE 注意 这里并没有采用 ring_buffer_write 去写入数据
		ring_buffer_init(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*4*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.adc_dma_id, RB_DMA_TYPE_WRITE);
		LOGI("step3: init and config mic DMA complete, adc_dma_id:%d, mic_ring_buff:%p, size:%d, carry_length:%d \n", aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*4)*aud_tras_drv_info.voc_info.mic_frame_number, aud_tras_drv_info.voc_info.mic_samp_rate_points*4);
#else
		ring_buffer_init(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.adc_dma_id, RB_DMA_TYPE_WRITE);
		LOGI("step3: init and config mic DMA complete, adc_dma_id:%d, mic_ring_buff:%p, size:%d, carry_length:%d \n", aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*2)*aud_tras_drv_info.voc_info.mic_frame_number, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
#endif
	} else if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		/* init mic_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);
		LOGI("%s, %d, uac mic_ring_buff:%p, size:%d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);

		/* debug */
//		uart_dump_mic_data(UART_ID_2, 2000000);

		/* register uac connect and disconnect callback */
		bk_aud_uac_register_disconnect_cb(aud_tras_uac_disconnect_cb);
		bk_aud_uac_register_connect_cb(aud_tras_uac_connect_cb);

		LOGI("%s, %d, init uac driver \n", __func__, __LINE__);
		ret = bk_aud_uac_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, init uac driver fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto aud_tras_drv_voc_init_exit;
		}

		LOGI("step3: init voc mic ring buff:%p, size:%d complete \n", aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	} else {
		err = BK_ERR_AUD_INTF_UAC_MIC;
		goto aud_tras_drv_voc_init_exit;
	}

	/*  -------------------step4: init and config DMA to carry dac data ----------------------------- */
	aud_tras_drv_info.voc_info.speaker_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.voc_info.speaker_ring_buff == NULL) {
		LOGE("%s, %d, malloc speaker ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* init dma driver */
		ret = bk_dma_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* allocate free DMA channel */
		aud_tras_drv_info.voc_info.dac_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		bk_printf("dac dma id is %d\r\n", aud_tras_drv_info.voc_info.dac_dma_id);
		if ((aud_tras_drv_info.voc_info.dac_dma_id < DMA_ID_0) || (aud_tras_drv_info.voc_info.dac_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc dac dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* config audio dac dma to carry dac data to "speaker_ring_buff" */
		/* NOTE 这里的 speaker_ring_buff 来自 speaker_rb ,接收到的远端数据(解码)后存储的位置 */
		/* NOTE speaker dac 数字 -> 模拟 */
		/* REVIEW important function aud_tras_drv_info.voc_info.speaker_samp_rate_points 是点数 ，而 PCM 一般都是16位的 所以这里 *2 变为字节  */
		ret = aud_tras_dac_dma_config(aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2, AUD_INTF_SPK_CHL_LEFT);
		if (ret != BK_OK) {
			LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}

		/* NOTE 这里 */
		ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.dac_dma_id, RB_DMA_TYPE_READ);
		
	    /* NOTE 增加一个缓冲区指向这里的 speaker_rb  在 aud_intf.c 中被替换 */
	    i4s_aud_rx_rb = &(aud_tras_drv_info.voc_info.speaker_rb);
	    aud_tras_drv_send_msg(AUD_TRAS_RECEIVE_ADDRESS, NULL);

	    bk_printf("aud_tras_drv_info.voc_info.speaker_rb is %p\r\n", i4s_aud_rx_rb);


		LOGI("step4: init and config speaker DMA complete, dac_dma_id:%d, speaker_ring_buff:%p, size:%d, carry_length:%d \r\n", aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	} else if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		/* save one frame pcm speaker data for usb used */
		aud_tras_drv_info.voc_info.uac_spk_buff_size = aud_tras_drv_info.voc_info.speaker_samp_rate_points*2;
		aud_tras_drv_info.voc_info.uac_spk_buff = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.uac_spk_buff_size);
		if (!aud_tras_drv_info.voc_info.uac_spk_buff) {
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		}

		ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

		/* register uac connect and disconnect callback */
		if (aud_tras_drv_info.voc_info.mic_type != AUD_INTF_MIC_TYPE_UAC) {
			bk_aud_uac_register_disconnect_cb(aud_tras_uac_disconnect_cb);
			bk_aud_uac_register_connect_cb(aud_tras_uac_connect_cb);

			LOGI("%s, %d, init uac driver \n", __func__, __LINE__);
			ret = bk_aud_uac_driver_init();
			if (ret != BK_OK) {
				LOGE("%s, %d, init uac driver fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_DRV;
				goto aud_tras_drv_voc_init_exit;
			}
		}

		LOGI("step4: init uac speaker_ring_buff:%p, spk_ring_buff_size:%d, uac_spk_buff:%p, uac_spk_buff_size:%d\n", aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.uac_spk_buff_size);
	} else {
		//TODO
	}

	/*  -------------------------step6: init all audio ring buffers -------------------------------- */
	/* init encoder and decoder temp buffer */
	aud_tras_drv_info.voc_info.encoder_temp.pcm_data = (int16_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points * 2);
	if (aud_tras_drv_info.voc_info.encoder_temp.pcm_data == NULL) {
		LOGE("%s, %d, malloc pcm_data of encoder used fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	aud_tras_drv_info.voc_info.decoder_temp.pcm_data = (int16_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	if (aud_tras_drv_info.voc_info.decoder_temp.pcm_data == NULL) {
		LOGE("%s, %d, malloc pcm_data of decoder used fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			LOGI("%s, %d, malloc law_data temp buffer \n", __func__, __LINE__);
			aud_tras_drv_info.voc_info.encoder_temp.law_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points);
			if (aud_tras_drv_info.voc_info.encoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of encoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}

			aud_tras_drv_info.voc_info.decoder_temp.law_data = (unsigned char *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points);
			if (aud_tras_drv_info.voc_info.decoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of decoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			//os_printf("not need to malloc law_data temp buffer \r\n");
			break;

#if CONFIG_AUD_INTF_SUPPORT_G722
		case AUD_INTF_VOC_DATA_TYPE_G722:

            g722_encode_init(&g722_enc, 64000, 0);
            g722_decode_init(&g722_dec, 64000, 0);

			LOGI("%s, %d, malloc law_data temp buffer \n", __func__, __LINE__);
			aud_tras_drv_info.voc_info.encoder_temp.law_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points / 2);
			if (aud_tras_drv_info.voc_info.encoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of encoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}

			aud_tras_drv_info.voc_info.decoder_temp.law_data = (unsigned char *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points / 2);
			if (aud_tras_drv_info.voc_info.decoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of decoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}
			break;
#endif

		default:
			break;
	}

	/* audio debug */
	aud_tras_drv_info.voc_info.aud_tras_dump_tx_cb = NULL;
	aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb = NULL;
	aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb = NULL;

	/* change status: AUD_TRAS_DRV_VOC_NULL --> AUD_TRAS_DRV_VOC_IDLE */
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_IDLE;
	LOGD("step6: init aud ring buff complete \n");

#ifdef CONFIG_UAC_MIC_SPK_COUNT_DEBUG
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		uac_mic_spk_count_open();
	}
#endif

	LOGI("%s, %d, init voc complete \n", __func__, __LINE__);

#if CONFIG_AI_ASR_MODE_CPU2

	ret = rtos_init_semaphore(&gl_aud_cp2_ready_sem, 1);
	if (ret != BK_OK) {
		LOGE("%s, init jdec_config->jdec_cp2_init_sem failed\r\n", __func__);
        err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

    if(CPU2_USER_ASR == vote_start_cpu2_core(CPU2_USER_ASR)) {    //first owner start CPU2, so needs to wait sem
        rtos_get_semaphore(&gl_aud_cp2_ready_sem, BEKEN_WAIT_FOREVER);
    }

    /* NOTE 开启语音识别功能 */
    ret = msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_INIT_REQ, MINOR_MODULE, 0, NULL);
    if (ret != BK_OK) {
        LOGE("%s, %d, init asr in cpu2 fail, ret: %d\n", __func__, __LINE__, ret);
        err = BK_ERR_AUD_INTF_START_CPU2;
        goto aud_tras_drv_voc_init_exit;
    }
#endif

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    /* init ringbuffer 正常的话 这里应该是从未操作的过的函数 gl_prompt_tone_rb = NULL，过来的 */
    if (gl_prompt_tone_rb)
    {
        rb_destroy(gl_prompt_tone_rb);
        gl_prompt_tone_rb = NULL;
    }
    gl_prompt_tone_rb = rb_create(PROMPT_TONE_RB_SIZE);
    if (!gl_prompt_tone_rb)
    {
        LOGE("%s, %d, create gl_prompt_tone_rb: %d fail\n", __func__, __LINE__, PROMPT_TONE_RB_SIZE);
        goto aud_tras_drv_voc_init_exit;
    }

    /* BUG 传 NULL 不会有任何操作 */
    ret = aud_tras_drv_prompt_tone_play_open(NULL);
    if (ret != BK_OK)
    {
        LOGE("%s, %d, prompt_tone_play open fail, ret: %d\n", __func__, __LINE__, ret);
        goto aud_tras_drv_voc_init_exit;
    }
#endif

    AEC_DATA_DUMP_BY_UART_OPEN();
    SPK_DATA_DUMP_BY_UART_OPEN();

	return BK_ERR_AUD_INTF_OK;

aud_tras_drv_voc_init_exit:
	/* audio transfer driver deconfig */
	aud_tras_drv_voc_deinit();
	return err;
}

/* 
 * VOC 模块就是 mic 模块 和 speak 模块的组合
 * 所以对于mic 和 speak 模块的操作 可以自行调用 而 系统已经 voc_init 和 voc_start 了 
 */
static bk_err_t aud_tras_drv_voc_start(void)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	uint32_t size = 0;
	uint8_t *pcm_data = NULL;

	LOGI("%s \n", __func__);

	if (aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) { /* NOTE 麦克风目前是板载的 */
			/* init audio and config ADC and DAC */
			ret = aud_tras_adc_config(aud_tras_drv_info.voc_info.adc_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, audio adc init fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_ADC;
				goto audio_start_transfer_exit;
			}

			/* start DMA */
			ret = bk_dma_start(aud_tras_drv_info.voc_info.adc_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start adc dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto audio_start_transfer_exit;
			}

			/* enable adc */
			/* NOTE 先启动 dma， 再启动 adc， 防止adc先启动会让dma错乱，丢失 */
			/* wait receive data and then open adc */
			bk_aud_adc_start();
		} else {
			LOGI("%s, %d, start uac mic \n", __func__, __LINE__);
			/* check uac connect status */
			if (aud_tras_drv_info.uac_status == AUD_INTF_UAC_CONNECTED) {
				ret = bk_aud_uac_start_mic();
				if (ret != BK_OK) {
					LOGE("%s, %d, start uac mic fail, ret: %d \n", __func__, __LINE__, ret);
					err = BK_ERR_AUD_INTF_UAC_MIC;
					goto audio_start_transfer_exit;
				} else {
					aud_tras_drv_info.uac_mic_open_status = true;
					aud_tras_drv_info.uac_mic_open_current = true;
				}
			} else {
				aud_tras_drv_info.uac_mic_open_status = true;
			}
		}
	}

	if (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_OPEN) {
		pcm_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (pcm_data == NULL) {
			LOGE("%s, %d, malloc temp pcm_data fial \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto audio_start_transfer_exit;
		} else {
			os_memset(pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			ret = aud_tras_dac_config(aud_tras_drv_info.voc_info.dac_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, audio dac init fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DAC;
				goto audio_start_transfer_exit;
			}

			/* enable dac */
			/* NOTE dac 是被动消费 等着 dma 将内容传递过去 */
			bk_aud_dac_start();
            aud_tras_dac_pa_ctrl(true, true);

			ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto audio_start_transfer_exit;
			}
		} else {
#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS
			/* reopen uac mic */
			if ((aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN) && (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC)) {
				bk_aud_uac_start_mic();
			}
#endif

			LOGI("%s, %d, start uac spk \n", __func__, __LINE__);
			/* check uac connect status */
			if (aud_tras_drv_info.uac_status == AUD_INTF_UAC_CONNECTED) {
				ret = bk_aud_uac_start_spk();
				if (ret != BK_OK) {
					LOGE("%s, %d, start uac spk fail, ret: %d \r\n", __func__, __LINE__, ret);
					err = BK_ERR_AUD_INTF_UAC_SPK;
					goto audio_start_transfer_exit;
				} else {
					aud_tras_drv_info.uac_spk_open_status = true;
					aud_tras_drv_info.uac_spk_open_current = true;
				}
			} else {
				aud_tras_drv_info.uac_spk_open_status = true;
			}
		}

		// bk_printf("write speaker 7\r\n");    /* NOTE voc start 只执行一次 */
		/* write two frame data to speaker and ref ring buffer */
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
			LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto audio_start_transfer_exit;
		}
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
			LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto audio_start_transfer_exit;
		}

		audio_tras_drv_free(pcm_data);
		pcm_data = NULL;
	}
	LOGI("%s, %d, voice start complete \n", __func__, __LINE__);

	/* change status:
				AUD_TRAS_DRV_VOC_STA_IDLE --> AUD_TRAS_DRV_VOC_STA_START
				AUD_TRAS_DRV_VOC_STA_STOP --> AUD_TRAS_DRV_VOC_STA_START
	*/
	/* NOTE 如果不适用 VAD 判断 只要开始 就没停止过 */
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_START;

	return BK_ERR_AUD_INTF_OK;

audio_start_transfer_exit:
	//deinit audio transfer
	if (pcm_data)
		audio_tras_drv_free(pcm_data);

	return err;
}

/* NOTE 这个  */
static bk_err_t aud_tras_drv_voc_stop(void)
{
	bk_err_t ret = BK_OK;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_STOP)
		return ret;

	LOGI("%s \n", __func__);

	/* stop adc and dac dma */
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		ret = bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
		if (ret != BK_OK) {
			LOGE("%s, %d, stop adc dma fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_DMA;
		}
	} else {
		LOGI("%s, %d, stop uac mic \n", __func__, __LINE__);
		ret = bk_aud_uac_stop_mic();
		if (ret != BK_OK) {
			LOGE("%s, %d, stop uac mic fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_UAC_MIC;
		} else {
			aud_tras_drv_info.uac_mic_open_status = false;
			aud_tras_drv_info.uac_mic_open_current = false;
		}
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		ret = bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		if (ret != BK_OK) {
			LOGE("%s, %d, stop dac dma fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_DMA;
		}
	} else {
		LOGI("%s, %d, stop uac spk \n", __func__, __LINE__);
		ret = bk_aud_uac_stop_spk();
		if (ret != BK_OK) {
			//err = BK_ERR_AUD_INTF_DMA;
			LOGE("%s, %d, stop uac spk fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_UAC_SPK;
		} else {
			aud_tras_drv_info.uac_spk_open_status = false;
			aud_tras_drv_info.uac_spk_open_current = false;
		}
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* disable adc */
		bk_aud_adc_stop();
		bk_aud_adc_deinit();
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* disable dac */
		aud_tras_dac_pa_ctrl(false, true);
		bk_aud_dac_stop();
		bk_aud_dac_deinit();
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
	/* deinit audio driver */
		bk_aud_driver_deinit();

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC)
		bk_aud_uac_driver_deinit();

	/* clear adc and dac ring buffer */
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));

	uac_mic_read_flag = false;
	uac_spk_write_flag = false;

	/* change status:
				AUD_TRAS_DRV_VOC_STA_IDLE --> AUD_TRAS_DRV_VOC_STA_STOP
				AUD_TRAS_DRV_VOC_STA_STOP --> AUD_TRAS_DRV_VOC_STA_STOP
	*/
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_STOP;

	LOGI("%s, %d, stop voice transfer complete \n", __func__, __LINE__);

	return ret;
}

static bk_err_t aud_tras_drv_voc_ctrl_mic(aud_intf_voc_mic_ctrl_t mic_en)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	GLOBAL_INT_DECLARATION();

	if (mic_en == AUD_INTF_VOC_MIC_OPEN) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
			LOGI("%s, %d, open onboard mic \n", __func__, __LINE__);
			/* enable adc */
			bk_aud_adc_start();

			ret = bk_dma_start(aud_tras_drv_info.voc_info.adc_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start adc dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto voc_ctrl_mic_fail;
			}
		} else {
			LOGI("%s, %d, open uac mic \n", __func__, __LINE__);
			ret = bk_aud_uac_start_mic();
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac mic fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto voc_ctrl_mic_fail;
			}
		}
	} else if (mic_en == AUD_INTF_VOC_MIC_CLOSE) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
			LOGI("%s, %d, close onboard mic \n", __func__, __LINE__);
			bk_aud_adc_stop();
			bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
			ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));
		} else {
			LOGI("%s, %d, close uac mic \n", __func__, __LINE__);
			ret = bk_aud_uac_stop_mic();
			if (ret != BK_OK) {
				LOGE("%s, %d, stop uac mic fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto voc_ctrl_mic_fail;
			}
			uac_mic_read_flag = false;
		}
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto voc_ctrl_mic_fail;
	}

	GLOBAL_INT_DISABLE();
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD)
		aud_tras_drv_info.voc_info.mic_en = mic_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;

voc_ctrl_mic_fail:
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_stop();
		bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
	}

	return err;
}

static bk_err_t aud_tras_drv_voc_ctrl_spk(aud_intf_voc_spk_ctrl_t spk_en)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	uint32_t size = 0;
	uint8_t *pcm_data = NULL;

	GLOBAL_INT_DECLARATION();

	if (spk_en == AUD_INTF_VOC_SPK_OPEN) {
		pcm_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (pcm_data == NULL) {
			LOGE("%s, %d, malloc temp pcm_data fial \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto voc_ctrl_spk_fail;
		} else {
			os_memset(pcm_data, 0x00, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			LOGI("%s, %d, open onboard spk \n", __func__, __LINE__);
			/* enable dac */
			bk_aud_dac_start();
			aud_tras_dac_pa_ctrl(true, true);

			ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto voc_ctrl_spk_fail;
			}
		} else {
			LOGI("%s, %d, open uac spk \n", __func__, __LINE__);
			ret = bk_aud_uac_start_spk();
			if (ret != BK_OK) {
				LOGE("%s, %d, open uac spk fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto voc_ctrl_spk_fail;
			}
			uac_spk_write_flag = false;
		}

		/* write two frame data to speaker and ref ring buffer */
		// bk_printf("write speaker 8\r\n"); // 无
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.mic_samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto voc_ctrl_spk_fail;
		}

		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.mic_samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto voc_ctrl_spk_fail;
		}

		audio_tras_drv_free(pcm_data);
		pcm_data = NULL;
	} else if (spk_en == AUD_INTF_VOC_SPK_CLOSE) {
		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			LOGI("%s, %d, open onboard spk \n", __func__, __LINE__);
			aud_tras_dac_pa_ctrl(false, true);
			bk_aud_dac_stop();
			bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		} else {
			LOGI("%s, %d, close uac spk \n", __func__, __LINE__);
			ret = bk_aud_uac_stop_spk();
			if (ret != BK_OK) {
				LOGE("%s, %d, close uac spk fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto voc_ctrl_spk_fail;
			}
		}
		ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto voc_ctrl_spk_fail;
	}

	GLOBAL_INT_DISABLE();
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
		aud_tras_drv_info.voc_info.spk_en = spk_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;

voc_ctrl_spk_fail:
	if (pcm_data)
		audio_tras_drv_free(pcm_data);

	if (spk_en == AUD_INTF_VOC_SPK_OPEN) {
		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			aud_tras_dac_pa_ctrl(false, true);
			bk_aud_dac_stop();
			bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		} else {
			bk_aud_uac_stop_spk();
		}
	}

	return err;
}

static bk_err_t aud_tras_drv_voc_ctrl_aec(bool aec_en)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.voc_info.aec_enable = aec_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_set_aec_para(aud_intf_voc_aec_ctl_t *aec_ctl)
{
	switch (aec_ctl->op) {
		case AUD_INTF_VOC_AEC_MIC_DELAY: /*  */
			aud_tras_drv_info.voc_info.aec_info->aec_config->mic_delay = aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, aud_tras_drv_info.voc_info.aec_info->aec_config->mic_delay);
			break;

		case AUD_INTF_VOC_AEC_EC_DEPTH: /* AEC 强度 */
			aud_tras_drv_info.voc_info.aec_info->aec_config->ec_depth = aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_EC_DEPTH, aud_tras_drv_info.voc_info.aec_info->aec_config->ec_depth);
			break;

		case AUD_INTF_VOC_AEC_REF_SCALE:
			aud_tras_drv_info.voc_info.aec_info->aec_config->ref_scale = (uint8_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_REF_SCALE, aud_tras_drv_info.voc_info.aec_info->aec_config->ref_scale);
			break;

		case AUD_INTF_VOC_AEC_VOICE_VOL:
			aud_tras_drv_info.voc_info.aec_info->aec_config->voice_vol = (uint8_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_VOL, aud_tras_drv_info.voc_info.aec_info->aec_config->voice_vol);
			break;

#if CONFIG_AEC_VERSION_V1
		case AUD_INTF_VOC_AEC_TXRX_THR:
			aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxThr = aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_TxRxThr, aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxThr);
			break;

		case AUD_INTF_VOC_AEC_TXRX_FLR:
			aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxFlr = aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_TxRxFlr, aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxFlr);
			break;
#endif

		case AUD_INTF_VOC_AEC_NS_LEVEL:
			aud_tras_drv_info.voc_info.aec_info->aec_config->ns_level = (uint8_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_NS_LEVEL, aud_tras_drv_info.voc_info.aec_info->aec_config->ns_level);
			break;

		case AUD_INTF_VOC_AEC_NS_PARA:
			aud_tras_drv_info.voc_info.aec_info->aec_config->ns_para = (uint8_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_NS_PARA, aud_tras_drv_info.voc_info.aec_info->aec_config->ns_para);
			break;

		case AUD_INTF_VOC_AEC_DRC:
			aud_tras_drv_info.voc_info.aec_info->aec_config->drc = (uint8_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_DRC, aud_tras_drv_info.voc_info.aec_info->aec_config->drc);
			break;

		case AUD_INTF_VOC_AEC_INIT_FLAG:
			aud_tras_drv_info.voc_info.aec_info->aec_config->init_flags = (uint16_t)aec_ctl->value;
			aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_FLAGS, aud_tras_drv_info.voc_info.aec_info->aec_config->init_flags);
			break;

		default:
			break;
	}

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_get_aec_para(void)
{
	LOGI("aud_intf aec params: \n");
	LOGI("init_flags: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->init_flags);
	LOGI("ec_depth: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->ec_depth);
	LOGI("ref_scale: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->ref_scale);
	LOGI("voice_vol: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->voice_vol);
	LOGI("TxRxThr: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxThr);
	LOGI("TxRxFlr: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxFlr);
	LOGI("ns_level: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->ns_level);
	LOGI("ns_para: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->ns_para);
	LOGI("drc: %d \n", aud_tras_drv_info.voc_info.aec_info->aec_config->drc);
	LOGI("end \n");

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_voc_tx_debug(aud_intf_dump_data_callback callback)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.voc_info.aud_tras_dump_tx_cb = callback;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_voc_rx_debug(aud_intf_dump_data_callback callback)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.voc_info.aud_tras_dump_rx_cb = callback;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_voc_aec_debug(aud_intf_dump_data_callback callback)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.voc_info.aud_tras_dump_aec_cb = callback;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_uac_register_connect_state_cb(void * cb)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.uac_connect_state_cb_exist = true;
	aud_tras_drv_info.aud_tras_drv_uac_connect_state_cb = cb;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_spk_set_samp_rate(uint32_t samp_rate)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;

	aud_tras_dac_pa_ctrl(false, true);
	bk_aud_dac_stop();
	ret = bk_aud_dac_set_samp_rate(samp_rate);
	bk_aud_dac_start();
	bk_aud_dac_start();
	aud_tras_dac_pa_ctrl(true, true);

	return ret;
}



bk_err_t aud_tras_drv_set_work_mode(aud_intf_work_mode_t mode)
{
	bk_err_t ret = BK_OK;

	LOGI("%s, %d, set mode: %d \n", __func__, __LINE__, mode);

	switch (mode) {
		case AUD_INTF_WORK_MODE_GENERAL:
			if ((aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) && (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL)) {
				ret = aud_tras_drv_voc_deinit();
				if (ret != BK_OK) {
					LOGE("%s, %d, spk deinit fail \n", __func__, __LINE__);
					return BK_ERR_AUD_INTF_FAIL;
				}
			}
			aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_GENERAL;
			break;

		case AUD_INTF_WORK_MODE_VOICE:
			if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
				if (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_NULL) {
					ret = aud_tras_drv_mic_deinit();
					if (ret != BK_OK) {
						LOGE("%s, %d, mic deinit fail \n", __func__, __LINE__);
						return BK_ERR_AUD_INTF_FAIL;
					}
				}
				if (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_NULL) {
					ret = aud_tras_drv_spk_deinit();
					if (ret != BK_OK) {
						LOGE("%s, %d, spk deinit fail \n", __func__, __LINE__);
						return BK_ERR_AUD_INTF_FAIL;
					}
				}
			}
			aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_VOICE;
			break;

		case AUD_INTF_WORK_MODE_NULL:
			if ((aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) && (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL)) {
				ret = aud_tras_drv_voc_deinit();
				if (ret != BK_OK) {
					LOGE("%s, %d, spk deinit fail \n", __func__, __LINE__);
					return BK_ERR_AUD_INTF_FAIL;
				}
			}
			if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
				if (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_NULL) {
					ret = aud_tras_drv_mic_deinit();
					if (ret != BK_OK) {
						LOGE("%s, %d, mic deinit fail \n", __func__, __LINE__);
						return BK_ERR_AUD_INTF_FAIL;
					}
				}
				if (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_NULL) {
					ret = aud_tras_drv_spk_deinit();
					if (ret != BK_OK) {
						LOGE("%s, %d, spk deinit fail \n", __func__, __LINE__);
						return BK_ERR_AUD_INTF_FAIL;
					}
				}
			}
			aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_NULL;
			break;

		default:
			return BK_ERR_AUD_INTF_FAIL;
			break;
	}

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_set_mic_gain(uint8_t value)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;

	if (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_BOARD || aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_set_gain((uint32_t)value);
		ret = BK_ERR_AUD_INTF_OK;
	}

	return ret;
}

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS // no
static void uac_mic_spk_recover(void)
{
	LOGD("%s \n", __func__);
	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
		/* check mic status */
		if ((aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_START) && (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC)) {
			bk_aud_uac_start_mic();
		}

		/* check spk status */
		if ((aud_tras_drv_info.spk_info.status == AUD_TRAS_DRV_SPK_STA_START) && (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC)) {
			bk_aud_uac_start_spk();
		}
	} else {
		if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
			if ((aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) && (aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN)) {
				bk_aud_uac_start_mic();
			}
			if ((aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) && (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_OPEN)) {
				bk_aud_uac_start_spk();
			}
		}
	}
}
#endif //CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS

static bk_err_t aud_tras_drv_set_spk_gain(uint16_t value)
{
	bk_err_t ret = BK_OK;
	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		bk_aud_dac_set_gain((uint32_t)value);

        if (value == 0) {
            aud_tras_dac_pa_ctrl(false, true);
        } else {
            aud_tras_dac_pa_ctrl(true, true);
        }

        if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
            aud_tras_drv_info.spk_info.dac_config->dac_gain = value;
        } else {
            aud_tras_drv_info.voc_info.dac_config->dac_gain = value;
        }

		return BK_ERR_AUD_INTF_OK;
	}

	if (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		/* check uac support volume configuration */
		if (bk_aud_uac_check_spk_gain_cfg() == BK_OK) {
			LOGI("%s, %d, set uac speaker volume: %d \n", __func__, __LINE__, value);
#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS
			/* step1: stop uac mic and speaker
			   step2: set volume
			   step3: recover uac mic and speaker status
			*/
			/* step1: stop uac mic and speaker */
			bk_aud_uac_stop_mic();
			bk_aud_uac_stop_spk();

			/* step2: set volume */
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);

			/* step3: recover uac mic and speaker status */
			uac_mic_spk_recover();
#endif

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_DIRECT
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);
#endif

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_MUTE
			bk_aud_uac_ctrl_spk_mute(1);
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);
			bk_aud_uac_ctrl_spk_mute(0);
#endif

			return ret;
		} else {
			LOGW("%s, %d, The uac speaker not support volume configuration \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_PARAM;
		}
	}

	return ret;
}

// XXX 所有语音功能都得走这里确认是播放哪段音频
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
static bk_err_t aud_tras_drv_play_prompt_tone(aud_intf_voc_prompt_tone_t prompt_tone)
{
    bool play_flag = true;
    bk_err_t ret = BK_FAIL;

/*
 * 如果需要新增语音 请增加枚举 然后这里新增一个case指向 新语音地址
 */
    switch (prompt_tone)
    {
        case AUD_INTF_VOC_ASR_WAKEUP: 
            LOGI("[prompt_tone] ASR_WAKEUP\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = asr_wakeup_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY // .c文件存储的方式
            prompt_tone_info.url = (char *)asr_wakeup_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(asr_wakeup_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_ASR_STANDBY:
            LOGI("[prompt_tone] ASR_STANDBY\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = asr_standby_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)asr_standby_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(asr_standby_prompt_tone_array);
#endif
            break;
#if I4S_RECORD_REPEAT_MODE
        case AUD_INTF_VOC_ASR_OPEN_RECORD: /* NOTE 录音开始音频 */
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = asr_start_record_prompt_tone_path;
#endif
        	break;
        case AUD_INTF_VOC_ASR_CLOSE_RECORD: /* NOTE 录音停止音频 */
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = asr_stop_record_prompt_tone_path;
#endif
        	break;
#endif
        case AUD_INTF_VOC_NETWORK_PROVISION:
            LOGI("[prompt_tone] NETWORK_PROVISION\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = network_provision_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)network_provision_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(network_provision_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_NETWORK_PROVISION_SUCCESS:
            LOGI("[prompt_tone] NETWORK_PROVISION_SUCCESS\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = network_provision_success_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)network_provision_success_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(network_provision_success_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_NETWORK_PROVISION_FAIL:
            LOGI("[prompt_tone] NETWORK_PROVISION_FAIL\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = network_provision_fail_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)network_provision_fail_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(network_provision_fail_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_RECONNECT_NETWORK:
            LOGI("[prompt_tone] RECONNECT_NETWORK\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = reconnect_network_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)reconnect_network_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(reconnect_network_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_RECONNECT_NETWORK_SUCCESS:
        	bk_printf("reconnect network success back\r\n");
            LOGI("[prompt_tone] RECONNECT_NETWORK_SUCCESS\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = reconnect_network_success_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)reconnect_network_success_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(reconnect_network_success_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_RECONNECT_NETWORK_FAIL:
            LOGI("[prompt_tone] RECONNECT_NETWORK_FAIL\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = reconnect_network_fail_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)reconnect_network_fail_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(reconnect_network_fail_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_RTC_CONNECTION_LOST:
            LOGI("[prompt_tone] CONNECTION_LOST\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = rtc_connection_lost_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)rtc_connection_lost_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(rtc_connection_lost_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_AGENT_JOINED:
            LOGI("[prompt_tone] AGENT_JOINED\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = agent_joined_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)agent_joined_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(agent_joined_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_AGENT_OFFLINE:
            LOGI("[prompt_tone] AGENT_OFFLINE\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = agent_offline_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)agent_offline_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(agent_offline_prompt_tone_array);
#endif
            break;

        case AUD_INTF_VOC_LOW_VOLTAGE:
            LOGI("[prompt_tone] LOW_VOLTAGE\n");
#if CONFIG_PROMPT_TONE_SOURCE_VFS
            prompt_tone_info.url = low_voltage_prompt_tone_path;
#endif
#if CONFIG_PROMPT_TONE_SOURCE_ARRAY
            prompt_tone_info.url = (char *)low_voltage_prompt_tone_array;
            prompt_tone_info.total_len = sizeof(low_voltage_prompt_tone_array);
#endif
            break;

        default:
            LOGE("%s, %d, prompt_tone: %d not support fail\n", __func__, __LINE__, prompt_tone);
            play_flag = false;
            break;
    }

    if (play_flag)
    {
    	bk_printf("set volume2(ensure which wav)\r\n");
        ret = aud_tras_drv_send_msg(AUD_TRAS_PLAY_PROMPT_TONE, (void *)&prompt_tone_info);
        if (ret != BK_OK)
        {
            LOGE("%s, %d, send tras play prompt tone fail\n", __func__, __LINE__);
        }
    }
    else
    {
        ret = BK_FAIL;
    }

    return ret;
}
#endif

// NOTE  线程函数  内容发送来自 audio_event_handle  
// aud_tras_drv_send_msg 由这个发送过来 （往队列中发）
static void aud_tras_drv_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
	aud_intf_drv_config_t *aud_trs_setup = NULL;

	aud_trs_setup = (aud_intf_drv_config_t *)param_data;

	aud_tras_drv_info.work_mode = aud_trs_setup->setup.work_mode;
	aud_tras_drv_info.aud_tras_tx_mic_data = aud_trs_setup->setup.aud_intf_tx_mic_data;
	aud_tras_drv_info.aud_tras_rx_spk_data = aud_trs_setup->setup.aud_intf_rx_spk_data;

	/* set work status to IDLE */
	aud_tras_drv_info.status = AUD_TRAS_DRV_STA_IDLE;

	rtos_set_semaphore(&aud_tras_drv_task_sem); // 释放

//	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_320M);

	while(1) {
		aud_tras_drv_msg_t msg;
		media_mailbox_msg_t *mailbox_msg = NULL;
		//GPIO_UP(5);
		/* 出队列 查看事件类型 */
		/* NOTE 无休眠 就是不停的请求队列中的数据 */
		ret = rtos_pop_from_queue(&aud_trs_drv_int_msg_que, &msg, BEKEN_WAIT_FOREVER);

		if (kNoErr == ret) {
			// bk_printf("get msg.op is %d\r\n", msg.op);
			switch (msg.op) {
				case AUD_TRAS_DRV_IDLE:
					break;

				case AUD_TRAS_DRV_EXIT:
					LOGD("%s, %d, goto: AUD_TRAS_DRV_EXIT \n", __func__, __LINE__);
					goto aud_tras_drv_exit;
					break;

				case AUD_TRAS_DRV_SET_MODE:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_work_mode((aud_intf_work_mode_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* mic op */
				case AUD_TRAS_DRV_MIC_INIT: // NOTE
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_init((aud_intf_mic_config_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_DEINIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_deinit();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_START:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_start();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_PAUSE:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_pause();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_STOP:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_stop();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_SET_CHL:  /* NOTE 设置单次采样数据 */
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_set_chl((aud_intf_mic_chl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_SET_SAMP_RATE:
					bk_printf("sample rate is %d\r\n", (uint32_t)(msg.param));
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_mic_set_samp_rate(mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_SET_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_mic_gain(*((uint8_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_MIC_TX_DATA:
					aud_tras_drv_mic_tx_data(); /* 获取 mic data */
					break;

				/* spk op */
				case AUD_TRAS_DRV_SPK_INIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_init((aud_intf_spk_config_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_DEINIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_deinit();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_START:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_start();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_PAUSE:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_pause();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_STOP:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_stop();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_SET_CHL:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_set_chl((aud_intf_spk_chl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_SET_SAMP_RATE:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_spk_set_samp_rate(mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_SET_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_spk_gain(*((uint16_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_SPK_REQ_DATA:
					aud_tras_drv_spk_req_data((audio_packet_t *)msg.param);
					break;

				/* voc op */
				case AUD_TRAS_DRV_VOC_INIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_init((aud_intf_voc_config_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_DEINIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					if (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL) {
						ret = aud_tras_drv_voc_deinit();
					} else {
						ret = BK_ERR_AUD_INTF_OK;
					}
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_START:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_start();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_STOP:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_stop();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_MIC:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_mic((aud_intf_voc_mic_ctrl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_SPK:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_spk((aud_intf_voc_spk_ctrl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_AEC:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_aec((bool)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_MIC_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_mic_gain(*((uint8_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_SPK_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_spk_gain(*((uint16_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_AEC_PARA:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_aec_para((aud_intf_voc_aec_ctl_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_GET_AEC_PARA:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_get_aec_para();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_TX_DEBUG:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_tx_debug((aud_intf_dump_data_callback)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_RX_DEBUG:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_rx_debug((aud_intf_dump_data_callback)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_AEC_DEBUG: // 注册 AEC 回调函数 
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_aec_debug((aud_intf_dump_data_callback)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* uac op */
				case AUD_TRAS_DRV_UAC_REGIS_CONT_STATE_CB:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_uac_register_connect_state_cb((void *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_UAC_CONT: /* NOTE aud_tras_uac_connect_cb 入队列 走此处 */
					aud_tras_uac_connect_handle();
					break;

				case AUD_TRAS_DRV_UAC_DISCONT:
					aud_tras_uac_disconnect_handle();
					break;

				case AUD_TRAS_DRV_UAC_AUTO_CONT_CTRL:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_uac_auto_connect_ctrl((bool)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* voc int op */
				/* REVIEW adc、dma 中断函数通知 */
				case AUD_TRAS_DRV_AEC: /* NOTE dma、adc -> 采集一帧语音数据 通知 */
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						// bk_printf("aec work 1\r\n"); /* NOTE 循环执行 */
						aud_tras_aec(); // NOTE
					}
					break;

				case AUD_TRAS_DRV_ENCODER: /* NOTE 编码 aec处理结束 通知 */
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						// bk_printf("aec work 2\r\n"); /* NOTE 循环执行 */
						aud_tras_enc(); // NOTE
					}
					break;

				/* TODO 下面的函数 移除会导致喇叭根本无法出声音 */
				case AUD_TRAS_DRV_DECODER: /* NOTE 解码 dac、dma 中断函数通知 */
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						// bk_printf("aec work 3\r\n");  /* NOTE 循环执行 */
						aud_tras_dec(); // NOTE
					}
					break;

                case AUD_TRAS_ASR_WAKEUP_IND:
                    LOGD("AUD_TRAS_ASR_WAKEUP_IND\n"); /* NOTE 唤醒 */
                    if (BK_OK != msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_WAKEUP_IND, APP_MODULE, 1, NULL))
                    {
                        LOGE("%s, %d, send asr wakeup fail\n", __func__, __LINE__);
                    }
                    break;
                case AUD_TRAS_ASR_STANDBY_IND:
                    LOGD("AUD_TRAS_ASR_STANDBY_IND\n"); /* NOTE 休眠 */
                    if (BK_OK != msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_STANDBY_IND, APP_MODULE, 1, NULL))
                    {
                        LOGE("%s, %d, send asr standby fail\n", __func__, __LINE__);
                    }
                    break;
                case AUD_TRAS_RECEIVE_ADDRESS:
                	msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_RECEIVE_ADDRESS, APP_MODULE, (uint32_t)i4s_aud_rx_rb, NULL);
                	break;
#if I4S_RECORD_REPEAT_MODE /* REVIEW 录音模式 */
          		case AUD_TRAS_OPEN_RECORD_MODE:
          			msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_OPEN_RECORD_IND, APP_MODULE, 1, NULL);
          			break;
          		case AUD_TRAS_CLOSE_RECORD_MODE:
          			msg_send_req_to_media_major_mailbox_sync(EVENT_ASR_CLOSE_RECORD_IND, APP_MODULE, 1, NULL);
          			break;
#endif
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
                case AUD_TRAS_PLAY_PROMPT_TONE:
                	// 先 AUD_TRAS_PLAY_PROMPT_TONE_REQ 再 AUD_TRAS_PLAY_PROMPT_TONE 就是有语音才对
                	bk_printf("ready to play audio\r\n"); // 有
                    LOGD("AUD_TRAS_PLAY_PROMPT_TONE\n");
                    // 播放音频
                    AUD_PLAY_PROMPT_TONE_START();

                    /*暂停未播放完的 设置新的 播放新的*/
                    prompt_tone_play_stop(gl_prompt_tone_play_handle);
                    prompt_tone_play_set_url(gl_prompt_tone_play_handle, (url_info_t *)msg.param);
                    prompt_tone_play_start(gl_prompt_tone_play_handle); /*  */

                    AUD_PLAY_PROMPT_TONE_END();

                    break;

                case AUD_TRAS_STOP_PROMPT_TONE:
                    LOGD("AUD_TRAS_STOP_PROMPT_TONE\n");

                    AUD_STOP_PROMPT_TONE_START();

                    //prompt_tone_play_stop(gl_prompt_tone_play_handle);

                    AUD_STOP_PROMPT_TONE_END();

                    break;

                case AUD_TRAS_PLAY_PROMPT_TONE_REQ: // XXX
                {
                	// XXX wakeup aha~ （所有语音处理全在这里面）
                    mailbox_msg = (media_mailbox_msg_t *)msg.param;
                    bk_printf("ready get audio\r\n");
                    // prompt_tone_info 全局变量更改了
                    ret = aud_tras_drv_play_prompt_tone((aud_intf_voc_prompt_tone_t)mailbox_msg->param);
                    msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
                    break;
                }

                case AUD_TRAS_STOP_PROMPT_TONE_REQ:
                    ret = aud_tras_drv_send_msg(AUD_TRAS_STOP_PROMPT_TONE, NULL);
                    if (ret != BK_OK)
                    {
                        LOGE("%s, %d, send tras stop prompt tone fail\n", __func__, __LINE__);
                    }
                    msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
                    break;
#endif

#if CONFIG_AUD_INTF_SUPPORT_MULTIPLE_SPK_SOURCE_TYPE
                case AUD_TRAS_SET_SPK_SOURCE_TYPE:
                    aud_tras_drv_set_spk_source_type((spk_source_type_t)msg.param);
                    break;
#endif

				default:
					break;
			}
			//GPIO_DOWN(5);
		}
	}

aud_tras_drv_exit:
	/* deinit mic, speaker and voice */
	/* check audio transfer driver work status */
	switch (aud_tras_drv_info.work_mode) {
		case AUD_INTF_WORK_MODE_GENERAL:
			/* check mic work status */
			if (aud_tras_drv_info.mic_info.status != AUD_TRAS_DRV_MIC_STA_NULL) {
				/* stop mic and deinit */
				aud_tras_drv_mic_stop();
				aud_tras_drv_mic_deinit();
			}
			/* check speaker work status */
			if (aud_tras_drv_info.spk_info.status != AUD_TRAS_DRV_SPK_STA_NULL) {
				/* stop speaker and deinit */
				aud_tras_drv_spk_stop();
				aud_tras_drv_spk_deinit();
			}
			break;

		case AUD_INTF_WORK_MODE_VOICE:
			/* check voice work status */
			if (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL) {
				/* stop voice transfer and deinit */
				aud_tras_drv_voc_stop();
				aud_tras_drv_voc_deinit();
			}
			break;

		default:
			break;
	}

	aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_NULL;
	aud_tras_drv_info.aud_tras_tx_mic_data = NULL;
	aud_tras_drv_info.aud_tras_rx_spk_data = NULL;

	/* set work status to NULL */
	aud_tras_drv_info.status = AUD_TRAS_DRV_STA_NULL;


	/* delete msg queue */
	ret = rtos_deinit_queue(&aud_trs_drv_int_msg_que);
	if (ret != kNoErr) {
		LOGE("%s, %d, delete message queue fail \n", __func__, __LINE__);
	}
	aud_trs_drv_int_msg_que = NULL;
	LOGI("%s, %d, delete message queue complete \n", __func__, __LINE__);

#if AUD_MEDIA_SEM_ENABLE
	/* deinit semaphore used to  */
	if (mailbox_media_aud_mic_sem) {
		rtos_deinit_semaphore(&mailbox_media_aud_mic_sem);
		mailbox_media_aud_mic_sem = NULL;
	}
#endif
//	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);

	/* reset uac to default */
	aud_tras_drv_info.aud_tras_drv_uac_connect_state_cb = NULL;
	aud_tras_drv_info.uac_status = AUD_INTF_UAC_NORMAL_DISCONNECTED;
	aud_tras_drv_info.uac_auto_connect = true;

	rtos_set_semaphore(&aud_tras_drv_task_sem);

	/* delete task */
	aud_trs_drv_thread_hdl = NULL;
	rtos_delete_thread(NULL);
}

aud_intf_drv_config_t aud_trs_drv_setup_bak = {0};

// XXX
bk_err_t aud_tras_drv_init(aud_intf_drv_config_t *setup_cfg)
{
	bk_err_t ret = BK_OK;

#if AUD_MEDIA_SEM_ENABLE
	/* init semaphore used to  */
	if (!mailbox_media_aud_mic_sem) {
		ret = rtos_init_semaphore(&mailbox_media_aud_mic_sem, 1);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, create mailbox audio mic semaphore failed \n", __func__, __LINE__);
			goto fail;
		}
	}
#endif

	if (aud_tras_drv_task_sem == NULL) {
		ret = rtos_init_semaphore(&aud_tras_drv_task_sem, 1);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, create audio tras drv task semaphore failed \n", __func__, __LINE__);
			goto fail;
		}
	}

	if ((!aud_trs_drv_thread_hdl) && (!aud_trs_drv_int_msg_que)) {
		LOGD("%s, %d, init audio transfer driver \n", __func__, __LINE__);
		os_memcpy(&aud_trs_drv_setup_bak, setup_cfg, sizeof(aud_intf_drv_config_t));

		ret = rtos_init_queue(&aud_trs_drv_int_msg_que,
							  "aud_tras_int_que",
							  sizeof(aud_tras_drv_msg_t),
							  TU_QITEM_COUNT);
		if (ret != kNoErr) {
			LOGE("%s, %d, create audio transfer driver internal message queue fail \n", __func__, __LINE__);
			goto fail;
		}
		LOGD("%s, %d, create audio transfer driver internal message queue complete \n", __func__, __LINE__);

		//create audio transfer driver task
		// XXX
		ret = rtos_create_thread(&aud_trs_drv_thread_hdl,
							 setup_cfg->setup.task_config.priority,
							 "aud_tras_drv",
							 (beken_thread_function_t)aud_tras_drv_main,
							 4096,
							 (beken_thread_arg_t)&aud_trs_drv_setup_bak);
		if (ret != kNoErr) {
			LOGE("%s, %d, create audio transfer driver task fail \n", __func__, __LINE__);
			rtos_deinit_queue(&aud_trs_drv_int_msg_que);
			aud_trs_drv_int_msg_que = NULL;
			aud_trs_drv_thread_hdl = NULL;
			goto fail;
		}

		ret = rtos_get_semaphore(&aud_tras_drv_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			goto fail;
		}

		LOGD("%s, %d, create audio transfer driver task complete \n", __func__, __LINE__);
	}

	return BK_OK;

fail:
	//LOGE("%s, %d, aud_tras_drv_init fail, ret: %d \n", __func__, __LINE__, ret);

	if(aud_tras_drv_task_sem)
	{
		rtos_deinit_semaphore(&aud_tras_drv_task_sem);
		aud_tras_drv_task_sem = NULL;
	}

	return BK_FAIL;
}

bk_err_t aud_tras_drv_deinit(void)
{
	bk_err_t ret;
	aud_tras_drv_msg_t msg;

	msg.op = AUD_TRAS_DRV_EXIT;
	if (aud_trs_drv_int_msg_que) {
		ret = rtos_push_to_queue_front(&aud_trs_drv_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, audio send msg: AUD_TRAS_DRV_EXIT fail \n", __func__, __LINE__);
			return kOverrunErr;
		}

		ret = rtos_get_semaphore(&aud_tras_drv_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			return BK_FAIL;
		}

		if(aud_tras_drv_task_sem)
		{
			rtos_deinit_semaphore(&aud_tras_drv_task_sem);
			aud_tras_drv_task_sem = NULL;
		}
	} else {
		LOGW("%s, %d, aud_trs_drv_int_msg_que is NULL\n", __func__, __LINE__);
	}

	return BK_OK;
}

#if CONFIG_AUD_INTF_SUPPORT_AI_DIALOG_FREE
bk_err_t aud_tras_drv_register_aec_ouput_callback(aud_tras_drv_aec_output_callback cb, void *user_data)
{
    gl_aec_output_callback = cb; // aec_output_callback
    gl_user_data = user_data;

    return BK_OK;
}

/* NOTE 唤醒词 决断位置 */
bk_err_t aud_tras_drv_set_dialog_run_state_by_asr_result(uint32_t asr_result)
{

#if I4S_RECORD_REPEAT_MODE
    if (asr_result == RECORD_REPEAT_START) { /* 录音模式 这里结合本地的 VAD 使用 */
        bk_printf("open record mode\r\n");
        i4s_record_flag = 1;
        gl_dialog_running = true;
        aud_tras_drv_send_msg(AUD_TRAS_OPEN_RECORD_MODE, NULL);
        aud_tras_dac_pa_ctrl(false, false); /* 打开DAC的PA 引脚*/
        return BK_OK;
    } else if (asr_result == RECORD_REPEAT_STOP) {
    	bk_printf("close record mode\r\n");
    	gl_dialog_running = false;
    	i4s_record_flag = 0;
    	aud_tras_drv_send_msg(AUD_TRAS_CLOSE_RECORD_MODE, NULL);
    	aud_tras_dac_pa_ctrl(false, false); /*关闭 DAC的PA 引脚*/
    	return BK_OK;
    }
#endif
    if (asr_result == HI_ARMINO) {
#if 0
        gl_dialog_running = true;
        LOGI("%s \n", "hi armino ");

        aud_tras_dac_pa_ctrl(true, false); /* 打开DAC的PA 引脚*/

        if (aud_tras_drv_send_msg(AUD_TRAS_ASR_WAKEUP_IND, NULL) != BK_OK)
        {
            LOGE("%s, %d, send tras asr wakeup fail\n", __func__, __LINE__);
        }
#endif
    } else if (asr_result == BYEBYE_ARMINO) {
#if 0
        gl_dialog_running = false;
        LOGI("%s \n", "byebye armino ");

#if CONFIG_AUD_INTF_SUPPORT_MULTIPLE_SPK_SOURCE_TYPE
        if (aud_tras_drv_get_spk_source_type() == SPK_SOURCE_TYPE_VOICE)
        {
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
            /* Do not close PA because the prompt tone is about to be played, which will reopen PA.
                However, PA opening requires a stabilization time, and frequent switching will cause some prompt tones to be lost.
             */
#else
            aud_tras_dac_pa_ctrl(false, false); /*关闭 DAC的PA 引脚*/
#endif
        }
#endif

        if (aud_tras_drv_send_msg(AUD_TRAS_ASR_STANDBY_IND, NULL) != BK_OK)
        {
            LOGE("%s, %d, send tras asr wakeup fail\n", __func__, __LINE__);
        }
#endif
    } else {
    	//
    }

    return BK_OK;
}
#endif


#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
int aud_tras_drv_read_prompt_tone_data(char *buffer, uint32_t len, uint32_t timeout)
{
    if (gl_prompt_tone_rb) {
        return rb_read(gl_prompt_tone_rb, buffer, len, timeout);
    } else {
        return BK_FAIL;
    }
}

int aud_tras_drv_write_prompt_tone_data(char *buffer, uint32_t len, uint32_t timeout)
{
    if (gl_prompt_tone_rb) {
        return rb_write(gl_prompt_tone_rb, buffer, len, timeout);
    } else {
        return BK_FAIL;
    }
}

bk_err_t aud_tras_drv_register_prompt_tone_pool_empty_notify(prompt_tone_pool_empty_notify notify, void *user_data)
{
    gl_prompt_tone_empty_notify = notify;
    gl_notify_user_data = user_data;

    return BK_OK;
}
#endif

#if CONFIG_AI_ASR_MODE_CPU2
void aud_cp2_ready_notify(void)
{
	if (gl_aud_cp2_ready_sem)
	{
		rtos_set_semaphore(&gl_aud_cp2_ready_sem);
	}
}
#endif

#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
static bk_err_t aud_tras_drv_close_voc_spk_source(void)
{
    LOGI("%s\n", __func__);

    /* stop dac and dac dma */
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
        /* disable dac */
        aud_tras_dac_pa_ctrl(false, true);
        bk_aud_dac_stop();
	} else {
        //not support
        //TODO
	}

    /* reconfig speaker ringbuffer and dma */
    if (aud_tras_drv_info.voc_info.speaker_ring_buff) {
        /* clear dac ring buffer */
        ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
        audio_tras_drv_free(aud_tras_drv_info.voc_info.speaker_ring_buff);
        aud_tras_drv_info.voc_info.speaker_ring_buff = NULL;
    }

    bk_dma_deinit(aud_tras_drv_info.voc_info.dac_dma_id);

    return BK_OK;
}

static bk_err_t aud_tras_drv_open_voc_spk_source(void)
{
    bk_err_t ret = BK_OK;

    LOGI("%s\n", __func__);

    /* recover speaker sample rate */
    bk_aud_dac_set_samp_rate(aud_tras_drv_info.voc_info.dac_config->samp_rate);

	aud_tras_drv_info.voc_info.speaker_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.voc_info.speaker_ring_buff == NULL) {
		LOGE("%s, %d, malloc speaker ring buffer fail \n", __func__, __LINE__);
		goto fail;
	}

    /* config audio dac dma to carry dac data to "speaker_ring_buff" */
    ret = aud_tras_dac_dma_config(aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2, AUD_INTF_SPK_CHL_LEFT);
    if (ret != BK_OK) {
        LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
		goto fail;
    }

    ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.dac_dma_id, RB_DMA_TYPE_READ);

    /* write two frame data to speaker and ref ring buffer */
    // bk_printf("write speaker 1\r\n"); // 无
    os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    int size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
        LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
		goto fail;
    }
    size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
    if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
        LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
		goto fail;
    }

    /* enable dac */
    bk_aud_dac_start();
    aud_tras_dac_pa_ctrl(true, true);

    ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
    if (ret != BK_OK) {
        LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
		goto fail;
    }

    return BK_OK;

fail:

    return BK_FAIL;
}


static bk_err_t aud_tras_drv_open_spk_a2dp_source(uint32_t sample_rate)
{
    bk_err_t ret = BK_OK;

    LOGI("%s\n", __func__);

    /* update speaker sample rate */
    bk_aud_dac_set_samp_rate(sample_rate);

    /* two frame(20ms data) */
    a2dp_frame_size = sample_rate * 2 * 20 / 1000;
    LOGD("%s, a2dp_frame_size: %d\n", __func__, a2dp_frame_size);
	aud_tras_drv_info.voc_info.speaker_ring_buff = (int32_t *)audio_tras_drv_malloc(a2dp_frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (!aud_tras_drv_info.voc_info.speaker_ring_buff) {
		LOGE("%s, %d, malloc speaker ring buffer: %d fail \n", __func__, __LINE__, a2dp_frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
        goto fail;
	}

	/* config audio dac dma to carry dac data to "speaker_ring_buff" */
	ret = aud_tras_dac_dma_config(aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, a2dp_frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, a2dp_frame_size, AUD_INTF_SPK_CHL_LEFT);
	if (ret != BK_OK) {
		LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
        goto fail;
	}
	ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, a2dp_frame_size * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.dac_dma_id, RB_DMA_TYPE_READ);

    /* start audio dac */
    if (a2dp_read_buff) {
        audio_tras_drv_free(a2dp_read_buff);
        a2dp_read_buff = NULL;
    }
    a2dp_read_buff = (int32_t *)audio_tras_drv_malloc(a2dp_frame_size);
    if (!a2dp_read_buff) {
        LOGE("%s, %d, malloc a2dp_read_buff: %d fail \n", __func__, __LINE__, a2dp_frame_size);
        goto fail;
    }
    os_memset(a2dp_read_buff, 0, a2dp_frame_size);
    /* write two frame data to speaker and ref ring buffer */
    // bk_printf("write speaker 2\r\n"); // 无
    int size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)a2dp_read_buff, a2dp_frame_size);
    if (size != a2dp_frame_size) {
        LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
        goto fail;
    }
    size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)a2dp_read_buff, a2dp_frame_size);
    if (size != a2dp_frame_size) {
        LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
        goto fail;
    }

    bk_aud_dac_start();
    aud_tras_dac_pa_ctrl(true, true);

    ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
    if (ret != BK_OK) {
        LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
        goto fail;
    }

    return BK_OK;

fail:

    return BK_FAIL;
}

static bk_err_t aud_tras_drv_close_spk_a2dp_source(void)
{
    LOGI("%s\n", __func__);

    /* recover speaker sample rate */
    bk_aud_dac_set_samp_rate(aud_tras_drv_info.voc_info.dac_config->samp_rate);

    /* stop dac and dac dma */
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
        /* disable dac */
        aud_tras_dac_pa_ctrl(false, true);
        bk_aud_dac_stop();
	} else {
        //not support
        //TODO
	}

    if (aud_tras_drv_info.voc_info.speaker_ring_buff) {
        /* clear dac ring buffer */
        ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
        audio_tras_drv_free(aud_tras_drv_info.voc_info.speaker_ring_buff);
        aud_tras_drv_info.voc_info.speaker_ring_buff = NULL;
    }

    bk_dma_deinit(aud_tras_drv_info.voc_info.dac_dma_id);

    if (a2dp_read_buff) {
        audio_tras_drv_free(a2dp_read_buff);
        a2dp_read_buff = NULL;
    }

    /* free ringbuffer */
    if (gl_prompt_tone_rb)
    {
        rb_reset(gl_prompt_tone_rb);
    }

    return BK_OK;
}
#endif

#if CONFIG_AUD_INTF_SUPPORT_MULTIPLE_SPK_SOURCE_TYPE
bk_err_t aud_tras_drv_voc_set_spk_source_type(spk_source_type_t type)
{
    if (aud_tras_drv_send_msg(AUD_TRAS_SET_SPK_SOURCE_TYPE, (void *)type) != BK_OK)
    {
        LOGE("%s, %d, send set spk source type fail\n", __func__, __LINE__);
    }

    return BK_OK;
}

static bk_err_t aud_tras_drv_set_spk_source_type(spk_source_type_t type)
{
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
    bk_err_t ret = BK_OK;
#endif

    if (spk_source_type == type)
    {
        LOGD("spk source type not need change\n");
        return BK_OK;
    }

    LOGI("%s, type: %d\n", __func__, type);

    AUD_SET_SPK_SOURCE_START(); /* 暂未开启 debug 空 */

    switch (type)
    {
        case SPK_SOURCE_TYPE_VOICE:
        {
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
            spk_source_type_t spk_source_type_old = spk_source_type;
            spk_source_type = type;
            if (spk_source_type_old == SPK_SOURCE_TYPE_A2DP)
            {
                /* stop a2dp music */
                LOGI("stop play a2dp music\n");
                aud_tras_drv_close_spk_a2dp_source();
                ret = aud_tras_drv_open_voc_spk_source();
                if (ret != BK_OK)
                {
                    LOGE("%s, %d, open voice speaker source fail, ret: %d\n", __func__, __LINE__, ret);
                    return BK_FAIL;
                }
            }
#else
            spk_source_type = type;
#endif
            /* check whether wakeup,
                wakeup: not close pa
                others: close pa
             */
            if (!gl_dialog_running) {
                aud_tras_dac_pa_ctrl(false, false);
            }
            break;
        }

        case SPK_SOURCE_TYPE_PROMPT_TONE:
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
            spk_source_type = type;
            aud_tras_dac_pa_ctrl(true, false);
#else
            LOGW("%s, SPK_SOURCE_TYPE_PROMPT_TONE not support, please enable CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE\n", __func__);
#endif
            break;

        case SPK_SOURCE_TYPE_A2DP:
#if CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP
            spk_source_type = type;
#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
            /* check whether playing prompt_tone */
            if (spk_source_type == SPK_SOURCE_TYPE_PROMPT_TONE)
            {
                /* stop prompt tone, and then change speaker source type to A2DP music */
                LOGI("stop play prompt tone\n");
                prompt_tone_play_stop(gl_prompt_tone_play_handle);
                rtos_delay_milliseconds(10);
                //wait prompt tone stop, and spk_source_type change to "SPK_SOURCE_TYPE_VOICE"
                //TODO
            }
#endif
            aud_tras_drv_close_voc_spk_source();
            ret = aud_tras_drv_open_spk_a2dp_source(A2DP_MUSIC_SAMPLE_RATE);
            if (ret != BK_OK)
            {
                LOGE("%s, %d, open a2dp speaker source fail\n", __func__, __LINE__);
                return BK_FAIL;
            }
#else
            LOGW("%s, SPK_SOURCE_TYPE_A2DP not support, please enable CONFIG_AUD_INTF_SUPPORT_BLUETOOTH_A2DP\n", __func__);
#endif
            break;

        default:
            LOGE("%s, %d, type: %d not support\n", __func__, __LINE__, type);
            return BK_FAIL;
            break;
    }

    AUD_SET_SPK_SOURCE_END(); /* 暂未开启 debug 空 */

    return BK_OK;
}


spk_source_type_t aud_tras_drv_get_spk_source_type(void)
{
    return spk_source_type; /* NOTE 默认值: SPK_SOURCE_TYPE_VOICE */
}
#endif

// NOTE 是 media_major_mailbox.c 来的 发送方式 mailbox_media_aud_send_msg
// 这里发  收的位置 => aud_tras_drv_main
bk_err_t audio_event_handle(media_mailbox_msg_t * msg)
{
	bk_err_t ret = BK_FAIL;

	/* save mailbox msg received from media app */
	LOGD("%s, %d, event: %d \n", __func__, __LINE__, msg->event);
	// bk_printf("audio_event_handle msg->event is %d\r\n", msg->event);

	switch (msg->event)
	{
		case EVENT_AUD_VOC_CLEAR_SPK_BUF_REQ: /* NOTE i4s */
			ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
			ret = BK_OK; /* NOTE 这里需要回复，否则会照成信号量死等 破坏结构 */
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
			break;
		case EVENT_AUD_INIT_REQ: // major_mailbox 中的 AUD_EVENT 来的
			// NOTE
			bk_printf("aud init\r\n");
			ret = aud_tras_drv_init((aud_intf_drv_config_t *)msg->param); // XXX
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
			break;

		case EVENT_AUD_DEINIT_REQ:
			// NOTE 
			bk_printf("aud deinit\r\n");
			ret = aud_tras_drv_deinit();
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
			break;

		case EVENT_AUD_SET_MODE_REQ:
			bk_printf("set workmode\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SET_MODE, (void *)msg);
			break;

		/* mic mode event  入唯一位置 bk_aud_intf_mic_init */
		case EVENT_AUD_MIC_INIT_REQ:
			bk_printf("mic init\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_INIT, (void *)msg);
			break;

		case EVENT_AUD_MIC_DEINIT_REQ:
			bk_printf("mic deinit\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_DEINIT, (void *)msg);
			break;

		case EVENT_AUD_MIC_START_REQ:
			bk_printf("mic start\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_START, (void *)msg);
			break;

		case EVENT_AUD_MIC_PAUSE_REQ:
			bk_printf("mic pause\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_PAUSE, (void *)msg);
			break;

		case EVENT_AUD_MIC_STOP_REQ:
			bk_printf("mic stop\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_STOP, (void *)msg);
			break;

		case EVENT_AUD_MIC_SET_CHL_REQ:
			bk_printf("set mic bit width\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_SET_CHL, (void *)msg);
			break;

		case EVENT_AUD_MIC_SET_SAMP_RATE_REQ:
			bk_printf("set sample rate\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_SET_SAMP_RATE, (void *)msg);
			break;

		case EVENT_AUD_MIC_SET_GAIN_REQ:
			bk_printf("set mic gain\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_MIC_SET_GAIN, (void *)msg);
			break;

		/* spk event */
		case EVENT_AUD_SPK_INIT_REQ:
			bk_printf("spk init\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_INIT, (void *)msg);
			break;

		case EVENT_AUD_SPK_DEINIT_REQ:
			bk_printf("spk deinit\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_DEINIT, (void *)msg);
			break;

		case EVENT_AUD_SPK_START_REQ:
			bk_printf("spk start\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_START, (void *)msg);
			break;

		case EVENT_AUD_SPK_PAUSE_REQ:
			bk_printf("spk pause\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_PAUSE, (void *)msg);
			break;

		case EVENT_AUD_SPK_STOP_REQ:
			bk_printf("spk stop\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_STOP, (void *)msg);
			break;

		case EVENT_AUD_SPK_SET_CHL_REQ:
			bk_printf("set spk bit width\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_SET_CHL, (void *)msg);
			break;

		case EVENT_AUD_SPK_SET_SAMP_RATE_REQ:
			bk_printf("set spk sample rate\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_SET_SAMP_RATE, (void *)msg);
			break;

		case EVENT_AUD_SPK_SET_GAIN_REQ:
			bk_printf("set spk gain\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SPK_SET_GAIN, (void *)msg);
			break;

		/* voc op */
		case EVENT_AUD_VOC_INIT_REQ: /* REVIEW 没有看到有 spk init 和 mic init 打印 */
			bk_printf("voc init\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_INIT, (void *)msg);
			break;

		case EVENT_AUD_VOC_DEINIT_REQ:
			bk_printf("voc deinit\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_DEINIT, (void *)msg);
			break;

		case EVENT_AUD_VOC_START_REQ: /* NOTE mic start 只是开启麦克风，如果需要录音还需要 voc start */
			bk_printf("voc start\r\n"); 
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_START, (void *)msg);
			break;

		case EVENT_AUD_VOC_STOP_REQ:
			bk_printf("voc stop\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_STOP, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_MIC_REQ: 
			bk_printf("voc ctrl mic\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_MIC, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_SPK_REQ:
			bk_printf("voc ctrl spk\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_SPK, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_AEC_REQ: /* NOTE 控制是否启动 回音消除 AEC */
			bk_printf("voc ctrl aec\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_AEC, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_MIC_GAIN_REQ:
			bk_printf("voc set mic gain\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_MIC_GAIN, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_SPK_GAIN_REQ:
			bk_printf("voc set spk gain\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_SPK_GAIN, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_AEC_PARA_REQ:
			bk_printf("voc set aec param\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_AEC_PARA, (void *)msg);
			break;

		case EVENT_AUD_VOC_GET_AEC_PARA_REQ:
			bk_printf("voc get aec param\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_GET_AEC_PARA, (void *)msg);
			break;

		case EVENT_AUD_VOC_TX_DEBUG_REQ:
			bk_printf("voc tx debug\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_TX_DEBUG, (void *)msg);
			break;

		case EVENT_AUD_VOC_RX_DEBUG_REQ:
			bk_printf("voc rx debug\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_RX_DEBUG, (void *)msg);
			break;

		case EVENT_AUD_VOC_AEC_DEBUG_REQ:
			bk_printf("aec debug\r\n");
			/*NOTE 注册AEC 回调函数 msg->param bk_aud_intf_voc_aec_debug */
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_AEC_DEBUG, (void *)msg);
			break;

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
        case EVENT_AUD_VOC_PLAY_PROMPT_TONE_REQ:
        	bk_printf("voc play prompt tone\r\n");
            aud_tras_drv_send_msg(AUD_TRAS_PLAY_PROMPT_TONE_REQ, (void *)msg);
            break;

        case EVENT_AUD_VOC_STOP_PROMPT_TONE_REQ:
        	bk_printf("voc stop prompt tone\r\n");
            aud_tras_drv_send_msg(AUD_TRAS_STOP_PROMPT_TONE_REQ, (void *)msg);
            break;
#endif

		/* NOTE uac event 入位置: bk_aud_intf_register_uac_connect_state_cb  */
		case EVENT_AUD_UAC_REGIS_CONT_STATE_CB_REQ:
			bk_printf("uac regis cont state cb\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_REGIS_CONT_STATE_CB, (void *)msg);
			break;
#if 0
		case EVENT_AUD_UAC_CONT_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_CONT, (void *)msg->param);
			break;

		case EVENT_AUD_UAC_DISCONT_REQ:
			//aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_CONT, NULL);
			break;
#endif
		case EVENT_AUD_UAC_AUTO_CONT_CTRL_REQ:
			bk_printf("uac auto cont ctrl\r\n");
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_AUTO_CONT_CTRL, (void *)msg);
			break;

		case EVENT_AUD_MIC_DATA_NOTIFY:
#if AUD_MEDIA_SEM_ENABLE
			ret = rtos_set_semaphore(&msg->sem);
			if (ret != BK_OK)
			{
				LOGE("%s semaphore set failed: %d\n", __func__, ret);
			}
#endif
			break;

		case EVENT_AUD_SPK_DATA_NOTIFY:
			//TODO set sem
			//	;
			break;

		default:
			break;
	}

	return ret;
}



