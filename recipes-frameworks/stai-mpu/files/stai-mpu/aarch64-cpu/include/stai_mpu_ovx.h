/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_OVX_H_
#define STAI_MPU_OVX_H_


#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <VX/vx.h>
#include <VX/vx_khr_nn.h>
#include <VX/vx_khr_import_kernel.h>

#include "stai_mpu_wrapper.h"

vx_int16 stai_mpu_fp32_to_fp16(vx_float32 input_value);
vx_float32 stai_mpu_fp16_to_fp32(const vx_uint16 input_buffer);

/**
 * @brief A class that wraps NBG model inference functionality.
 *
 * This class provides an interface for loading a Network binary graph model, setting input data,
 * running inference, and retrieving output data. It inherits from the stai_mpu_wrapper class and wraps
 * around OpenVX framework to provide all the required functionalities for inferencing.
 */
class stai_mpu_ovx : public stai_mpu_wrapper {
public:
    /**
     * @brief Constructor for the stai_mpu_ovx class.
     */
    stai_mpu_ovx();

    /**
     * @brief Destructor for the stai_mpu_ovx class.
     */
    ~stai_mpu_ovx();

    /**
     * @brief Loads a NBG model from a file.
     *
     * @param model_path The path to the NBG model file.
     * @param use_hw_acceleration Enable HW acceleration if available.
     */
    virtual void load_model(const std::string& model_path, bool use_hw_acceleration) override;

    /**
     * @brief Sets input data for a specific input tensor of the NBG model based.
     *
     * @param index The index of the input tensor.
     * @param data A pointer to the input data.
     */
    virtual void set_input(int index, const void* data) override;

    /**
     * @brief Retrieves output data for a specific output tensor of the NBG model.
     *
     * @param index The index of the output tensor.
     * @return A pointer to the output data.
     */
    virtual void* get_output(int index) override;

    /**
     * @brief Runs inference on the loaded NBG model.
     *
     * @return True if inference was successful, false otherwise.
     */
    virtual bool run() override;

    /**
     * @brief Retrieves the number of input tensors of the NBG model.
     *
     * @return The number of input tensors.
     */
    virtual int get_num_inputs() override;

    /**
     * @brief Retrieves the number of output tensors of the NBG model.
     *
     * @return The number of output tensors.
     */
    virtual int get_num_outputs() override;

    /**
     * @brief Retrieves information about the input tensors of the NBG model.
     *
     * @return A vector of stai_mpu_tensor objects containing information about the input tensors.
     */
    virtual std::vector<stai_mpu_tensor> get_input_infos() const override;

    /**
     * @brief Retrieves information about the output tensors of the NBG model.
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
    /** \brief The OpenVX context for management of all OpenVX objects. */
    vx_context context_;
    /** \brief The OpenVX graph for execution. */
    vx_graph graph_;
    /** \brief The OpenVX NBG node. */
    vx_node nbg_node_;
    /** \brief The OpenVX NBG kernel. */
    vx_kernel nbg_kernel_;
    /** \brief The NBG buffer. */
    std::vector<char> nbg_buffer_;
    /** \brief  Vector of input tensors */
    std::vector<vx_tensor> input_tensors_;
    /** \brief  Vector of output tensors */
    std::vector<vx_tensor> output_tensors_;
    /** \brief Number of input nodes. */
    int num_inputs_;
    /** \brief Number of output nodes. */
	int num_outputs_;
    /** \brief The STAI Engine and backend used for inference. */
    stai_mpu_backend_engine stai_mpu_backend_;

    /* Method to get the num of bytes depending on the data types*/
    int stai_mpu_get_dtype_bytes(stai_mpu_dtype dtype)
    {
        switch(dtype){
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT8:
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT8:
            case stai_mpu_dtype::STAI_MPU_DTYPE_BOOL8:
            case stai_mpu_dtype::STAI_MPU_DTYPE_CHAR:
                return 1;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT16:
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT16:
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16:
            case stai_mpu_dtype::STAI_MPU_DTYPE_BFLOAT16:
                return 2;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT32:
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT32:
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32:
                return 4;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT64:
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT64:
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT64:
                return 8;
            default:
                printf("Not support format:%d,line=%d\n", dtype,__LINE__);
        }
        return 0;
    }

    /* Method to switch from */
    stai_mpu_dtype stai_mpu_map_dtype(vx_enum vx_type) const {
        switch (vx_type) {
            case VX_TYPE_INT8:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT8;
            case VX_TYPE_INT16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT16;
            case VX_TYPE_INT32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT32;
            case VX_TYPE_INT64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_INT64;
            case VX_TYPE_UINT8:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT8;
            case VX_TYPE_UINT16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT16;
            case VX_TYPE_UINT32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT32;
            case VX_TYPE_UINT64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UINT64;
            case VX_TYPE_BOOL:
                return stai_mpu_dtype::STAI_MPU_DTYPE_BOOL8;
            case VX_TYPE_CHAR:
                return stai_mpu_dtype::STAI_MPU_DTYPE_CHAR;
            case VX_TYPE_BFLOAT16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_BFLOAT16;
            case VX_TYPE_FLOAT16:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16;
            case VX_TYPE_FLOAT32:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32;
            case VX_TYPE_FLOAT64:
                return stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT64;
            default:
                return stai_mpu_dtype::STAI_MPU_DTYPE_UNDEFINED;
        }
    }

    /* Method to switch from */
    vx_enum stai_mpu_map_vx_type(stai_mpu_dtype dtype)
    {
        switch (dtype) {
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT8:
                return VX_TYPE_INT8;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT16:
                return VX_TYPE_INT16;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT32:
                return VX_TYPE_INT32;
            case stai_mpu_dtype::STAI_MPU_DTYPE_INT64:
                return VX_TYPE_INT64;
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT8:
                return VX_TYPE_UINT8;
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT16:
                return VX_TYPE_UINT16;
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT32:
                return VX_TYPE_UINT32;
            case stai_mpu_dtype::STAI_MPU_DTYPE_UINT64:
                return VX_TYPE_UINT64;
            case stai_mpu_dtype::STAI_MPU_DTYPE_BOOL8:
                return VX_TYPE_BOOL8;
            case stai_mpu_dtype::STAI_MPU_DTYPE_CHAR:
                return VX_TYPE_CHAR;
            case stai_mpu_dtype::STAI_MPU_DTYPE_BFLOAT16:
                return VX_TYPE_BFLOAT16;
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT16:
                return VX_TYPE_FLOAT16;
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT32:
                return VX_TYPE_FLOAT32;
            case stai_mpu_dtype::STAI_MPU_DTYPE_FLOAT64:
                return VX_TYPE_FLOAT64;
            case stai_mpu_dtype::STAI_MPU_DTYPE_UNDEFINED:
            default:
                throw std::invalid_argument("Invalid stai_mpu_dtype");
        }
    }

    /* Method to switch from */
    vx_enum stai_mpu_map_vx_qtype(stai_mpu_qtype qtype)
    {
        switch (qtype) {
            case stai_mpu_qtype::STAI_MPU_QTYPE_DYNAMIC_FIXED_POINT:
                return VX_QUANT_DYNAMIC_FIXED_POINT;
            case stai_mpu_qtype::STAI_MPU_QTYPE_STATIC_AFFINE:
                return VX_QUANT_AFFINE_SCALE;
            case stai_mpu_qtype::STAI_MPU_QTYPE_NONE:
                return VX_QUANT_NONE;
            default:
                throw std::invalid_argument("Invalid stai_mpu_qtype");
        }
    }

    /**
     * @brief Helper function to allow mapping between the
     * vx_tensor_quant_param and the stai_mpu_quant_params.
     * @param qparams an stai_mpu_quant_params
     * @param vx_qparams an vx_tensor_quant_param
     * @param vx_qparams quantization type stai_mpu_qtype
     */
    void stai_mpu_map_vx_qparams(stai_mpu_quant_params* qparams,
                            vx_tensor_quant_param* vx_qparams,
                            stai_mpu_qtype qtype) {
        switch(qtype){
            case stai_mpu_qtype::STAI_MPU_QTYPE_NONE:
                break;
            case stai_mpu_qtype::STAI_MPU_QTYPE_STATIC_AFFINE:
                vx_qparams->affine.scale = qparams->static_affine.scale;
                vx_qparams->affine.zeroPoint = qparams->static_affine.zero_point;
                break;
            case stai_mpu_qtype::STAI_MPU_QTYPE_DYNAMIC_FIXED_POINT:
                vx_qparams->dfp.fixed_point_pos = qparams->dfp.fixed_point_pos;
                break;
            default:
                printf("[OVX] Quant format %u is not supported!", qtype);
                break;
            }
    }

    vx_tensor stai_mpu_create_tensor(vx_context context, const stai_mpu_tensor& tensor_info)
    {
        std::vector<int> tensor_shape = tensor_info.get_shape();
        std::vector<vx_uint32> shape(tensor_info.get_rank());
        std::vector<int> vx_shape(tensor_shape.rbegin(), tensor_shape.rend());
        std::transform(vx_shape.begin(), vx_shape.end(), shape.begin(),
                         [](int i) { return static_cast<vx_uint32>(i); });
        vx_tensor_quant_param vx_qparams;
        stai_mpu_quant_params stai_mpu_qparams = tensor_info.get_qparams();
        stai_mpu_map_vx_qparams(&stai_mpu_qparams, &vx_qparams, tensor_info.get_qtype());
        vx_tensor_create_params_t tensor_create_params = {
            .num_of_dims = static_cast<uint32_t>(tensor_info.get_rank()),
            .sizes = shape.data(),
            .data_format = stai_mpu_map_vx_type(tensor_info.get_dtype()),
            .quant_format = stai_mpu_map_vx_qtype(tensor_info.get_qtype()),
            .quant_data = vx_qparams,
        };
        vx_tensor created_tensor = vxCreateTensor2(context, &tensor_create_params,
                                                sizeof(tensor_create_params));
        if (created_tensor == nullptr) {
            throw std::runtime_error("[OVX] Failed to create vx tensor.");
        }

        return created_tensor;
    }
};

#endif //STAI_MPU_OVX_H