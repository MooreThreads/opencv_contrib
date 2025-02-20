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

#include "opencv2/core/musa.hpp"
#include "opencv2/ts/musa_test.hpp"

namespace opencv_test { namespace {

////////////////////////////////////////////////////////////////////////////////
// SetTo

PARAM_TEST_CASE(GpuMat_SetTo, cv::musa::DeviceInfo, cv::Size, MatType, UseRoi)
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

MUSA_TEST_P(GpuMat_SetTo, Zero)
{
    cv::Scalar zero = cv::Scalar::all(0);

    cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
    mat.setTo(zero);

    EXPECT_MAT_NEAR(cv::Mat::zeros(size, type), mat, 0.0);
}

MUSA_TEST_P(GpuMat_SetTo, SameVal)
{
    cv::Scalar val = cv::Scalar::all(randomDoubleMusa(0.0, 255.0));

    if (CV_MAT_DEPTH(type) == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
            mat.setTo(val);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
        mat.setTo(val);

        EXPECT_MAT_NEAR(cv::Mat(size, type, val), mat, 0.0);
    }
}

MUSA_TEST_P(GpuMat_SetTo, DifferentVal)
{
    cv::Scalar val = randomScalarMusa(0.0, 255.0);

    if (CV_MAT_DEPTH(type) == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
            mat.setTo(val);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
        mat.setTo(val);

        EXPECT_MAT_NEAR(cv::Mat(size, type, val), mat, 0.0);
    }
}

MUSA_TEST_P(GpuMat_SetTo, Masked)
{
    cv::Scalar val = randomScalarMusa(0.0, 255.0);
    cv::Mat mat_gold = randomMatMusa(size, type);
    cv::Mat mask = randomMatMusa(size, CV_8UC1, 0.0, 2.0);

    if (CV_MAT_DEPTH(type) == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat mat = createGpuMat(size, type, useRoi);
            mat.setTo(val, musaLoadMat(mask));
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat mat = musaLoadMat(mat_gold, useRoi);
        mat.setTo(val, musaLoadMat(mask, useRoi));

        mat_gold.setTo(val, mask);

        EXPECT_MAT_NEAR(mat_gold, mat, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA, GpuMat_SetTo, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_TYPES,
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// CopyTo

PARAM_TEST_CASE(GpuMat_CopyTo, cv::musa::DeviceInfo, cv::Size, MatType, UseRoi)
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

MUSA_TEST_P(GpuMat_CopyTo, WithOutMask)
{
    cv::Mat src = randomMatMusa(size, type);

    cv::musa::GpuMat d_src = musaLoadMat(src, useRoi);
    cv::musa::GpuMat dst = createGpuMat(size, type, useRoi);
    d_src.copyTo(dst);

    EXPECT_MAT_NEAR(src, dst, 0.0);
}

MUSA_TEST_P(GpuMat_CopyTo, Masked)
{
    cv::Mat src = randomMatMusa(size, type);
    cv::Mat mask = randomMatMusa(size, CV_8UC1, 0.0, 2.0);

    if (CV_MAT_DEPTH(type) == CV_64F && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat d_src = musaLoadMat(src);
            cv::musa::GpuMat dst;
            d_src.copyTo(dst, musaLoadMat(mask, useRoi));
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat d_src = musaLoadMat(src, useRoi);
        cv::musa::GpuMat dst = musaLoadMat(cv::Mat::zeros(size, type), useRoi);
        d_src.copyTo(dst, musaLoadMat(mask, useRoi));

        cv::Mat dst_gold = cv::Mat::zeros(size, type);
        src.copyTo(dst_gold, mask);

        EXPECT_MAT_NEAR(dst_gold, dst, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA, GpuMat_CopyTo, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_TYPES,
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// ConvertTo

PARAM_TEST_CASE(GpuMat_ConvertTo, cv::musa::DeviceInfo, cv::Size, MatDepth, MatDepth, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int depth1;
    int depth2;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        depth1 = GET_PARAM(2);
        depth2 = GET_PARAM(3);
        useRoi = GET_PARAM(4);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(GpuMat_ConvertTo, WithOutScaling)
{
    cv::Mat src = randomMatMusa(size, depth1);

    if ((depth1 == CV_64F || depth2 == CV_64F) && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat d_src = musaLoadMat(src);
            cv::musa::GpuMat dst;
            d_src.convertTo(dst, depth2);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat d_src = musaLoadMat(src, useRoi);
        cv::musa::GpuMat dst = createGpuMat(size, depth2, useRoi);
        d_src.convertTo(dst, depth2);

        cv::Mat dst_gold;
        src.convertTo(dst_gold, depth2);

        EXPECT_MAT_NEAR(dst_gold, dst, depth2 < CV_32F ? 1.0 : 1e-4);
    }
}

MUSA_TEST_P(GpuMat_ConvertTo, WithScaling)
{
    cv::Mat src = randomMatMusa(size, depth1);
    double a = randomDoubleMusa(0.0, 1.0);
    double b = randomDoubleMusa(-10.0, 10.0);

    if ((depth1 == CV_64F || depth2 == CV_64F) && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat d_src = musaLoadMat(src);
            cv::musa::GpuMat dst;
            d_src.convertTo(dst, depth2, a, b);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat d_src = musaLoadMat(src, useRoi);
        cv::musa::GpuMat dst = createGpuMat(size, depth2, useRoi);
        d_src.convertTo(dst, depth2, a, b);

        cv::Mat dst_gold;
        src.convertTo(dst_gold, depth2, a, b);

        EXPECT_MAT_NEAR(dst_gold, dst, depth2 < CV_32F ? 1.0 : 1e-4);
    }
}

MUSA_TEST_P(GpuMat_ConvertTo, InplaceWithOutScaling)
{
    cv::Mat src = randomMatMusa(size, depth1);

    if ((depth1 == CV_64F || depth2 == CV_64F) && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat d_srcDst = musaLoadMat(src);
            d_srcDst.convertTo(d_srcDst, depth2);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat d_srcDst = musaLoadMat(src, useRoi);
        d_srcDst.convertTo(d_srcDst, depth2);

        cv::Mat dst_gold;
        src.convertTo(dst_gold, depth2);

        EXPECT_MAT_NEAR(dst_gold, d_srcDst, depth2 < CV_32F ? 1.0 : 1e-4);
    }
}


MUSA_TEST_P(GpuMat_ConvertTo, InplaceWithScaling)
{
    cv::Mat src = randomMatMusa(size, depth1);
    double a = randomDoubleMusa(0.0, 1.0);
    double b = randomDoubleMusa(-10.0, 10.0);

    if ((depth1 == CV_64F || depth2 == CV_64F) && !supportMusaFeature(devInfo, cv::musa::NATIVE_DOUBLE))
    {
        try
        {
            cv::musa::GpuMat d_srcDst = musaLoadMat(src);
            d_srcDst.convertTo(d_srcDst, depth2, a, b);
        }
        catch (const cv::Exception& e)
        {
            ASSERT_EQ(cv::Error::StsUnsupportedFormat, e.code);
        }
    }
    else
    {
        cv::musa::GpuMat d_srcDst = musaLoadMat(src, useRoi);
        d_srcDst.convertTo(d_srcDst, depth2, a, b);

        cv::Mat dst_gold;
        src.convertTo(dst_gold, depth2, a, b);

        EXPECT_MAT_NEAR(dst_gold, d_srcDst, depth2 < CV_32F ? 1.0 : 1e-4);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA, GpuMat_ConvertTo, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_DEPTH,
    ALL_DEPTH,
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// locateROI

PARAM_TEST_CASE(GpuMat_LocateROI, cv::musa::DeviceInfo, cv::Size, MatDepth, UseRoi)
{
    cv::musa::DeviceInfo devInfo;
    cv::Size size;
    int depth;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        size = GET_PARAM(1);
        depth = GET_PARAM(2);
        useRoi = GET_PARAM(3);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(GpuMat_LocateROI, locateROI)
{
    Point ofsGold;
    Size wholeSizeGold;
    musa::GpuMat src = createGpuMat(size, depth, wholeSizeGold, ofsGold, useRoi);

    Point ofs;
    Size wholeSize;
    src.locateROI(wholeSize, ofs);
    ASSERT_TRUE(ofs == ofsGold && wholeSize == wholeSizeGold);

    musa::GpuMat srcPtr(src.size(), src.type(), src.data, src.step);
    src.locateROI(wholeSize, ofs);
    ASSERT_TRUE(ofs == ofsGold && wholeSize == wholeSizeGold);
}

INSTANTIATE_TEST_CASE_P(MUSA, GpuMat_LocateROI, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    ALL_DEPTH,
    WHOLE_SUBMAT));

////////////////////////////////////////////////////////////////////////////////
// ensureSizeIsEnough

struct EnsureSizeIsEnough : testing::TestWithParam<cv::musa::DeviceInfo>
{
    virtual void SetUp()
    {
        cv::musa::DeviceInfo devInfo = GetParam();
        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(EnsureSizeIsEnough, BufferReuse)
{
    cv::musa::GpuMat buffer(100, 100, CV_8U);
    cv::musa::GpuMat old = buffer;

    // don't reallocate memory
    cv::musa::ensureSizeIsEnough(10, 20, CV_8U, buffer);
    EXPECT_EQ(10, buffer.rows);
    EXPECT_EQ(20, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_EQ(reinterpret_cast<intptr_t>(old.data), reinterpret_cast<intptr_t>(buffer.data));

    // don't reallocate memory
    cv::musa::ensureSizeIsEnough(20, 30, CV_8U, buffer);
    EXPECT_EQ(20, buffer.rows);
    EXPECT_EQ(30, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_EQ(reinterpret_cast<intptr_t>(old.data), reinterpret_cast<intptr_t>(buffer.data));
}

INSTANTIATE_TEST_CASE_P(MUSA, EnsureSizeIsEnough, ALL_DEVICES);

////////////////////////////////////////////////////////////////////////////////
// createContinuous

struct CreateContinuous : testing::TestWithParam<cv::musa::DeviceInfo>
{
    virtual void SetUp()
    {
        cv::musa::DeviceInfo devInfo = GetParam();
        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(CreateContinuous, BufferReuse)
{
    cv::musa::GpuMat buffer;

    cv::musa::createContinuous(100, 100, CV_8UC1, buffer);
    EXPECT_EQ(100, buffer.rows);
    EXPECT_EQ(100, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_TRUE(buffer.isContinuous());
    EXPECT_EQ(buffer.cols * sizeof(uchar), buffer.step);

    cv::musa::createContinuous(10, 1000, CV_8UC1, buffer);
    EXPECT_EQ(10, buffer.rows);
    EXPECT_EQ(1000, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_TRUE(buffer.isContinuous());
    EXPECT_EQ(buffer.cols * sizeof(uchar), buffer.step);

    cv::musa::createContinuous(10, 10, CV_8UC1, buffer);
    EXPECT_EQ(10, buffer.rows);
    EXPECT_EQ(10, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_TRUE(buffer.isContinuous());
    EXPECT_EQ(buffer.cols * sizeof(uchar), buffer.step);

    cv::musa::createContinuous(100, 100, CV_8UC1, buffer);
    EXPECT_EQ(100, buffer.rows);
    EXPECT_EQ(100, buffer.cols);
    EXPECT_EQ(CV_8UC1, buffer.type());
    EXPECT_TRUE(buffer.isContinuous());
    EXPECT_EQ(buffer.cols * sizeof(uchar), buffer.step);
}

INSTANTIATE_TEST_CASE_P(MUSA, CreateContinuous, ALL_DEVICES);

}} // namespace
#endif // HAVE_MUSA
