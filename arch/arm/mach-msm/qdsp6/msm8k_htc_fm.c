/*
 *
 * Copyright (c) 2009 HTC Corporation
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>

#include <mach/qdsp6/msm8k_cad.h>
#include <mach/qdsp6/msm8k_cad_ioctl.h>
#include <mach/qdsp6/msm8k_cad_write_pcm_format.h>
#include <mach/qdsp6/msm8k_cad_devices.h>

#if 0
#define D(fmt, args...) printk(KERN_INFO "msm8k_htc_fm: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define MSM8K_FM_PROC_NAME "msm8k_htc_fm"

#define AUDIO_MAGIC 'a'

struct fm {
	u32 cad_w_handle;
	struct mutex lock;
	int opened;
};

struct fm g_fm;

static int msm8k_htc_fm_open(struct inode *inode, struct file *f)
{
	struct fm *fm = &g_fm;
	struct cad_open_struct_type  cos;
	int rc = CAD_RES_SUCCESS;
	u32 stream_device[1];
	struct cad_stream_device_struct_type cad_stream_dev;
	struct cad_stream_info_struct_type cad_stream_info;
	struct cad_write_pcm_format_struct_type cad_write_pcm_fmt;
	D("%s\n", __func__);

	mutex_lock(&fm->lock);

	if (fm->opened) {
		pr_err("fm: busy\n");
		rc = -EBUSY;
		goto done;
	}

	f->private_data = fm;

	cos.format = CAD_FORMAT_FM;
	cos.op_code = CAD_OPEN_OP_WRITE;
	fm->cad_w_handle = cad_open(&cos);

	if (fm->cad_w_handle == 0) {
		rc = CAD_RES_FAILURE;
		goto done;
	}

	cad_stream_info.app_type = CAD_STREAM_APP_PLAYBACK;
	cad_stream_info.priority = 0;
	cad_stream_info.buf_mem_type = CAD_STREAM_BUF_MEM_HEAP;
	cad_stream_info.ses_buf_max_size = 1024 * 10;
	rc = cad_ioctl(fm->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_INFO,
		&cad_stream_info,
		sizeof(struct cad_stream_info_struct_type));
	if (rc) {
		pr_err("pcm_out: cad_ioctl() SET_STREAM_INFO failed\n");
		goto done;
	}
	stream_device[0] = CAD_HW_DEVICE_ID_DEFAULT_RX;
	cad_stream_dev.device = (u32 *)&stream_device[0];
	cad_stream_dev.device_len = 1;
	rc = cad_ioctl(fm->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_DEVICE,
		&cad_stream_dev,
		sizeof(struct cad_stream_device_struct_type));
	if (rc) {
		pr_err("pcm_out: cad_ioctl() SET_STREAM_DEVICE failed\n");
		goto done;
	}
	cad_write_pcm_fmt.us_ver_id = CAD_WRITE_PCM_VERSION_10;
	cad_write_pcm_fmt.pcm.us_channel_config = 1;
	cad_write_pcm_fmt.pcm.us_width = 1;
	cad_write_pcm_fmt.pcm.us_sign = 0;
	cad_write_pcm_fmt.pcm.us_sample_rate = 11;
	rc = cad_ioctl(fm->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_CONFIG,
		&cad_write_pcm_fmt,
		sizeof(struct cad_write_pcm_format_struct_type));
	if (rc) {
		pr_err("pcm_out: cad_ioctl() SET_STREAM_CONFIG failed\n");
		goto done;
	}
	rc = cad_ioctl(fm->cad_w_handle, CAD_IOCTL_CMD_STREAM_START,
		NULL, 0);
	if (rc) {
		pr_err("pcm_out: cad_ioctl() STREAM_START failed\n");
		goto done;
	}
	fm->opened = 1;
done:
	mutex_unlock(&fm->lock);
	return rc;
}

static int msm8k_htc_fm_release(struct inode *inode, struct file *f)
{
	int rc = CAD_RES_SUCCESS;
	struct fm *fm = f->private_data;
	D("%s\n", __func__);

	mutex_lock(&fm->lock);

	cad_close(fm->cad_w_handle);

	fm->opened = 0;
	mutex_unlock(&fm->lock);

	return rc;
}

static ssize_t msm8k_htc_fm_read(struct file *f, char __user *buf, size_t cnt,
		loff_t *pos)
{
	D("%s\n", __func__);
	return -EINVAL;
}

static ssize_t msm8k_htc_fm_write(struct file *f, const char __user *buf,
		size_t cnt, loff_t *pos)
{
	D("%s\n", __func__);
	return -EINVAL;
}

static int msm8k_htc_fm_ioctl(struct inode *inode, struct file *f,
		unsigned int cmd, unsigned long arg)
{
	D("%s\n", __func__);
	return -EINVAL;
}

#ifdef CONFIG_PROC_FS
int msm8k_htc_fm_read_proc(char *pbuf, char **start, off_t offset,
			int count, int *eof, void *data)
{
	int len = 0;
	len += snprintf(pbuf, 16, "fm\n");

	*eof = 1;
	return len;
}
#endif

static const struct file_operations msm8k_htc_fm_fops = {
	.owner = THIS_MODULE,
	.open = msm8k_htc_fm_open,
	.release = msm8k_htc_fm_release,
	.read = msm8k_htc_fm_read,
	.write = msm8k_htc_fm_write,
	.ioctl = msm8k_htc_fm_ioctl,
	.llseek = no_llseek,
};


struct miscdevice msm8k_htc_fm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_htc_fm",
	.fops	= &msm8k_htc_fm_fops,
};

static int __init msm8k_htc_fm_init(void)
{
	int rc;
	D("%s\n", __func__);

	rc = misc_register(&msm8k_htc_fm_misc);
	mutex_init(&g_fm.lock);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry(MSM8K_FM_PROC_NAME,
			0, NULL, msm8k_htc_fm_read_proc, NULL);
#endif

	return rc;
}

static void __exit msm8k_htc_fm_exit(void)
{
	D("%s\n", __func__);
#ifdef CONFIG_PROC_FS
	remove_proc_entry(MSM8K_FM_PROC_NAME, NULL);
#endif
}


module_init(msm8k_htc_fm_init);
module_exit(msm8k_htc_fm_exit);

MODULE_DESCRIPTION("HTC FM driver");
MODULE_LICENSE("GPL v2");

