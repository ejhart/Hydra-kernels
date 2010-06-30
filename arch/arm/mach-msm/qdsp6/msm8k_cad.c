/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2009, HTC Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <mach/qdsp6/msm8k_cad_module.h>
#include <mach/qdsp6/msm8k_cad_q6dec_drv.h>
#include <mach/qdsp6/msm8k_cad_itypes.h>
#include <mach/qdsp6/msm8k_cad_volume.h>

#define MODULE_NAME "CAD"

#if 0
#define D(fmt, args...) printk(KERN_INFO "msm8k_cad: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define CAD_SESSION_INUSE 1
#define CAD_SESSION_FREE  0

#define CAD_SESSION_DEV_CTRL 1
#define CAD_SESSION_VOICE_TX 2
#define CAD_SESSION_VOICE_RX 3
#define CAD_SESSION_PCM 4
#define CAD_SESSION_PCM_IN 5
#define CAD_SESSION_FM 6

#if CONFIG_QSD_HTC_FM
#define CAD_SESSION_HW_CODEC_POOL_START 7
#define CAD_SESSION_HW_CODEC_POOL_END 9
#else
#define CAD_SESSION_HW_CODEC_POOL_START 6
#define CAD_SESSION_HW_CODEC_POOL_END 9
#endif


struct cad_session_info_struct_type {
	s8				status;
	struct mutex			sync;
	struct cad_func_tbl_type	*hw_accel;
};

struct cad_aux_pcm_gpios {
	int	dout;
	int	din;
	int	syncout;
	int	clkin_a;
};

struct cad_state_struct_type {
	struct mutex			sync;
	struct cad_func_tbl_type	*resource_alloc;
	struct cad_func_tbl_type	*volume;
	struct cad_func_tbl_type	*dtmf;
	struct cad_func_tbl_type	*equalizer;
	struct cad_func_tbl_type	*audiodec;
	struct cad_func_tbl_type	*audioenc;
	struct cad_func_tbl_type	*voicedec;
	struct cad_func_tbl_type	*voiceenc;
	struct cad_func_tbl_type	*device_filter;
	struct cad_func_tbl_type	*ard;

	struct cad_session_info_struct_type session_info[CAD_MAX_SESSION];
	struct cad_aux_pcm_gpios	aux_pcm;
};

struct cad_singleton_info_struct_type {
	u8	cad_ref_ct;
};


static DEFINE_SPINLOCK(slock);

static struct cad_state_struct_type cad;
static struct cad_singleton_info_struct_type cad_singleton;

u8 *g_audio_mem;
u32 g_audio_base;
u32 g_audio_size;

static u8 add_ref_count(void);
static u8 release_ref_count(void);

static int __init cad_probe(struct platform_device *pdev)
{
	u8			ref_count;
	s32			rc;

	rc = CAD_RES_SUCCESS;

	ref_count = add_ref_count();

	if (ref_count != 1) {
		pr_err("CAD already Initialized %d\n",
			cad_singleton.cad_ref_ct);
		rc = CAD_RES_FAILURE;
		goto done;
	}

	D("%s: %s called\n", MODULE_NAME, __func__);

	mutex_init(&cad.sync);

	g_audio_base = pdev->resource[0].start;
	g_audio_size = pdev->resource[0].end - pdev->resource[0].start + 1;

	g_audio_mem = ioremap(g_audio_base, g_audio_size);
	if (g_audio_mem == NULL)
		return -ENOMEM;

	cad.ard			= NULL;
	cad.audiodec		= NULL;
	cad.audioenc		= NULL;
	cad.device_filter	= NULL;
	cad.voicedec		= NULL;
	cad.voiceenc		= NULL;
	cad.resource_alloc	= NULL;

	rc = cad_audio_dec_init(&cad.audiodec);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_audio_dec_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}

	rc = cad_audio_enc_init(&cad.audioenc);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_audio_enc_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}

	rc = cad_ard_init(&cad.ard);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_ard_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}

	rc = cad_volume_init(&cad.volume);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_volume_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}

	rc = cad_dtmf_init(&cad.dtmf);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_dtmf_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}

	rc = cad_filter_eq_init(&cad.equalizer);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_filter_eq_init failed\n");
		rc = CAD_RES_FAILURE;
		goto done;
	}
done:
	return rc;
}

s32 cad_open(struct cad_open_struct_type *open_param)
{
	s32 handle;
	s8 resource_alloc, ard, hw_accel;
	s32 rc;
	int i;

	D("%s: %s called\n", MODULE_NAME, __func__);

	rc = CAD_RES_FAILURE;
	handle = resource_alloc = ard = hw_accel = 0;

	mutex_lock(&cad.sync);

	if (open_param != NULL) {
		switch (open_param->format) {
		case CAD_FORMAT_PCM:
			if (open_param->op_code == CAD_OPEN_OP_DEVICE_CTRL)
				handle = CAD_SESSION_DEV_CTRL;
			else if (open_param->op_code == CAD_OPEN_OP_WRITE)
				handle = CAD_SESSION_PCM;
			else if (open_param->op_code == CAD_OPEN_OP_READ)
				handle = CAD_SESSION_PCM_IN;
			break;
		case CAD_FORMAT_MP3:
		case CAD_FORMAT_AAC:
			for (i = CAD_SESSION_HW_CODEC_POOL_START;
				i <= CAD_SESSION_HW_CODEC_POOL_END; i++) {
				if (cad.session_info[i].status ==
					CAD_SESSION_FREE) {
					handle = i;
					break;
				}
			}
			break;
		case CAD_FORMAT_FM:
			handle = CAD_SESSION_FM;
			open_param->format = CAD_FORMAT_PCM;
			break;
		case CAD_FORMAT_VOICE:
			if (open_param->op_code == CAD_OPEN_OP_WRITE)
				handle = CAD_SESSION_VOICE_TX;
			else if (open_param->op_code == CAD_OPEN_OP_READ)
				handle = CAD_SESSION_VOICE_RX;
			break;
		default:
			break;
		}

		if ((handle > 0) &&
			cad.session_info[handle].status ==
					CAD_SESSION_FREE) {
			pr_info("cad_open: use session %d\n", handle);
				if (open_param->op_code == CAD_OPEN_OP_READ) {
				cad.session_info[handle].hw_accel
							= cad.audioenc;
				} else if (open_param->op_code ==
					CAD_OPEN_OP_WRITE) {

					if (open_param->format !=
						CAD_FORMAT_DTMF)

					cad.session_info[handle].
							hw_accel = cad.audiodec;
					else
					cad.session_info[handle].
							hw_accel = cad.dtmf;
				}
				rc = CAD_RES_SUCCESS;
		}
	} else {
		rc = CAD_RES_FAILURE;
		goto done;
	}

	if (rc == CAD_RES_FAILURE)
		goto done;

	if (cad.resource_alloc != NULL) {
		if (cad.resource_alloc->open != NULL) {

			rc = cad.resource_alloc->open(handle,
							open_param);

			if (rc == CAD_RES_FAILURE)
				goto done;
			resource_alloc = 1;
		}

	}

	if ((cad.ard != NULL) && (cad.ard->open != NULL)) {

		rc = cad.ard->open(handle, open_param);

		if (rc == CAD_RES_FAILURE)
			goto done;

		ard = 1;
	}

	if ((cad.session_info[handle].hw_accel != NULL) &&
		(cad.session_info[handle].hw_accel->open != NULL)) {

		rc = cad.session_info[handle].hw_accel->open(
						handle, open_param);

		if (rc == CAD_RES_FAILURE)
			goto done;

		hw_accel = 1;
	}

	mutex_init(&cad.session_info[handle].sync);

	if (rc == CAD_RES_SUCCESS)
		cad.session_info[handle].status = CAD_SESSION_INUSE;

done:
	/* This could be implemented with multiple goto labels, but that would
	   be ugly.
	*/
	if (rc == CAD_RES_FAILURE) {

		if (ard && cad.ard->close)
			cad.ard->close(handle);

		if (hw_accel &&
			cad.session_info[handle].hw_accel->close) {

			cad.session_info[handle].hw_accel->close(
								handle);
		}

		if (resource_alloc && cad.resource_alloc->close)
			cad.resource_alloc->close(handle);

		handle = 0;
	}
	mutex_unlock(&cad.sync);

	return handle;
}
EXPORT_SYMBOL(cad_open);




s32 cad_close(s32 driver_handle)
{
	s32 rc;

	D("%s: %s called\n", MODULE_NAME, __func__);

	rc = CAD_RES_SUCCESS;

	if (driver_handle > 0) {

		mutex_lock(&cad.session_info[driver_handle].sync);

		if (cad.session_info[driver_handle].hw_accel &&
			cad.session_info[driver_handle].hw_accel->close)
			cad.session_info[driver_handle].hw_accel->
				close(driver_handle);

		if (cad.equalizer && cad.equalizer->close)
			(void) cad.equalizer->close(driver_handle);

		if (cad.ard && cad.ard->close)
			(void) cad.ard->close(driver_handle);

		if (cad.resource_alloc && cad.resource_alloc->close)
			(void) cad.resource_alloc->close(driver_handle);

		mutex_lock(&cad.sync);
		mutex_unlock(&cad.session_info[driver_handle].sync);

		cad.session_info[driver_handle].status = CAD_SESSION_FREE;
		cad.session_info[driver_handle].hw_accel = NULL;

		mutex_unlock(&cad.sync);
	}

	return rc;
}
EXPORT_SYMBOL(cad_close);



s32 cad_read(s32 driver_handle, struct cad_buf_struct_type *buf)
{
	s32 data_read;

	D("%s: %s called\n", MODULE_NAME, __func__);

	data_read = CAD_RES_FAILURE;

	if (cad.session_info[driver_handle].hw_accel &&
		cad.session_info[driver_handle].hw_accel->read)
		data_read = cad.session_info[driver_handle].hw_accel->
					read(driver_handle, buf);

	return buf->actual_size;
}
EXPORT_SYMBOL(cad_read);


s32 cad_write(s32 driver_handle, struct cad_buf_struct_type *buf)
{
	s32 data_written;

	D("%s: %s called\n", MODULE_NAME, __func__);

	data_written = CAD_RES_FAILURE;

	if (cad.session_info[driver_handle].hw_accel &&
		cad.session_info[driver_handle].hw_accel->write)
		data_written = cad.session_info[driver_handle].hw_accel->
			write(driver_handle, buf);

	return data_written;
}
EXPORT_SYMBOL(cad_write);


s32 cad_ioctl(s32 driver_handle, u32 cmd_code, void *cmd_buf, u32 cmd_buf_len)
{
	s32 ret_val;

	D("%s: %s called\n", MODULE_NAME, __func__);

	ret_val = CAD_RES_SUCCESS;

	if (cmd_code != CAD_IOCTL_CMD_STREAM_PAUSE &&
	    cmd_code != CAD_IOCTL_CMD_STREAM_RESUME &&
	    cmd_code != CAD_FILTER_CONFIG_STREAM_VOLUME)
		mutex_lock(&cad.session_info[driver_handle].sync);

	if (cad.ard && cad.ard->ioctl)
		ret_val = cad.ard->ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	if ((ret_val != CAD_RES_FAILURE)
		&& cad.session_info[driver_handle].hw_accel
		&& cad.session_info[driver_handle].hw_accel->ioctl)
		ret_val = cad.session_info[driver_handle].hw_accel->
				ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	if ((ret_val == CAD_RES_SUCCESS) && cad.volume && cad.volume->ioctl)
		ret_val = cad.volume->ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	if ((ret_val == CAD_RES_SUCCESS) && cad.equalizer
			&& cad.equalizer->ioctl)
		ret_val = cad.equalizer->ioctl(driver_handle, cmd_code,
							cmd_buf, cmd_buf_len);

	if (cmd_code != CAD_IOCTL_CMD_STREAM_PAUSE &&
	    cmd_code != CAD_IOCTL_CMD_STREAM_RESUME &&
	    cmd_code != CAD_FILTER_CONFIG_STREAM_VOLUME)
		mutex_unlock(&cad.session_info[driver_handle].sync);

	return ret_val;
}
EXPORT_SYMBOL(cad_ioctl);

static struct platform_driver cad_driver = {
	.probe = cad_probe,
	.driver = {
		.name = "msm_audio",
		.owner = THIS_MODULE,
	},
};

static int __init cad_init(void)
{
	s32 rc;

	rc = platform_driver_register(&cad_driver);

	return rc;
}

static void __exit cad_exit(void)
{
	u8	ref_count;
	s32	i = 0;

	ref_count = release_ref_count();

	if (ref_count == 0) {
		D("%s: %s called\n", MODULE_NAME, __func__);

		mutex_unlock(&cad.sync);

		while (i < CAD_MAX_SESSION)
			mutex_unlock(&cad.session_info[i++].sync);

		(void)cad_audio_dec_dinit();
		(void)cad_audio_enc_dinit();
		(void)cad_ard_dinit();
		(void)cad_volume_dinit();
		(void)cad_dtmf_dinit();
		(void)cad_filter_eq_dinit();

		iounmap(g_audio_mem);

	} else {
		pr_err("CAD not De-Initialized as cad_ref_ct = %d\n",
			cad_singleton.cad_ref_ct);
	}
}

u8 add_ref_count(void)
{
	unsigned long	flags;

	spin_lock_irqsave(&slock, flags);
	cad_singleton.cad_ref_ct++;
	spin_unlock_irqrestore(&slock, flags);

	D("add_ref_count(cad_ref_ct = %d)\n", cad_singleton.cad_ref_ct);

	return cad_singleton.cad_ref_ct;
}

u8 release_ref_count(void)
{
	unsigned long	flags;

	spin_lock_irqsave(&slock, flags);
	cad_singleton.cad_ref_ct--;
	spin_unlock_irqrestore(&slock, flags);

	D("release_ref_count(cad_ref_ct = %d)\n", cad_singleton.cad_ref_ct);

	return cad_singleton.cad_ref_ct;
}

module_init(cad_init);
module_exit(cad_exit);

MODULE_DESCRIPTION("CAD driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");

