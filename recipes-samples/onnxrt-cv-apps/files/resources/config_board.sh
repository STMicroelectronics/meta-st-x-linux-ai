#!/bin/bash
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

COMPATIBLE=$(cat /proc/device-tree/compatible)
STM32MP135="stm32mp135"
STM32MP157="stm32mp157"
STM32MP157FEV1="stm32mp157f-ev1st"

if [[ "$COMPATIBLE" == *"$STM32MP135"* ]]; then
  MACHINE=$STM32MP135
  DWIDTH=320
  DHEIGHT=240
  DFPS=10
  COMPUTE_ENGINE=""
fi

if [[ "$COMPATIBLE" == *"$STM32MP157"* ]]; then
  if [[ "$COMPATIBLE" == *"$STM32MP157FEV1"* ]]; then
    MACHINE=$STM32MP157FEV1
    DWIDTH=320
    DHEIGHT=240
    DFPS=15
    COMPUTE_ENGINE=""
  else
    MACHINE=$STM32MP157
    DWIDTH=640
    DHEIGHT=480
    DFPS=15
    COMPUTE_ENGINE=""
  fi
fi

echo "machine used = "$MACHINE
