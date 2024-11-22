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

#define STAI_MPU_F16_EXPONENT_BITS 0x1F
#define STAI_MPU_F16_EXPONENT_BIAS 15
#define STAI_MPU_F16_EXPONENT_SHIFT 10
#define STAI_MPU_F16_MANTISSA_BITS ((1 << STAI_MPU_F16_EXPONENT_SHIFT) - 1)
#define STAI_MPU_F16_MANTISSA_SHIFT (23 - STAI_MPU_F16_EXPONENT_SHIFT)
#define STAI_MPU_F16_MAX_EXPONENT (STAI_MPU_F16_EXPONENT_BITS << STAI_MPU_F16_EXPONENT_SHIFT)

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
    STAI_MPU_QTYPE_STATIC_AFFINE,
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
        } static_affine;
    };
} stai_mpu_quant_params;

enum class stai_mpu_backend_engine {
    STAI_MPU_TFLITE_CPU_ENGINE,
    STAI_MPU_TFLITE_NPU_ENGINE,
    STAI_MPU_ORT_CPU_ENGINE,
    STAI_MPU_ORT_NPU_ENGINE,
    STAI_MPU_OVX_NPU_ENGINE,
};


#endif //STAI_MPU_TYPES_H