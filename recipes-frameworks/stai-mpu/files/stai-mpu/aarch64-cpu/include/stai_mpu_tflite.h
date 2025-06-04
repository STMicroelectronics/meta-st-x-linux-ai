/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_TFLITE_H_
#define STAI_MPU_TFLITE_H_

#include "stai_mpu_wrapper.h"

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/c/c_api_types.h"

#ifdef VSI_OP
#include "VX/vsi_npu_custom_op.h"
#include "tensorflow/lite/delegates/external/external_delegate.h"
#include "tensorflow/lite/interpreter.h"
#endif

/**
 * @brief A class that wraps TensorFlow Lite model inference functionality.
 *
 * This class provides an interface for loading a TensorFlow Lite model, setting input data,
 * running inference, and retrieving output data. It inherits from the stai_mpu_wrapper class.
 */
class stai_mpu_tflite : public stai_mpu_wrapper {
public:
    /**
     * @brief Constructor for the stai_mpu_tflite class.
     */
    stai_mpu_tflite();

    /**
     * @brief Destructor for the stai_mpu_tflite class.
     */
    ~stai_mpu_tflite();

    /**
     * @brief Loads a TensorFlow Lite model from a file.
     *
     * @param model_path The path to the TensorFlow Lite model file.
     * @param use_hw_acceleration Enable HW acceleration if available.
     */
    virtual void  load_model(const std::string& model_path, bool use_hw_acceleration) override;

    /**
     * @brief Sets input data for a specific input tensor of the TensorFlow Lite model based.
     *
     * @param index The index of the input tensor.
     * @param data A pointer to the input data.
     */
    virtual void set_input(int index, const void* data) override;

    /**
     * @brief Retrieves output data for a specific output tensor of the TensorFlow Lite model.
     *
     * @param index The index of the output tensor.
     * @return A pointer to the output data.
     */
    virtual void* get_output(int index) override;

    /**
     * @brief Runs inference on the loaded TensorFlow Lite model.
     *
     * @return True if inference was successful, false otherwise.
     */
    inline virtual bool run() override;

    /**
     * @brief Retrieves the number of input tensors of the TensorFlow Lite model.
     *
     * @return The number of input tensors.
     */
    virtual int get_num_inputs() override;

    /**
     * @brief Retrieves the number of output tensors of the TensorFlow Lite model.
     *
     * @return The number of output tensors.
     */
    virtual int get_num_outputs() override;

    /**
     * @brief Retrieves information about the input tensors of the TensorFlow Lite model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the input tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_input_infos() const override;

    /**
     * @brief Retrieves information about the output tensors of the TensorFlow Lite model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the output tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_output_infos() const override;

    /**
     * @brief Retrieves information about the stai backend engine used for inference.
     *
     * @return A stai_mpu_backend_engine referring to the type of the model.
     */
    virtual stai_mpu_backend_engine get_backend_engine() override;

private:
    /** \brief A unique pointer to the TensorFlow Lite model. */
    std::unique_ptr<tflite::FlatBufferModel> model_;
    /** \brief A unique pointer to the TensorFlow Lite interpreter. */
    std::unique_ptr<tflite::Interpreter> interpreter_;
    /** \brief A vector of input tensor indices. */
    std::vector<int> input_indices_;
    /** \brief A vector of output tensor indices. */
    std::vector<int> output_indices_;
    /** \brief A vector of input tensor sizes. */
    std::vector<int> input_size_;
    /** \brief Number of input tensors. */
    int num_inputs_;
    /** \brief Number of output tensors. */
    int num_outputs_;
    /** \brief The STAI Engine and backend used for inference. */
    stai_mpu_backend_engine stai_mpu_backend_;

    stai_mpu_dtype stai_mpu_map_dtype(const TfLiteType tflite_type) const {
        switch (tflite_type) {
            case TfLiteType::kTfLiteInt8:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT8;
            case TfLiteType::kTfLiteInt16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT16;
            case TfLiteType::kTfLiteInt32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT32;
            case TfLiteType::kTfLiteInt64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT64;
            case TfLiteType::kTfLiteUInt8:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT8;
            case TfLiteType::kTfLiteUInt16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT16;
            case TfLiteType::kTfLiteUInt32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT32;
            case TfLiteType::kTfLiteUInt64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT64;
            case TfLiteType::kTfLiteBool:
                return stai_mpu_dtype::STAI_MPU_DTYPE_BOOL8;
            case TfLiteType::kTfLiteString:
                return stai_mpu_dtype::STAI_MPU_DTYPE_CHAR;
            case TfLiteType::kTfLiteFloat16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16;
            case TfLiteType::kTfLiteFloat32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32;
            case TfLiteType::kTfLiteFloat64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT64;
            case TfLiteType::kTfLiteResource:
            case TfLiteType::kTfLiteInt4:
            case TfLiteType::kTfLiteComplex64:
            case TfLiteType::kTfLiteComplex128:
            case TfLiteType::kTfLiteNoType:
            default:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UNDEFINED;
        }
    }

    stai_mpu_qtype stai_mpu_map_qtype(const TfLiteQuantizationType tflite_qtype, TfLiteTensor* tensor) const {
        switch (tflite_qtype) {
            case TfLiteQuantizationType::kTfLiteAffineQuantization:
            {
                auto* quant_params =
                        reinterpret_cast<TfLiteAffineQuantization*>(tensor->quantization.params);
                if (quant_params->scale) {
                    // per-channel quantization along the specified dimension
                    return stai_mpu_qtype::STAI_MPU_QTYPE_STATIC_AFFINE;
                } else {
                    // per-tensor quantization
                    return stai_mpu_qtype::STAI_MPU_QTYPE_NONE;
                }
            }
            case TfLiteQuantizationType::kTfLiteNoQuantization:
            default:
                return stai_mpu_qtype::STAI_MPU_QTYPE_NONE;
        }
    }
};

#endif