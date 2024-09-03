/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_TYPES_H
#define STAI_MPU_TYPES_H

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>

#ifdef STM32MP2_PLATFORM
#include <VX/vx_khr_nn.h>
#endif

#define MAX_NUM_DIMS 6

#define STAI_MPU_NON_SIGN_BITS 0x7fff
#define STAI_MPU_SIGN_BIT 0x8000
#define STAI_MPU_FLOAT_EXPONENT 0x7c00
#define STAI_MPU_FLOAT_BIAS 0x38000000


/**
 * @brief An enumeration of the supported data type
 */
enum stai_mpu_dtype {
    STAI_MPU_DTYPE_INT8,
    STAI_MPU_DTYPE_INT16,
    STAI_MPU_DTYPE_INT32,
    STAI_MPU_DTYPE_INT64,
    STAI_MPU_DTYPE_UINT8,
    STAI_MPU_DTYPE_UINT16,
    STAI_MPU_DTYPE_UINT32,
    STAI_MPU_DTYPE_UINT64,
    STAI_MPU_DTYPE_BOOL8,
    STAI_MPU_DTYPE_CHAR,
    STAI_MPU_DTYPE_BFLOAT16,
    STAI_MPU_DTYPE_FLOAT16,
    STAI_MPU_DTYPE_FLOAT32,
    STAI_MPU_DTYPE_FLOAT64,
    STAI_MPU_DTYPE_UNDEFINED,
};

/**
 * @brief An enumeration of the supported quantization type
 */
enum stai_mpu_qtype {
    STAI_MPU_QTYPE_DYNAMIC_FIXED_POINT,
    STAI_MPU_QTYPE_AFFINE_PER_CHANNEL,
    STAI_MPU_QTYPE_AFFINE_PER_TENSOR,
    STAI_MPU_QTYPE_NONE,
};

/**
 * @brief A struct that contains quantization parameters
 */
typedef struct _stai_mpu_quant_params{
    union {
        struct {
            /** \brief Specifies the fixed point position when the input element type is int16/int8, if 0 calculations are performed in integer math */
            int8_t fixed_point_pos;
        } dfp;

        struct {
            /** \brief Scale value for the quantized value */
            float scale;
            /** \brief  A 32 bit integer, in range [0, 255] */
            uint32_t zero_point;
        } affine_per_tensor;

        struct {
            /** \brief a 32 bit unsigned integer indicating channel dimension */
            uint32_t quant_dim;
            /** \brief an array of positive 32 bit floating point value. */
            const float* scales;
            /** \brief  A 32 bit integer, in range [0, 255] */
            const int32_t* zero_points;
        } affine_per_channel;
    };
} stai_mpu_quant_params;

enum class stai_mpu_backend_engine {
    STAI_MPU_TFLITE_CPU_ENGINE,
    STAI_MPU_ORT_CPU_ENGINE,
    STAI_MPU_OVX_NPU_ENGINE,
};

std::string stai_mpu_map_dtype_str(stai_mpu_dtype data_type) {
  switch (data_type) {
    case stai_mpu_dtype::STAI_MPU_DTYPE_INT8:
      return "int8";
    case stai_mpu_dtype::STAI_MPU_DTYPE_UINT8:
      return "uint8";
    case stai_mpu_dtype::STAI_MPU_DTYPE_BOOL8:
      return "bool";
    case stai_mpu_dtype::STAI_MPU_DTYPE_INT16:
      return "int16";
    case stai_mpu_dtype::STAI_MPU_DTYPE_UINT16:
      return "uint16";
    case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16:
      return "float16";
    case stai_mpu_dtype::STAI_MPU_DTYPE_BFLOAT16:
      return "bfloat16";
    case stai_mpu_dtype::STAI_MPU_DTYPE_INT32:
      return "int32";
    case stai_mpu_dtype::STAI_MPU_DTYPE_UINT32:
      return "uint32";
    case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32:
      return "float32";
    case stai_mpu_dtype::STAI_MPU_DTYPE_INT64:
      return "int64";
    case stai_mpu_dtype::STAI_MPU_DTYPE_UINT64:
      return "uint64";
    case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT64:
      return "float32";
    case stai_mpu_dtype::STAI_MPU_DTYPE_UNDEFINED:
    default:
      return "unknown";
  }
}

/**
 * @brief Helper function to allow mapping between the stai_mpu_qtype \
 * and the str quantization type.
 * @param quant_type the stai_mpu_qtype
 */
std::string stai_mpu_map_qtype_str(stai_mpu_qtype quant_type) {
  switch (quant_type) {
    case stai_mpu_qtype::STAI_MPU_QTYPE_DYNAMIC_FIXED_POINT:
      return "dynamicFixedPoint";
    case stai_mpu_qtype::STAI_MPU_QTYPE_AFFINE_PER_TENSOR:
      return "affinePerTensor";
    case stai_mpu_qtype::STAI_MPU_QTYPE_AFFINE_PER_CHANNEL:
      return "affinePerChannel";
    case stai_mpu_qtype::STAI_MPU_QTYPE_NONE:
    default:
      return "none";
  }
}


#endif //STAI_MPU_TYPES_H