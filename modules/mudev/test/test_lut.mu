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

#include "test_precomp.hpp"

using namespace cv;
using namespace cv::musa;
using namespace cv::mudev;
using namespace cvtest;

////////////////////////////////////////////////////////////////////////////////
// LutTest

template <typename T>
class LutTest : public ::testing::Test
{
public:
    void test_gpumat()
    {
        const Size size = randomSizeMusa(100, 400);
        const int type = DataType<T>::type;

        Mat src = randomMatMusa(size, type);
        Mat tbl = randomMatMusa(Size(256, 1), type);

        GpuMat_<T> d_src(src), d_tbl(tbl);

        GpuMat_<T> dst = lut_(d_src, d_tbl);

        Mat dst_gold;
        cv::LUT(src, tbl, dst_gold);

        EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
    }
};

TYPED_TEST_CASE(LutTest, uchar);

TYPED_TEST(LutTest, GpuMat)
{
    LutTest<TypeParam>::test_gpumat();
}
