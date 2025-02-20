/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                          License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Copyright (C) 2013, OpenCV Foundation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#pragma once

#ifndef OPENCV_MUDEV_PTR2D_CONSTANT_HPP
#define OPENCV_MUDEV_PTR2D_CONSTANT_HPP

#include "../common.hpp"
#include "traits.hpp"

namespace cv { namespace mudev {

//! @addtogroup mudev
//! @{

template <typename T> struct ConstantPtr
{
    typedef T   value_type;
    typedef int index_type;

    T value;

    __device__ __forceinline__ T operator ()(int, int) const { return value; }
};

template <typename T> struct ConstantPtrSz : ConstantPtr<T>
{
    int rows, cols;
};

template <typename T>
__host__ ConstantPtr<T> constantPtr(T value)
{
    ConstantPtr<T> p;
    p.value = value;
    return p;
}

template <typename T> ConstantPtrSz<T>
__host__ constantPtr(T value, int rows, int cols)
{
    ConstantPtrSz<T> p;
    p.value = value;
    p.rows = rows;
    p.cols = cols;
    return p;
}

template <typename T> struct PtrTraits< ConstantPtrSz<T> > : PtrTraitsBase< ConstantPtrSz<T>, ConstantPtr<T> >
{
};

//! @}

}}

#endif
