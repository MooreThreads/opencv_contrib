/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
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

#ifdef HAVE_MUSA

namespace opencv_test { namespace {

////////////////////////////////////////////////////////////////////////////////
// Merge

PARAM_TEST_CASE(Merge, cv::musa::DeviceInfo, cv::Size, MatDepth, Channels, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int depth;
    int channels;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        depth = GET_PARAM(2);
        channels = GET_PARAM(3);
        useRoi = GET_PARAM(4);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(Merge, Accuracy)
{
    std::vector<cv::Mat> src;
    src.reserve(channels);
    for (int i = 0; i < channels; ++i)
        src.push_back(cv::Mat(size, depth, cv::Scalar::all(i)));

    std::vector<cv::musa::GpuMat> d_src;
    for (int i = 0; i < channels; ++i)
        d_src.push_back(musaLoadMat(src[i], useRoi));

    if (depth == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat dst;
            cv::musa::merge(d_src, dst);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat dst;
        cv::musa::merge(d_src, dst);

        cv::Mat dst_gold;
        cv::merge(src, dst_gold);

        EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, Merge, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_DEPTH,
    testing::Values(1, 2, 3, 4),
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// Split

PARAM_TEST_CASE(Split, cv::musa::DeviceInfo, cv::Size, MatDepth, Channels, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int depth;
    int channels;
    bool useRoi;

    int type;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        depth = GET_PARAM(2);
        channels = GET_PARAM(3);
        useRoi = GET_PARAM(4);

        cv::musa::setDevice(devInfo.deviceID());

        type = CV_MAKE_TYPE(depth, channels);
    }
};

MUSA_TEST_P(Split, Accuracy)
{
    cv::Mat src = randomMatMusa(size, type);

    if (depth == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            std::vector<cv::musa::GpuMat> dst;
            cv::musa::split(musaLoadMat(src), dst);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        std::vector<cv::musa::GpuMat> dst;
        cv::musa::split(musaLoadMat(src, useRoi), dst);

        std::vector<cv::Mat> dst_gold;
        cv::split(src, dst_gold);

        ASSERT_EQ(dst_gold.size(), dst.size());

        for (size_t i = 0; i < dst_gold.size(); ++i)
        {
            EXPECT_MAT_NEAR(dst_gold[i], dst[i], 0.0);
        }
    }
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, Split, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_DEPTH,
    testing::Values(1, 2, 3, 4),
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// Transpose

PARAM_TEST_CASE(Transpose, cv::musa::DeviceInfo, cv::Size, MatType, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int type;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        type = GET_PARAM(2);
        useRoi = GET_PARAM(3);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(Transpose, Accuracy)
{
    cv::Mat src = randomMatMusa(size, type);

    if (CV_MAT_DEPTH(type) == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat dst;
            cv::musa::transpose(musaLoadMat(src), dst);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat dst = createGpuMat(cv::Size(size.height, size.width), type, useRoi);
        cv::musa::transpose(musaLoadMat(src, useRoi), dst);

        cv::Mat dst_gold;
        cv::transpose(src, dst_gold);

        EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, Transpose, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8UC1),
                    MatType(CV_8UC4),
                    MatType(CV_16UC2),
                    MatType(CV_16SC2),
                    MatType(CV_32SC1),
                    MatType(CV_32SC2),
                    MatType(CV_64FC1)),
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// Flip

enum {FLIP_BOTH = 0, FLIP_X = 1, FLIP_Y = -1};
CV_ENUM(FlipCode, FLIP_BOTH, FLIP_X, FLIP_Y)
#define ALL_FLIP_CODES testing::Values(FlipCode(FLIP_BOTH), FlipCode(FLIP_X), FlipCode(FLIP_Y))

PARAM_TEST_CASE(Flip, cv::musa::DeviceInfo, cv::Size, MatType, FlipCode, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int type;
    int flip_code;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        type = GET_PARAM(2);
        flip_code = GET_PARAM(3);
        useRoi = GET_PARAM(4);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(Flip, Accuracy)
{
    cv::Mat src = randomMatMusa(size, type);

    cv::musa::GpuMat dst = createGpuMat(size, type, useRoi);
    cv::musa::flip(musaLoadMat(src, useRoi), dst, flip_code);

    cv::Mat dst_gold;
    cv::flip(src, dst_gold, flip_code);

    EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
}

MUSA_TEST_P(Flip, AccuracyInplace)
{
    cv::Mat src = randomMatMusa(size, type);
    bool isSizeOdd = ((size.width & 1) == 1) || ((size.height & 1) == 1);
    cv::musa::GpuMat srcDst = musaLoadMat(src, useRoi);
    if(isSizeOdd)
    {
        EXPECT_THROW(cv::musa::flip(srcDst, srcDst, flip_code), cv::Exception);
        return;
    }
    cv::musa::flip(srcDst, srcDst, flip_code);

    cv::Mat dst_gold;
    cv::flip(src, dst_gold, flip_code);

    EXPECT_MAT_NEAR(dst_gold, srcDst, 0.0);
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, Flip, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8UC1),
                    MatType(CV_8UC3),
                    MatType(CV_8UC4),
                    MatType(CV_16UC1),
                    MatType(CV_16UC3),
                    MatType(CV_16UC4),
                    MatType(CV_32SC1),
                    MatType(CV_32SC3),
                    MatType(CV_32SC4),
                    MatType(CV_32FC1),
                    MatType(CV_32FC3),
                    MatType(CV_32FC4)),
    ALL_FLIP_CODES,
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// LUT

PARAM_TEST_CASE(LUT, cv::musa::DeviceInfo, cv::Size, MatType, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int type;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        type = GET_PARAM(2);
        useRoi = GET_PARAM(3);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(LUT, OneChannel)
{
    cv::Mat src = randomMatMusa(size, type);
    cv::Mat lut = randomMatMusa(cv::Size(256, 1), CV_8UC1);

    cv::Ptr<cv::musa::LookUpTable> lutAlg = cv::musa::createLookUpTable(lut);

    cv::musa::GpuMat dst = createGpuMat(size, CV_MAKE_TYPE(lut.depth(), src.channels()));
    lutAlg->transform(musaLoadMat(src, useRoi), dst);

    cv::Mat dst_gold;
    cv::LUT(src, lut, dst_gold);

    EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
}

MUSA_TEST_P(LUT, MultiChannel)
{
    cv::Mat src = randomMatMusa(size, type);
    cv::Mat lut = randomMatMusa(cv::Size(256, 1), CV_MAKE_TYPE(CV_8U, src.channels()));

    cv::Ptr<cv::musa::LookUpTable> lutAlg = cv::musa::createLookUpTable(lut);

    cv::musa::GpuMat dst = createGpuMat(size, CV_MAKE_TYPE(lut.depth(), src.channels()), useRoi);
    lutAlg->transform(musaLoadMat(src, useRoi), dst);

    cv::Mat dst_gold;
    cv::LUT(src, lut, dst_gold);

    EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, LUT, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8UC1), MatType(CV_8UC3)),
    WHOLE_SUBMAT));

//////////////////////////////////////////////////////////////////////////////
// CopyMakeBorder

namespace
{
    IMPLEMENT_PARAM_CLASS(Border, int)
}

PARAM_TEST_CASE(CopyMakeBorder, cv::musa::DeviceInfo, cv::Size, MatType, Border, BorderType, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int type;
    int border;
    int borderType;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        type = GET_PARAM(2);
        border = GET_PARAM(3);
        borderType = GET_PARAM(4);
        useRoi = GET_PARAM(5);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(CopyMakeBorder, Accuracy)
{
    cv::Mat src = randomMatMusa(size, type);
    cv::Scalar val = randomScalarMusa(0, 255);

    cv::musa::GpuMat dst = createGpuMat(cv::Size(size.width + 2 * border, size.height + 2 * border), type, useRoi);
    cv::musa::copyMakeBorder(musaLoadMat(src, useRoi), dst, border, border, border, border, borderType, val);

    cv::Mat dst_gold;
    cv::copyMakeBorder(src, dst_gold, border, border, border, border, borderType, val);

    EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
}

INSTANTIATE_TEST_CASE_P(MUSA_Arithm, CopyMakeBorder, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8UC1),
                    MatType(CV_8UC3),
                    MatType(CV_8UC4),
                    MatType(CV_16UC1),
                    MatType(CV_16UC3),
                    MatType(CV_16UC4),
                    MatType(CV_32FC1),
                    MatType(CV_32FC3),
                    MatType(CV_32FC4)),
    testing::Values(Border(1), Border(10), Border(50)),
    ALL_BORDER_TYPES,
    WHOLE_SUBMAT));


}} // namespace
#endif // HAVE_MUSA
