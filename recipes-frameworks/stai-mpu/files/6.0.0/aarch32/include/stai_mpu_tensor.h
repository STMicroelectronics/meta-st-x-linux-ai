/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#ifndef STAI_MPU_TENSOR_H
#define STAI_MPU_TENSOR_H

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>
#include "stai_mpu_types.h"
#include <dlfcn.h>

#define MAX_NUM_DIMS 6
#define USR_LIBDIR_PATH "/usr/lib/"

/**
 * @brief Class for representing a tensor used in the ST unified API. It contains all
 * the metadata and the internal informations of the tensor such as the name, the shape,
 * the data type and the quantization type and parameters. This class is exposed as an
 * interface for the user to allow accessing the input and the output tensors details.
 */
class stai_mpu_tensor {
public:
    /**
     * @brief Constructors for creating an stai_mpu_tensor object.
     * @param name The name of the tensor.
     * @param index The index of the tensor.
     * @param shape The shape of the tensor.
     * @param rank The rank of the tensor.
     * @param dtype The data type of the tensor.
     * @param qtype The quantization type of the tensor.
     * @param qparams The quantization paramaters of the tensor.
     */
    stai_mpu_tensor() {}

    stai_mpu_tensor(const std::string& name, int index, const std::vector<int>& shape,
                int rank, stai_mpu_dtype dtype)
              : name_(name), index_(index), shape_(shape), rank_(rank), dtype_(dtype),
                qtype_(stai_mpu_qtype::STAI_MPU_QTYPE_NONE), qparams_({0}) {}

    stai_mpu_tensor(const std::string& name, int index, const std::vector<int>& shape,
                int rank, stai_mpu_dtype dtype, stai_mpu_qtype qtype, stai_mpu_quant_params qparams)
              : name_(name), index_(index), shape_(shape), rank_(rank), dtype_(dtype),
                qtype_(qtype), qparams_(qparams){}

    // Getter functions

    /**
     * @brief Gets the name of the tensor.
     * @return The name of the tensor.
     */
    std::string get_name() const { return name_; }

    /**
     * @brief Gets the index of the tensor.
     * @return The index of the tensor.
     */
    int get_index() const { return index_; }

    /**
     * @brief Gets the shape of the tensor.
     * @return The shape of the tensor.
     */
    std::vector<int> get_shape() const { return shape_; }

    /**
     * @brief Gets the rank of the tensor.
     * @return The rank of the tensor.
     */
    int get_rank() const { return rank_; }

    /**
     * @brief Gets the data type of the tensor.
     * @return The data type of the tensor.
     */
    stai_mpu_dtype get_dtype() const { return dtype_; }

    /**
     * @brief Gets the quantization type of the tensor.
     * @return The quant type of the tensor.
     */
    stai_mpu_qtype get_qtype() const { return qtype_; }

    /**
     * @brief Gets the quantization parameters of the tensor.
     * @return The quant params of the tensor.
     */
    stai_mpu_quant_params get_qparams() const { return qparams_;}

private:
    /** \brief The name of the tensor. */
    std::string name_;
    /** \brief The index of the tensor. */
    int index_;
     /** \brief The shape of the tensor. */
    std::vector<int> shape_;
    /** \brief The rank (dims) of the tensor. */
    int rank_;
    /** \brief The data type of the tensor. */
    stai_mpu_dtype dtype_;
    /** \brief The quantization type of the tensor. */
    stai_mpu_qtype qtype_;
    /** \brief The quantization parameters of the tensor. */
    stai_mpu_quant_params qparams_; /** \brief */
};


#endif //STAI_MPU_TENSOR_H