# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

LICENCE: BSD-3-Clause

This application shows the advanced edge AI and connectivity capabilities of the STM32MP2 MPU.
Hardware:
STM32MP2 microprocessor series with up to dual Arm Cortex®-A35 and Cortex®-M33 cores and a GPU/NPU accelerator up to 1.35 TOPS
application in detail:
   - Scene capture with a MIPI CSI2 camera.
   - Images processed by the internal ISP
   - Inference at the edge on NPU\n"
   - YoloV8n trained for person detection + Tracking (ByteTrack)
   - H.264 video encoding including AI results to the PC
   - The PC used for display only

This out-of-the box application has been developed using Python 3.8.
Pre-requisites:
A STM32MP25 board flashed with the OSTL v6.1.0 and X-LINUX-AI v6.1.0 expansion package.

For board setup support please refer to X-LINUX-AI wiki articles :  https://wiki.st.com/stm32mpu/wiki/Category:X-LINUX-AI_expansion_package

--- Running the Demo ---
A\ Using Dockerfile (RECOMMENDED)

1 - Build the docker:

    $PC> cd path/to/docker
    $PC> docker build --network=host --build-arg http_proxy --build-arg https_proxy --no-cache -t st_people_tracking_heatmap_host .

2 - Set a screen local link:

    $PC> xhost +local:

3 - Start the docker

    $PC> docker run --network=host -e DISPLAY=$DISPLAY  -v /tmp/.X11-unix:/tmp/.X11-unix:ro -it --rm --name my-running-app st_people_tracking_heatmap_host

3 - Connect the PC and the board
    Ensure the board is up and connected to the same network as the host PC.

    Write the host PC IP address and the board IP address in the "Host PC IP" and "Board IP" fields, respectively.

4 - The preview will appear after 10-20 seconds.