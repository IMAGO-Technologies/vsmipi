/*
 * Copyright (C) IMAGO Technologies GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include "vsmipi.h"

#define DEFAULT_FPS 30

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
	#define v4l2_subdev_state v4l2_subdev_pad_config
#endif


struct sensor_mipi {
	struct v4l2_subdev		subdev;
	struct v4l2_pix_format pix;
	const struct sensor_mipi_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	int csi;
};

struct sensor_mipi_datafmt {
	u32	code;
	enum v4l2_colorspace		colorspace;
};



static const struct sensor_mipi_datafmt sensor_colour_fmts[] = {
	{MEDIA_BUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_RAW},
	{MEDIA_BUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_RAW},
	{MEDIA_BUS_FMT_SBGGR10_ALAW8_1X8, V4L2_COLORSPACE_RAW},
};


/* Find a data format by a pixel code in an array */
static const struct sensor_mipi_datafmt
			*sensor_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sensor_colour_fmts); i++)
		if (sensor_colour_fmts[i].code == code)
			return sensor_colour_fmts + i;

	return NULL;
}

/*!
 * sensor_mipi_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int sensor_mipi_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

/*!
 * sensor_mipi_g_param - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int sensor_mipi_g_param(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct sensor_mipi *sensor = v4l2_get_subdevdata(sd);
	struct device *dev = sensor->subdev.dev;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		dev_warn(dev, "Type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int sensor_mipi_s_param(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct sensor_mipi *sensor = v4l2_get_subdevdata(sd);
	struct device *dev = sensor->subdev.dev;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		dev_warn(dev, "Type is not V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		dev_warn(dev, "Type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sensor_mipi_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *state,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct sensor_mipi_datafmt *fmt = sensor_find_datafmt(mf->code);
	struct sensor_mipi *sensor = v4l2_get_subdevdata(sd);

	if (!fmt) {
		mf->code	= sensor_colour_fmts[0].code;
		mf->colorspace	= sensor_colour_fmts[0].colorspace;
		fmt		= &sensor_colour_fmts[0];
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	sensor->fmt = fmt;

	sensor->streamcap.capturemode = 0;
	sensor->pix.width = mf->width;
	sensor->pix.height = mf->height;
	return 0;

/*	dev_err(&client->dev, "Set format failed %d, %d\n",
		fmt->code, fmt->colorspace);
	return -EINVAL;*/
}


static int sensor_mipi_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct sensor_mipi *sensor = v4l2_get_subdevdata(sd);
	const struct sensor_mipi_datafmt *fmt = sensor->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	mf->width	= sensor->pix.width;
	mf->height	= sensor->pix.height;

	return 0;
}

static int sensor_mipi_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(sensor_colour_fmts))
		return -EINVAL;

	code->code = sensor_colour_fmts[code->index].code;
	return 0;
}

/*!
 * sensor_mipi_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int sensor_mipi_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index != 0)
		return -EINVAL;

	fse->max_width = 16384;
	fse->min_width = 128;
	fse->max_height = 16384;
	fse->min_height = 1;

	return 0;
}

/*!
 * sensor_mipi_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int sensor_mipi_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct device *dev = sd->dev;

	if (fie->index != 0)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		dev_warn(dev, "Please assign pixel format, width and height\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = DEFAULT_FPS;

	return 0;
}


static int sensor_mipi_s_stream(struct v4l2_subdev *sd, int enable)
{
	dev_dbg(sd->dev, "s_stream: %d\n", enable);

	return 0;
}

static struct v4l2_subdev_video_ops sensor_mipi_subdev_video_ops = {
	.g_parm = sensor_mipi_g_param,
	.s_parm = sensor_mipi_s_param,
	.s_stream = sensor_mipi_s_stream,
};

static const struct v4l2_subdev_pad_ops sensor_mipi_subdev_pad_ops = {
	.enum_frame_size       = sensor_mipi_enum_framesizes,
	.enum_frame_interval   = sensor_mipi_enum_frameintervals,
	.enum_mbus_code        = sensor_mipi_enum_mbus_code,
	.set_fmt               = sensor_mipi_set_fmt,
	.get_fmt               = sensor_mipi_get_fmt,
};

static struct v4l2_subdev_core_ops sensor_mipi_subdev_core_ops = {
	.s_power	= sensor_mipi_s_power,
};

static struct v4l2_subdev_ops sensor_mipi_subdev_ops = {
	.core	= &sensor_mipi_subdev_core_ops,
	.video	= &sensor_mipi_subdev_video_ops,
	.pad	= &sensor_mipi_subdev_pad_ops,
};


static int sensor_mipi_probe(struct platform_device *pdev)
{
//	struct pinctrl *pinctrl;
	struct device *dev = &pdev->dev;
	int retval;
	struct sensor_mipi *sensor;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	sensor->pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	sensor->pix.width = 1920;
	sensor->pix.height = 1080;
//	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
//					   V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capability = 0;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = DEFAULT_FPS;
	sensor->streamcap.timeperframe.numerator = 1;

	platform_set_drvdata(pdev, sensor);

	// register v4l sub-device
	v4l2_subdev_init(&sensor->subdev, &sensor_mipi_subdev_ops);
//	sensor->subdev.flags =
	sensor->subdev.owner = THIS_MODULE;
	sensor->subdev.dev = dev;
	v4l2_set_subdevdata(&sensor->subdev, sensor);
	snprintf(sensor->subdev.name, sizeof(sensor->subdev.name), "%s.%s",
		 KBUILD_MODNAME, dev_name(dev));
//	sensor->subdev.grp_id = 678;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0)
	retval = v4l2_async_register_subdev(&sensor->subdev);
#else
	retval = v4l2_async_register_subdev_sensor(&sensor->subdev);
#endif
	if (retval < 0) {
		dev_err(dev, "Async register failed (%d)\n", retval);
		return retval;
	}

	dev_info(dev, "v4l subdev registered\n");
	return retval;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,11,0)
static int sensor_mipi_remove(struct platform_device *pdev)
#else
static void sensor_mipi_remove(struct platform_device *pdev)
#endif
{
	struct sensor_mipi *sensor = platform_get_drvdata(pdev);

	v4l2_async_unregister_subdev(&sensor->subdev);

//	clk_disable_unprepare(sensor->sensor_clk);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,10,0)
	return 0;
#endif
}

#ifdef CONFIG_OF
static const struct of_device_id sensor_mipi_v2_of_match[] = {
	{ .compatible = "imago,sensor_mipi",
	},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, sensor_mipi_v2_of_match);
#endif

static struct platform_driver sensor_mipi_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = MODMODULENAME,
#ifdef CONFIG_OF
		  .of_match_table = of_match_ptr(sensor_mipi_v2_of_match),
#endif
		  },

	.probe	= sensor_mipi_probe,
	.remove	= sensor_mipi_remove,
};
module_platform_driver(sensor_mipi_driver);

MODULE_AUTHOR("IMAGO Technologies GmbH");
MODULE_DESCRIPTION("VisionSensor PV3 MIPI sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MODVERSION);
MODULE_ALIAS("CSI");
