/*
 * VisionSensor PV3 MIPI sensor driver
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.*
 */

#ifndef VSMIPI_H_
#define VSMIPI_H_

#define MODMODULENAME "vsmipi"

#define VSMIPI_V4L2_IOCTL_GET_VERSION	_IOR('V', BASE_VIDIOC_PRIVATE + 0, char[32])
#define VSMIPI_V4L2_IOCTL_SET_CSI_FREQ	_IOW('V', BASE_VIDIOC_PRIVATE + 1, s64)

#endif /* VSMIPI_H_ */

