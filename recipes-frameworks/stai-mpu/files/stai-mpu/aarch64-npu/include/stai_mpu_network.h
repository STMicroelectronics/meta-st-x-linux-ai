/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_NETWORK_H_
#define STAI_MPU_NETWORK_H_


#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

#ifndef STAI_MPU_VERSION_MAJOR
#define STAI_MPU_VERSION_MAJOR "6"
#endif

#include "stai_mpu_wrapper.h"

/**
 * @brief A class that wraps Unified model inference functionality. \
 * This class provides an interface for loading a TFLite/Onnx/Nbg model, setting input data, \
 * running inference, and retrieving output data. It inherits from the stai_mpu_wrapper class. \
 * This class is used along with @ref stai_mpu_tensor "stai_mpu_tensor" which hosts the input and \
 * output tensors information.
 */
class stai_mpu_network {
public:
    /**
     * @brief Constructor for the stai_mpu_network class. This function takes care of identifying the \
     * model loaded into the constructor. The backend_engine attribute is then set to its associated \
     * stai_mpu_backend_engine enum value. The stai_mpu_wrapper is then instanciated accordingly by loading \
     * dynamically the associated plugin (shared library of the backend).
     */
    stai_mpu_network(const std::string& model_path, bool use_hw_acceleration);

    /**
     * @brief Constructor for the stai_mpu_network class without arguments.
     */
    stai_mpu_network();

    /**
     * @brief Destructor for the stai_mpu_network class.
     */
    ~stai_mpu_network();

    /**
     * @brief Loads a TFLite/Onnx/Nbg model from a file.
     *
     * @param model_path The path to the TFLite/Onnx/Nbg model file.
     * @param use_hw_acceleration Enable HW acceleration if available for the TFLite/Onnx/Nbg model.
     */
    virtual void load_model(const std::string& model_path, bool use_hw_acceleration);

    /**
     * @brief Sets input data for a specific input tensor of the TFLite/Onnx/Nbg model based.
     *
     * @param index The index of the input tensor.
     * @param data A pointer to the input data.
     * @throws std::runtime_error If the index is out of bounds or if there is an error in setting the input data.
     */
    virtual void set_input(int index, const void* data);

    /**
     * @brief Retrieves output data for a specific output tensor of the TFLite/Onnx/Nbg model. \
     * The caller function of the get_output must check if the backend engine is STAI_MPU_OVX_NPU_ENGINE \
     * and take responsibility of to free the memory when it's no longer needed.
     *
     * @param index The index of the output tensor.
     * @return A pointer to the output data.
     * @throws std::runtime_error If the index is out of bounds or if there is an error in retrieving the data.
     */
    virtual void* get_output(int index);

    /**
     * @brief Runs inference on the loaded TFLite/Onnx/Nbg  model.
     *
     * @return True if inference was successful, false otherwise.
     */
    virtual bool run();

    /**
     * @brief Retrieves the number of input tensors of the TFLite/Onnx/Nbg  model.
     *
     * @return The number of input tensors.
     */
    virtual int get_num_inputs();

    /**
     * @brief Retrieves the number of output tensors of the TFLite/Onnx/Nbg  model.
     *
     * @return The number of output tensors.
     */
    virtual int get_num_outputs();

    /**
     * @brief Retrieves information about the input tensors of the TFLite/Onnx/Nbg  model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the input tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_input_infos() const;

    /**
     * @brief Retrieves information about the output tensors of the TFLite/Onnx/Nbg  model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the output tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_output_infos() const;

    /**
     * @brief Retrieves information about the stai backend engine used for inference.
     *
     * @return A stai_mpu_backend_engine referring to the type of the model.
     */
    virtual stai_mpu_backend_engine get_backend_engine();

private:
    stai_mpu_wrapper* stai_mpu_wrapper_;
    std::string library_path_;
    void *lib_handle_;
    create_t* create_stai_mpu_network_;
    destroy_t* destroy_stai_mpu_network_;
};

#endif //STAI_MPU_NETWORK_H_