This kernel module implements the MIPI CSI driver for IMAGO's *VisionSensor PV3* using the NXP i.MX8M Mini SoC.
The module is based on NXP's driver but with some modifications.

The module also includes a dummy sensor device. The sensor is actually configured from userspace to allow
different sensor types without changing the device tree.

The module code is automatically analyzed by using *sparse* and *smatch*.

Current status:
[![Analyze kernel module](https://github.com/IMAGO-Technologies/vsmipi/actions/workflows/analyze.yml/badge.svg)](https://github.com/IMAGO-Technologies/vsmipi/actions/workflows/analyze.yml)
