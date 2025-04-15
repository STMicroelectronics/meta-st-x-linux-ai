/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_ORT_H
#define STAI_MPU_ORT_H

#include "stai_mpu_wrapper.h"

#include <memory>

#include "onnxruntime_cxx_api.h"
#include "core/providers/vsinpu/vsinpu_provider_factory.h"
#include "onnxruntime_session_options_config_keys.h"


std::nullptr_t t;
std::vector<int> stai_mpu_get_tensor_shape(const Ort::TypeInfo& type_info);
stai_mpu_dtype stai_mpu_map_dtype(const ONNXTensorElementDataType ort_type);
ONNXTensorElementDataType stai_mpu_unmap_dtype(const stai_mpu_dtype stai_mpu_dtype);

class stai_mpu_ort : public stai_mpu_wrapper {
public:
    /**
     * @brief Constructor for the stai_mpu_ort class.
     */
    stai_mpu_ort();

    /**
     * @brief Loads a Onnxruntime model from a file.
     *
     * @param model_path The path to the Onnxruntime model file.
     * @param use_hw_acceleration Enable HW acceleration if available.
     */
    virtual void  load_model(const std::string& model_path, bool use_hw_acceleration) override;

    /**
     * @brief Sets input data for a specific input tensor of the Onnxruntime model based.
     *
     * @param index The index of the input tensor.
     * @param data A pointer to the input data.
     */
    virtual void set_input(int index, const void* data) override;

    /**
     * @brief Retrieves output data for a specific output tensor of the Onnxruntime model.
     *
     * @param index The index of the output tensor.
     * @return A pointer to the output data.
     */
    virtual void* get_output(int index) override;

    /**
     * @brief Runs inference on the loaded Onnxruntime model.
     *
     * @return True if inference was successful, false otherwise.
     */
    inline virtual bool run() override;

    /**
     * @brief Retrieves the number of input tensors of the Onnxruntime model.
     *
     * @return The number of input tensors.
     */
    virtual int get_num_inputs() override;

    /**
     * @brief Retrieves the number of output tensors of the Onnxruntime model.
     *
     * @return The number of output tensors.
     */
    virtual int get_num_outputs() override;

    /**
     * @brief Retrieves information about the input tensors of the Onnxruntime model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the input tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_input_infos() const override;

    /**
     * @brief Retrieves information about the output tensors of the Onnxruntime model.
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
    /** \brief A unique pointer to the ONNX runtime session.  */
    Ort::Session session_;
    /** \brief The memory info used for allocating input and output tensors. */
    Ort::MemoryInfo memory_info_;
    /** \brief The allocator used for allocating input and output tensors. */
    Ort::AllocatorWithDefaultOptions allocator_;
    /** \brief A vector of input tensors. */
    std::vector<Ort::Value> input_tensors_;
    /** \brief A vector of output tensors. */
    std::vector<Ort::Value> output_tensors_;
    /** \brief A vector of input tensor sizes. */
    std::vector<int> input_size_;
    /** \brief A vector of output tensor sizes. */
    std::vector<int> output_size_;
    /** \brief Number of input nodes. */
    int num_inputs_;
    /** \brief Number of output nodes. */
	int num_outputs_;
    /** \brief A vector of input tensor names. */
    std::vector<char*> input_names_;
    /** \brief A vector of input tensor names ptr. */
    std::vector<Ort::AllocatedStringPtr> input_names_ptrs;
    /** \brief A vector of output tensor names. */
    std::vector<char*> output_names_;
    /** \brief A vector of output tensor names ptr. */
    std::vector<Ort::AllocatedStringPtr> output_names_ptrs;
    /** \brief The STAI Engine and backend used for inference. */
    stai_mpu_backend_engine stai_mpu_backend_;
};

#endif // STAI_MPU_ORT_H