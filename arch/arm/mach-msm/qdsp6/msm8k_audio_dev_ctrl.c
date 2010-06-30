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
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/msm_audio.h>

#include <asm/ioctls.h>
#include <mach/qdsp6/msm8k_cad.h>
#include <mach/qdsp6/msm8k_cad_ioctl.h>
#include <mach/qdsp6/msm8k_cad_devices.h>
#include <mach/qdsp6/msm8k_cad_volume.h>
#include <mach/qdsp6/msm8k_ard_adie.h>


#if 0
#define D(fmt, args...) printk(KERN_INFO "msm8k_audio_dev_ctrl: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define MSM8K_AUDIO_PROC_NAME "msm8k_audio_dev_ctrl"

#define AUDIO_MAGIC 'a'

struct msm8k_audio_dev_ctrl {
	u32 cad_ctrl_handle;
	u32 current_volume;
	int current_rx_device;
	int current_tx_device;
};

struct msm8k_audio_dev_ctrl g_ctrl;
static atomic_t tx_mute = ATOMIC_INIT(0);

static int msm8k_audio_dev_ctrl_open(struct inode *inode, struct file *f)
{
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;
	D("%s\n", __func__);

	f->private_data = ctrl;

	return CAD_RES_SUCCESS;
}

static int msm8k_audio_dev_ctrl_release(struct inode *inode, struct file *f)
{
	int rc = CAD_RES_SUCCESS;

	D("%s\n", __func__);

	return rc;
}

static ssize_t msm8k_audio_dev_ctrl_read(struct file *f, char __user *buf,
	size_t cnt, loff_t *pos)
{
	D("%s\n", __func__);
	return -EINVAL;
}

static ssize_t msm8k_audio_dev_ctrl_write(struct file *f,
	const char __user *buf, size_t cnt, loff_t *pos)
{
	D("%s\n", __func__);
	return -EINVAL;
}

int audio_switch_device(int new_device)
{
	int rc;
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;
	struct cad_device_struct_type cad_dev;
	int prev_force_spkr_mode = get_force_speaker_mode();
	int prev_fm_mode = get_fm_mode();

	D("%s\n", __func__);

	memset(&cad_dev, 0, sizeof(struct cad_device_struct_type));

	switch (new_device) {
	case HANDSET_MIC:
		cad_dev.device = CAD_HW_DEVICE_ID_HANDSET_MIC;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case HANDSET_SPKR:
		cad_dev.device = CAD_HW_DEVICE_ID_HANDSET_SPKR;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_MIC:
		cad_dev.device = CAD_HW_DEVICE_ID_HEADSET_MIC;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case HEADSET_SPKR_MONO:
		cad_dev.device = CAD_HW_DEVICE_ID_HEADSET_SPKR_MONO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_SPKR_STEREO:
		cad_dev.device = CAD_HW_DEVICE_ID_HEADSET_SPKR_STEREO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case SPKR_PHONE_MIC:
		cad_dev.device = CAD_HW_DEVICE_ID_SPKR_PHONE_MIC;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case SPKR_PHONE_MONO:
		cad_dev.device = CAD_HW_DEVICE_ID_SPKR_PHONE_MONO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case SPKR_PHONE_STEREO:
		cad_dev.device = CAD_HW_DEVICE_ID_SPKR_PHONE_STEREO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case BT_SCO_MIC:
		cad_dev.device = CAD_HW_DEVICE_ID_BT_SCO_MIC;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case BT_SCO_SPKR:
		cad_dev.device = CAD_HW_DEVICE_ID_BT_SCO_SPKR;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case BT_A2DP_SPKR:
		cad_dev.device = CAD_HW_DEVICE_ID_BT_A2DP_SPKR;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case TTY_HEADSET_MIC:
		cad_dev.device = CAD_HW_DEVICE_ID_TTY_HEADSET_MIC;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case TTY_HEADSET_SPKR:
		cad_dev.device = CAD_HW_DEVICE_ID_TTY_HEADSET_SPKR;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_MONO_PLUS_SPKR_MONO_RX:
		cad_dev.device =
			CAD_HW_DEVICE_ID_HEADSET_MONO_PLUS_SPKR_MONO_RX;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_MONO_PLUS_SPKR_STEREO_RX:
		cad_dev.device =
			CAD_HW_DEVICE_ID_HEADSET_MONO_PLUS_SPKR_STEREO_RX;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_STEREO_PLUS_SPKR_MONO_RX:
		cad_dev.device =
			CAD_HW_DEVICE_ID_HEADSET_STEREO_PLUS_SPKR_MONO_RX;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case HEADSET_STEREO_PLUS_SPKR_STEREO_RX:
		cad_dev.device =
			CAD_HW_DEVICE_ID_HEADSET_STEREO_PLUS_SPKR_STEREO_RX;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case I2S_RX:
		cad_dev.device = CAD_HW_DEVICE_ID_I2S_RX;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case I2S_TX:
		cad_dev.device = CAD_HW_DEVICE_ID_I2S_TX;
		cad_dev.reserved = CAD_TX_DEVICE;
		break;
	case SPKR_PHONE_HEADSET_STEREO:
		cad_dev.device = CAD_HW_DEVICE_ID_HEADSET_SPKR_STEREO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case FM_HEADSET:
		cad_dev.device = CAD_HW_DEVICE_ID_HEADSET_SPKR_STEREO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	case FM_SPKR:
		cad_dev.device = CAD_HW_DEVICE_ID_SPKR_PHONE_STEREO;
		cad_dev.reserved = CAD_RX_DEVICE;
		break;
	default:
		return -ENODEV;
	}

	/* Set and check if force_speaker_mode is changed */
	if (cad_dev.reserved == CAD_RX_DEVICE) {
		if (new_device == SPKR_PHONE_HEADSET_STEREO)
			set_force_speaker_mode(1);
		else
			set_force_speaker_mode(0);

		if (new_device == FM_HEADSET)
			set_fm_mode(1);
		else if (new_device == FM_SPKR)
			set_fm_mode(2);
		else
			set_fm_mode(0);
	}
	if (prev_force_spkr_mode != get_force_speaker_mode() ||
	    prev_fm_mode != get_fm_mode())
		cad_dev.force_update = 1;

	pr_info("SWITCH DEVICE %d\n", cad_dev.device);
	rc = cad_ioctl(ctrl->cad_ctrl_handle,
			CAD_IOCTL_CMD_DEVICE_SET_GLOBAL_DEFAULT,
			&cad_dev,
			sizeof(struct cad_device_struct_type));
	if (rc) {
		pr_err("cad_ioctl() SET_GLOBAL_DEFAULT failed\n");
		return rc;
	}

	if (cad_dev.reserved == CAD_RX_DEVICE)
		ctrl->current_rx_device = cad_dev.device;
	else
		ctrl->current_tx_device = cad_dev.device;

#if 1
	if (cad_dev.reserved == CAD_TX_DEVICE) {
		struct msm_mute_info info;
		info.path = 1;
		info.mute = 0;
		pr_info("UNMUTE DEVICE %d\n", cad_dev.device);
		audio_set_device_mute(&info);
	}
#else
	if (cad_dev.reserved == CAD_TX_DEVICE &&
	    atomic_read(&tx_mute)) {
		/* keep tx mute status if path is changed. */
		struct msm_mute_info info;
		info.path = info.mute = 1;
		pr_info("MUTE DEVICE %d\n", cad_dev.device);
		audio_set_device_mute(&info);
	}
#endif
	return rc;
}
EXPORT_SYMBOL(audio_switch_device);


int audio_set_device_volume_path(struct msm_vol_info *v)
{
	int rc;
	struct cad_flt_cfg_dev_vol cad_dev_volume;
	struct cad_filter_struct flt;
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;

	D("%s\n", __func__);

	if ((v->vol < 0) || (v->vol > 100)) {
		D("invalid volume value\n");
		return -EINVAL;
	}

	if ((v->path != CAD_RX_DEVICE) && (v->path != CAD_TX_DEVICE)) {
		pr_err("%s: invalid path\n", __func__);
		return -1;
	}

	memset(&flt, 0,	sizeof(struct cad_filter_struct));
	memset(&cad_dev_volume, 0,
			sizeof(struct cad_flt_cfg_dev_vol));
	ctrl->current_volume = v->vol;
	cad_dev_volume.volume = ctrl->current_volume;

	cad_dev_volume.path = v->path;
	if (v->path == CAD_RX_DEVICE)
	cad_dev_volume.device_id = ctrl->current_rx_device;
	else
		cad_dev_volume.device_id = ctrl->current_tx_device;

	flt.filter_type = CAD_DEVICE_FILTER_TYPE_VOL;
	flt.cmd = CAD_FILTER_CONFIG_DEVICE_VOLUME;
	flt.format_block = &cad_dev_volume;
	flt.format_block_len = sizeof(struct cad_flt_cfg_dev_vol);

	pr_info("msm8k_audio_dev_ctrl set volume %d\n", ctrl->current_volume);
	rc = cad_ioctl(ctrl->cad_ctrl_handle,
		CAD_IOCTL_CMD_SET_DEVICE_FILTER_CONFIG,
		&flt,
		sizeof(struct cad_filter_struct));
	if (rc)
		pr_err("cad_ioctl() set volume failed\n");

	return rc;
}
EXPORT_SYMBOL(audio_set_device_volume_path);


int audio_set_device_volume(int vol)
{
	struct msm_vol_info vi;

	vi.vol = vol;
	vi.path = CAD_RX_DEVICE;

	return audio_set_device_volume_path(&vi);
}
EXPORT_SYMBOL(audio_set_device_volume);


int audio_set_device_mute(struct msm_mute_info *m)
{
	int rc;
	struct cad_filter_struct flt;
	struct cad_flt_cfg_dev_mute dev_mute_buf;
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;

	D("%s\n", __func__);

	if ((m->path != CAD_RX_DEVICE) && (m->path != CAD_TX_DEVICE)) {
		pr_err("%s: invalid path\n", __func__);
		return -1;
	}

	memset(&flt, 0, sizeof(struct cad_filter_struct));
	memset(&dev_mute_buf, 0,
		sizeof(struct cad_flt_cfg_dev_mute));

	dev_mute_buf.ver_id = CAD_FILTER_CONFIG_DEVICE_VOLUME_VERID;

	if (m->path == CAD_RX_DEVICE)
		dev_mute_buf.device_id = ctrl->current_rx_device;
	else
		dev_mute_buf.device_id = ctrl->current_tx_device;

	dev_mute_buf.path = m->path;
	dev_mute_buf.mute = m->mute;

	flt.cmd = CAD_FILTER_CONFIG_DEVICE_MUTE;
	flt.filter_type = CAD_DEVICE_FILTER_TYPE_VOL;
	flt.format_block_len =
		sizeof(struct cad_flt_cfg_dev_mute);
	flt.format_block = &dev_mute_buf;
	pr_info("msm8k_audio_dev_ctrl set device mute %d %d\n",
		dev_mute_buf.path, dev_mute_buf.mute);
	rc = cad_ioctl(ctrl->cad_ctrl_handle,
		CAD_IOCTL_CMD_SET_DEVICE_FILTER_CONFIG,
		&flt,
		sizeof(struct cad_filter_struct));
	if (rc)
		pr_err("cad_ioctl() set mute failed\n");
	else if (m->path == CAD_TX_DEVICE) {
		/* Keep latest TX mute status */
		atomic_set(&tx_mute, m->mute);
	}
	return rc;
}
EXPORT_SYMBOL(audio_set_device_mute);


static int msm8k_audio_dev_ctrl_ioctl(struct inode *inode, struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int rc;
	u32 uparam;
	struct msm_mute_info m;
	struct msm_vol_info v;

	D("%s\n", __func__);

	switch (cmd) {
	case AUDIO_SWITCH_DEVICE:
		if (copy_from_user(&uparam, (void *)arg,
				sizeof(uparam)))
			return CAD_RES_FAILURE;

		rc = audio_switch_device(uparam);
		break;
	case AUDIO_SET_VOLUME:
		if (copy_from_user(&uparam, (void *)arg,
				sizeof(uparam)))
			return CAD_RES_FAILURE;

		rc = audio_set_device_volume(uparam);

		break;
	case AUDIO_SET_VOLUME_PATH:
		if (copy_from_user(&v, (void *)arg,
				sizeof(struct msm_vol_info)))
			return CAD_RES_FAILURE;

		rc = audio_set_device_volume_path(&v);

		break;
	case AUDIO_SET_MUTE:
		rc = copy_from_user(&m, (void *)arg,
				sizeof(struct msm_mute_info));
		if (rc) {
			pr_err("AUDIO_SET_MUTE copy from user failed\n");
			break;
		}

		rc = audio_set_device_mute(&m);

		break;
	case AUDIO_SET_MAX_VOL_ALL:
		rc = volume_set_max_vol_all();

		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

#ifdef CONFIG_PROC_FS
int msm8k_audio_dev_ctrl_read_proc(char *pbuf, char **start, off_t offset,
			int count, int *eof, void *data)
{
	int len = 0;
	len += snprintf(pbuf, 16, "audio\n");

	*eof = 1;
	return len;
}
#endif

static const struct file_operations msm8k_audio_dev_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = msm8k_audio_dev_ctrl_open,
	.release = msm8k_audio_dev_ctrl_release,
	.read = msm8k_audio_dev_ctrl_read,
	.write = msm8k_audio_dev_ctrl_write,
	.ioctl = msm8k_audio_dev_ctrl_ioctl,
	.llseek = no_llseek,
};


struct miscdevice msm8k_audio_dev_ctrl_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_dev_ctrl",
	.fops	= &msm8k_audio_dev_ctrl_fops,
};

static int __init msm8k_audio_dev_ctrl_init(void)
{
	struct cad_open_struct_type  cos;
	int rc;
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;

	D("%s\n", __func__);

	rc = misc_register(&msm8k_audio_dev_ctrl_misc);
	if (rc) {
		pr_err("failed to register audio control device\n");
		return CAD_RES_FAILURE;
	}

	cos.format = 0;
	cos.op_code = CAD_OPEN_OP_DEVICE_CTRL;
	ctrl->cad_ctrl_handle = cad_open(&cos);
	ctrl->current_rx_device = CAD_HW_DEVICE_ID_HANDSET_SPKR;
	ctrl->current_tx_device = CAD_HW_DEVICE_ID_HANDSET_MIC;

	if (ctrl->cad_ctrl_handle < 0) {
		pr_err("Dev CTRL handle < 0\n");
		return CAD_RES_FAILURE;
	}

	set_audio_ctrl_handle(ctrl->cad_ctrl_handle);

	rc = cad_ioctl(ctrl->cad_ctrl_handle,
			CAD_IOCTL_CMD_STREAM_START,
			NULL,
			0);
	if (rc) {
		pr_err("%s: cad_ioctl() STREAM_START failed\n", __func__);
		return CAD_RES_FAILURE;
	}

#ifdef CONFIG_PROC_FS
	create_proc_read_entry(MSM8K_AUDIO_PROC_NAME,
			0, NULL, msm8k_audio_dev_ctrl_read_proc, NULL);
#endif

	return rc;
}

static void __exit msm8k_audio_dev_ctrl_exit(void)
{
	struct msm8k_audio_dev_ctrl *ctrl = &g_ctrl;
	D("%s\n", __func__);

	cad_close(ctrl->cad_ctrl_handle);

#ifdef CONFIG_PROC_FS
	remove_proc_entry(MSM8K_AUDIO_PROC_NAME, NULL);
#endif
}


module_init(msm8k_audio_dev_ctrl_init);
module_exit(msm8k_audio_dev_ctrl_exit);

MODULE_DESCRIPTION("MSM Audio Device Control driver");
MODULE_LICENSE("GPL v2");

