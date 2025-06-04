/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_WRAPPER_H
#define STAI_MPU_WRAPPER_H

#include <string>
#include <vector>
#include <numeric>
#include <cassert>
#include <thread>

#include "stai_mpu_tensor.h"
#include "stai_mpu_types.h"
#include "stai_mpu_utils.h"

/**
 * @brief Class for representing a ST AI Wrapper class.
 */
class stai_mpu_wrapper {
public:
    /**
     * @brief Constructor for the stai_mpu_wrapper class.
     */
    stai_mpu_wrapper() {}

    /**
     * @brief Destructor for the stai_mpu_wrapper class.
     */
    virtual ~stai_mpu_wrapper() {}

    /**
     * @brief Loads a generic model from a file.
     *
     * @param model_path The path to the generic model file.
     * @param use_hw_acceleration Enable HW acceleration if available.
     */
    virtual void load_model(const std::string& model_path, bool use_hw_acceleration) = 0;

    /**
     * @brief Sets input data for a specific input tensor of the generic model based.
     *
     * @param index The index of the input tensor.
     * @param data A pointer to the input data.
     */
    virtual void set_input(int index, const void* data) = 0;

    /**
     * @brief Retrieves output data for a specific output tensor of the generic model.
     *
     * @param index The index of the output tensor.
     * @return A pointer to the output data.
     */
    virtual void* get_output(int index) = 0;

    /**
     * @brief Runs inference on the loaded generic model.
     *
     * @return True if inference was successful, false otherwise.
     */
    inline virtual bool run() = 0;

    /**
     * @brief Retrieves the number of input tensors of the generic model.
     *
     * @return The number of input tensors.
     */
    virtual int get_num_inputs() = 0;

    /**
     * @brief Retrieves the number of output tensors of the generic model.
     *
     * @return The number of output tensors.
     */
    virtual int get_num_outputs() = 0;

    /**
     * @brief Retrieves information about the input tensors of the generic model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the input tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_input_infos() const = 0;

    /**
     * @brief Retrieves information about the output tensors of the generic model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the output tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_output_infos() const = 0;

    /**
     * @brief Retrieves information about the stai backend engine used for inference.
     *
     * @return A stai_mpu_backend_engine referring to the type of the model.
     */
    virtual stai_mpu_backend_engine get_backend_engine() = 0;
};

typedef stai_mpu_wrapper* create_t();
typedef void destroy_t(stai_mpu_wrapper*);

#endif // STAI_MPU_WRAPPER_H