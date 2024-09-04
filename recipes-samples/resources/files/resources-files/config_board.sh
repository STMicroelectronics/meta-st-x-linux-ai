#!/bin/bash
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

COMPATIBLE=$(cat /proc/device-tree/compatible)
STM32MP1="stm32mp1"
STM32MP2="stm32mp2"
STM32MP135="stm32mp135"
STM32MP157="stm32mp157"
STM32MP157FEV1="stm32mp157f-ev1st"
STM32MP257="stm32mp257"
STM32MP257FEV1="stm32mp257f-ev1st"

if [[ "$FRAMEWORK" == "nbg" ]]; then
  NN_EXT=".nb"
elif [[ "$FRAMEWORK" == "tflite" ]]; then
  NN_EXT=".tflite"
elif [[ "$FRAMEWORK" == "onnx" ]]; then
  NN_EXT=".onnx"
else
  #define a default value if no framework is specified
  if [[ "$COMPATIBLE" == *"$STM32MP1"* ]]; then
    NN_EXT=".tflite"
  else
    NN_EXT=".nb"
  fi
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
  FACE_DETECTION_MODEL="blazeface/blazeface_128x128_quant$NN_EXT"
  FACE_DETECTION_DATA="blazeface/testdata/"
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
    FACE_DETECTION_MODEL="blazeface/blazeface_128x128_quant$NN_EXT"
    FACE_DETECTION_DATA="blazeface/testdata/"
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
    FACE_DETECTION_MODEL="blazeface/blazeface_128x128_quant$NN_EXT"
    FACE_DETECTION_DATA="blazeface/testdata/"
  fi
fi

if [[ "$COMPATIBLE" == *"$STM32MP257"* ]]; then
  SEMANTIC_SEGMENTATION_MODEL="deeplabv3/deeplabv3_257_int8_per_tensor$NN_EXT"
  SEMANTIC_SEGMENTATION_LABEL="deeplabv3/labels_pascalvoc"
  SEMANTIC_SEGMENTATION_DATA="deeplabv3/testdata/"
  POSE_ESTIMATION_DATA="yolov8n_pose/testdata/"
  POSE_ESTIMATION_MODEL="yolov8n_pose/yolov8n_256_quant_pt_uf_pose_coco-st$NN_EXT"
  FACE_DETECTION_MODEL="blazeface/blazeface_128x128_quant$NN_EXT"
  FACE_DETECTION_DATA="blazeface/testdata/"
  FACE_RECO_MODEL="facenet/facenet512_160x160_quant$NN_EXT"
  FACE_RECO_DATA="facenet/testdata/"
  FACE_DATABASE="database/"
  if [[ "$COMPATIBLE" == *"$STM32MP257FEV1"* ]]; then
    MACHINE=$STM32MP257FEV1
    DWIDTH=760
    DHEIGHT=568
    DFPS=30
    COMPUTE_ENGINE="--npu"
    IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v2_1.0_224_int8_per_tensor$NN_EXT"
    IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet_2012"
    IMAGE_CLASSIF_DATA="mobilenet/testdata/"
    if [[ "$NN_EXT" == ".nb" ]]; then
      OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v2_fpnlite_10_256_int8_per_tensor$NN_EXT"
      OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset_80"
    else
      OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v2_fpnlite_10_256_int8$NN_EXT"
      OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset_80"
    fi
    OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
    OPTIONS="--dual_camera_pipeline"
  else
    MACHINE=$STM32MP257
    DWIDTH=640
    DHEIGHT=480
    DFPS=30
    COMPUTE_ENGINE="--npu"
    IMAGE_CLASSIFICATION_MODEL="mobilenet/mobilenet_v2_1.0_224_int8_per_tensor$NN_EXT"
    IMAGE_CLASSIFICATION_LABEL="mobilenet/labels_imagenet_2012"
    IMAGE_CLASSIF_DATA="mobilenet/testdata/"
    if [[ "$NN_EXT" == ".nb" ]]; then
      OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v2_fpnlite_10_256_int8_per_tensor$NN_EXT"
      OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset_80"
    else
      OBJ_DETEC_MODEL="coco_ssd_mobilenet/ssd_mobilenet_v2_fpnlite_10_256_int8$NN_EXT"
      OBJ_DETEC_MODEL_LABEL="coco_ssd_mobilenet/labels_coco_dataset_80"
    fi
    OBJ_DETECT_DATA="coco_ssd_mobilenet/testdata/"
    OPTIONS="--dual_camera_pipeline"
  fi
fi

echo "machine used = "$MACHINE
