#!/bin/bash
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

COMPATIBLE=$(cat /proc/device-tree/compatible)
SOFTWARE="AI_CPU"
STM32MP1="stm32mp1"
STM32MP2_CPU="stm32mp2x CPU only"
STM32MP135="stm32mp135"
STM32MP157="stm32mp157"
STM32MP157FEV1="stm32mp157f-ev1st"
STM32MP2="stm32mp2"

if [[ "$FRAMEWORK" == "nbg" ]]; then
  echo "NBG format is not supported in the X-LINUX-AI version installed, please use tflite or onnx"
  exit 1
elif [[ "$FRAMEWORK" == "tflite" ]]; then
  NN_EXT=".tflite"
elif [[ "$FRAMEWORK" == "onnx" ]]; then
  NN_EXT=".onnx"
else
  #define a default value if no framework is specified on CPU version TFLITE is used as default
  NN_EXT=".tflite"
fi

if [[ "$COMPATIBLE" == *"$STM32MP135"* ]]; then
  MACHINE=$STM32MP135
  DWIDTH=320
  DHEIGHT=240
  DFPS=10
  COMPUTE_ENGINE=""
  IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v1_0.5_128_quant$NN_EXT"
  IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet"
  IMAGE_CLASSIF_DATA="mobilenet/testdata/"
  OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v1_10_300$NN_EXT"
  OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset"
  OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
fi

if [[ "$COMPATIBLE" == *"$STM32MP157"* ]]; then
  if [[ "$COMPATIBLE" == *"$STM32MP157FEV1"* ]]; then
    MACHINE=$STM32MP157FEV1
    DWIDTH=320
    DHEIGHT=240
    DFPS=15
    COMPUTE_ENGINE=""
    IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v1_0.5_128_quant$NN_EXT"
    IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet"
    IMAGE_CLASSIF_DATA="mobilenet/testdata/"
    OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v1_10_300$NN_EXT"
    OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset"
    OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
  else
    MACHINE=$STM32MP157
    DWIDTH=640
    DHEIGHT=480
    DFPS=15
    COMPUTE_ENGINE=""
    IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v1_0.5_128_quant$NN_EXT"
    IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet"
    IMAGE_CLASSIF_DATA="mobilenet/testdata/"
    OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v1_10_300$NN_EXT"
    OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset"
    OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
  fi
fi


if [[ "$COMPATIBLE" == *"$STM32MP2"* ]]; then
  MACHINE=$STM32MP2_CPU
  DWIDTH=760
  DHEIGHT=568
  DFPS=30
  COMPUTE_ENGINE=""
  OPTIONS="--dual_camera_pipeline"
  IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v1_0.5_128_quant$NN_EXT"
  IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet"
  IMAGE_CLASSIF_DATA="mobilenet/testdata/"
  OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v1_10_300$NN_EXT"
  OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset"
  OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
fi

echo "machine used = "$MACHINE
