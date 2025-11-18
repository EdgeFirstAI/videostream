Quick Start
===========

The Quick Start chapter will walk you through installation of the VisionPack
software, which includes VAAL, onto an NXP i.MX 8M Plus EVK.  Additional
platforms are supported, please contact support@au-zone.com or visit our support
site at https://support.deepviewml.com to inquire about the latest supported
platforms.

Installation
------------

DeepView VisionPack is available through various packaging options depending on
technical requirements, this section documents the bundled package installation
otherwise refer to the installation chapter for a description of other supported
options.

A self-extracting bundled archive is provided to quickly install VisionPack
along with dependencies plus documentation and examples.  Simply run these two
commands and accept the EULA then the bundle will be extracted to a local folder
named `visionpack-latest`.

wget https://deepviewml.com/vpk/visionpack-latest-armv8.sh
sh visionpack-latest-armv8.sh

Hello World
-----------

The first "hello world" example demonstrates how to split a GStreamer pipeline
into two with frame passing using VideoStream Library.
