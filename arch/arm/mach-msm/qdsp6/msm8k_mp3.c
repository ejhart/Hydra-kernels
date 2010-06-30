/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#include <linux/wakelock.h>
#include <linux/msm_audio.h>
#include <linux/sched.h>

#include <asm/ioctls.h>
#include <mach/qdsp6/msm8k_cad.h>
#include <mach/qdsp6/msm8k_cad_ioctl.h>
#include <mach/qdsp6/msm8k_ard.h>
#include <mach/qdsp6/msm8k_cad_write_mp3_format.h>
#include <mach/qdsp6/msm8k_cad_devices.h>
#include <mach/qdsp6/msm8k_cad_volume.h>

#if 0
#define D(fmt, args...) printk(KERN_INFO "msm8k_mp3: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define MSM8K_MP3_PROC_NAME "msm8k_mp3"

#define AUDIO_MAGIC 'a'

struct mp3 {
	u32 cad_w_handle;
	u32 volume;
	struct mutex write_lock;
	wait_queue_head_t eos_wait;
	u16 eos_ack;
	u16 flush_rcvd;
};
static struct wake_lock g_mp3_wakelock;
static atomic_t g_mp3_open_count = ATOMIC_INIT(0);

static void audio_prevent_sleep(void)
{
	if (atomic_add_return(1, &g_mp3_open_count) == 1)
		wake_lock(&g_mp3_wakelock);
}

static void audio_allow_sleep(void)
{
	if (atomic_sub_and_test(1, &g_mp3_open_count))
		wake_unlock(&g_mp3_wakelock);
}

static int msm8k_mp3_open(struct inode *inode, struct file *f)
{
	struct mp3 *mp3;
	struct cad_open_struct_type  cos;
	D("%s\n", __func__);

	mp3 = kmalloc(sizeof(struct mp3), GFP_KERNEL);
	if (mp3 == NULL) {
		pr_err("Could not allocate memory for mp3 driver\n");
		return CAD_RES_FAILURE;
	}

	f->private_data = mp3;

	memset(mp3, 0, sizeof(struct mp3));

	mp3->eos_ack = 0;
	mp3->flush_rcvd = 0;

	mutex_init(&mp3->write_lock);
	init_waitqueue_head(&mp3->eos_wait);

	audio_prevent_sleep();
	cos.format = CAD_FORMAT_MP3;
	cos.op_code = CAD_OPEN_OP_WRITE;
	mp3->cad_w_handle = cad_open(&cos);

	if (mp3->cad_w_handle == 0) {
		kfree(mp3);
		audio_allow_sleep();
		return CAD_RES_FAILURE;
	} else
		return CAD_RES_SUCCESS;
}

static int msm8k_mp3_fsync(struct file *f, struct dentry *dentry, int datasync)
{
	int ret = CAD_RES_SUCCESS;
	struct mp3 *mp3 = f->private_data;

	mutex_lock(&mp3->write_lock);
	ret = cad_ioctl(mp3->cad_w_handle,
			CAD_IOCTL_CMD_STREAM_END_OF_STREAM,
			NULL, 0);
	mutex_unlock(&mp3->write_lock);

	ret = wait_event_interruptible(mp3->eos_wait, mp3->eos_ack ||
					 mp3->flush_rcvd);

	ret = (mp3->eos_ack) ? CAD_RES_SUCCESS : CAD_RES_FAILURE;
	mp3->eos_ack = 0;
	mp3->flush_rcvd = 0;

	return ret;

}

static int msm8k_mp3_release(struct inode *inode, struct file *f)
{
	int rc = CAD_RES_SUCCESS;
	struct mp3 *mp3 = f->private_data;
	D("%s %d\n", __func__, mp3->cad_w_handle);

	cad_close(mp3->cad_w_handle);
	kfree(mp3);
	audio_allow_sleep();

	return rc;
}

static ssize_t msm8k_mp3_read(struct file *f, char __user *buf, size_t cnt,
		loff_t *pos)
{
	D("%s\n", __func__);
	return -EINVAL;
}

void msm8k_mp3_eos_event_cb(u32 event, void *evt_packet,
				u32 evt_packet_len, void *client_data)
{
	struct mp3 *mp3 = client_data;

	if (event == CAD_EVT_STATUS_EOS) {
		mp3->eos_ack = 1;
		wake_up(&mp3->eos_wait);
	}
}

static ssize_t msm8k_mp3_write(struct file *f, const char __user *buf,
		size_t cnt, loff_t *pos)
{
	struct cad_buf_struct_type cbs;
	struct mp3 *mp3 = f->private_data;

	D("%s %d\n", __func__, mp3->cad_w_handle);

	memset(&cbs, 0, sizeof(struct cad_buf_struct_type));
	cbs.buffer = (void *)buf;
	cbs.phys_addr = 0;
	cbs.max_size = cnt;

	cad_write(mp3->cad_w_handle, &cbs);

	return cnt;
}

static int msm8k_mp3_ioctl(struct inode *inode, struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int rc;
	struct msm_audio_config cfg;
	struct mp3 *p = f->private_data;
	void *cad_arg = (void *)arg;
	u32 stream_device[1];
	struct cad_device_struct_type cad_dev;
	struct cad_stream_device_struct_type cad_stream_dev;
	struct cad_stream_info_struct_type cad_stream_info;
	struct cad_write_mp3_format_struct_type cad_write_mp3_fmt;
	struct cad_flt_cfg_strm_vol cad_strm_volume;
	struct cad_filter_struct flt;
	struct cad_filter_struct cfs;
	struct cad_event_struct_type eos_event;
	D("%s %d\n", __func__, p->cad_w_handle);

	memset(&cad_dev, 0, sizeof(struct cad_device_struct_type));
	memset(&cad_stream_dev, 0,
			sizeof(struct cad_stream_device_struct_type));
	memset(&cad_stream_info, 0, sizeof(struct cad_stream_info_struct_type));
	memset(&cad_write_mp3_fmt, 0,
			sizeof(struct cad_write_mp3_format_struct_type));
	memset(&cfg, 0, sizeof(struct msm_audio_config));
	memset(&flt, 0, sizeof(struct cad_filter_struct));
	memset(&cfs, 0, sizeof(struct cad_filter_struct));

	switch (cmd) {
	case AUDIO_START:
		cad_stream_info.app_type = CAD_STREAM_APP_PLAYBACK;
		cad_stream_info.priority = 0;
		cad_stream_info.buf_mem_type = CAD_STREAM_BUF_MEM_HEAP;
		cad_stream_info.ses_buf_max_size = 1024 * 10;
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_INFO,
			&cad_stream_info,
			sizeof(struct cad_stream_info_struct_type));
		if (rc) {
			pr_err("mp3: cad_ioctl() SET_STREAM_INFO failed\n");
			break;
		}

		stream_device[0] = CAD_HW_DEVICE_ID_DEFAULT_RX;
		cad_stream_dev.device = (u32 *)&stream_device[0];
		cad_stream_dev.device_len = 1;
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_DEVICE,
			&cad_stream_dev,
			sizeof(struct cad_stream_device_struct_type));
		if (rc) {
			pr_err("mp3: cad_ioctl() SET_STREAM_DEVICE failed\n");
			break;
		}

		cad_write_mp3_fmt.ver_id = CAD_WRITE_MP3_VERSION_10;

		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_CONFIG,
			&cad_write_mp3_fmt,
			sizeof(struct cad_write_mp3_format_struct_type));
		if (rc) {
			pr_err("mp3: cad_ioctl() SET_STREAM_CONFIG failed\n");
			break;
		}
		eos_event.callback = &msm8k_mp3_eos_event_cb;
		eos_event.client_data = p;

		rc = cad_ioctl(p->cad_w_handle,
				CAD_IOCTL_CMD_SET_STREAM_EVENT_LSTR,
				&eos_event,
				sizeof(struct cad_event_struct_type));

		if (rc) {
			pr_err("mp3: cad_ioctl() "
			       "SET_STREAM_EVENT_LSTR failed\n");
			break;
		}

		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_STREAM_START,
				NULL, 0);
		if (rc) {
			pr_err("mp3: cad_ioctl() STREAM_START failed\n");
			break;
		}
		break;
	case AUDIO_STOP:
	case AUDIO_ADSP_PAUSE:
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_STREAM_PAUSE,
			NULL, 0);
		break;
	case AUDIO_ADSP_RESUME:
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_STREAM_RESUME,
			NULL, 0);
		break;
	case AUDIO_FLUSH:
		p->flush_rcvd = 1;
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_STREAM_FLUSH,
				NULL, 0);
		wake_up(&p->eos_wait);
		break;
	case AUDIO_GET_CONFIG:
		/* hard-coded until we support this in the CAD */
		cfg.buffer_size = 4096;
		cfg.buffer_count = 2;
		cfg.channel_count = 1;
		cfg.sample_rate = 48000;
		if (copy_to_user((void *)arg, &cfg,
				sizeof(struct msm_audio_config)))
			return -EFAULT;
		rc = CAD_RES_SUCCESS;
		break;
	case AUDIO_SET_CONFIG:
		rc = cad_ioctl(p->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_CONFIG,
			cad_arg, sizeof(u32));
		break;
	case AUDIO_SET_VOLUME:
		rc = copy_from_user(&p->volume, (void *)arg, sizeof(u32));

		memset(&cad_strm_volume, 0,
				sizeof(struct cad_flt_cfg_strm_vol));
		cad_strm_volume.volume = p->volume;
		flt.filter_type = CAD_DEVICE_FILTER_TYPE_VOL;
		flt.format_block = &cad_strm_volume;
		flt.cmd = CAD_FILTER_CONFIG_STREAM_VOLUME;
		flt.format_block_len =
			sizeof(struct cad_flt_cfg_strm_vol);

		rc = cad_ioctl(p->cad_w_handle,
			CAD_IOCTL_CMD_SET_STREAM_FILTER_CONFIG,
			&flt,
			sizeof(struct cad_filter_struct));
		if (rc) {
			pr_err("mp3: cad_ioctl() set volume failed\n");
			break;
		}
		break;
	case AUDIO_SET_EQ:
		rc = copy_from_user(&cfs, (void *)arg,
				sizeof(struct cad_filter_struct));
		rc = cad_ioctl(p->cad_w_handle,
			CAD_IOCTL_CMD_SET_STREAM_FILTER_CONFIG,
			&cfs,
			sizeof(struct cad_filter_struct));
		if (rc)
			pr_err("mp3: cad_ioctl() set equalizer failed\n");
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

#ifdef CONFIG_PROC_FS
int msm8k_mp3_read_proc(char *pbuf, char **start, off_t offset,
			int count, int *eof, void *data)
{
	int len = 0;
	len += snprintf(pbuf, 16, "mp3\n");

	*eof = 1;
	return len;
}
#endif

static const struct file_operations msm8k_mp3_fops = {
	.owner = THIS_MODULE,
	.open = msm8k_mp3_open,
	.release = msm8k_mp3_release,
	.read = msm8k_mp3_read,
	.write = msm8k_mp3_write,
	.ioctl = msm8k_mp3_ioctl,
	.llseek = no_llseek,
	.fsync = msm8k_mp3_fsync,
};

struct miscdevice msm8k_mp3_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_mp3",
	.fops	= &msm8k_mp3_fops,
};

static int __init msm8k_mp3_init(void)
{
	int rc;
	D("%s\n", __func__);

	rc = misc_register(&msm8k_mp3_misc);
	wake_lock_init(&g_mp3_wakelock, WAKE_LOCK_SUSPEND, "audio_mp3");

#ifdef CONFIG_PROC_FS
	create_proc_read_entry(MSM8K_MP3_PROC_NAME,
			0, NULL, msm8k_mp3_read_proc, NULL);
#endif

	return rc;
}

static void __exit msm8k_mp3_exit(void)
{
	D("%s\n", __func__);
#ifdef CONFIG_PROC_FS
	remove_proc_entry(MSM8K_MP3_PROC_NAME, NULL);
#endif
}


module_init(msm8k_mp3_init);
module_exit(msm8k_mp3_exit);

MODULE_DESCRIPTION("MSM MP3 driver");
MODULE_LICENSE("GPL v2");

