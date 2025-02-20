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

#include "perf_precomp.hpp"

namespace opencv_test { namespace {

//////////////////////////////////////////////////////////////////////
// Norm

DEF_PARAM_TEST(Sz_Depth_Norm, cv::Size, MatDepth, NormType);

PERF_TEST_P(Sz_Depth_Norm, Norm,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32S, CV_32F),
                    Values(NormType(cv::NORM_INF), NormType(cv::NORM_L1), NormType(cv::NORM_L2))))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int normType = GET_PARAM(2);

    cv::Mat src(size, depth);
    if (depth == CV_8U)
        cv::randu(src, 0, 254);
    else
        declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat d_buf;
        double gpu_dst;

        TEST_CYCLE() gpu_dst = cv::musa::norm(d_src, normType, d_buf);

        SANITY_CHECK(gpu_dst, 1e-6, ERROR_RELATIVE);
    }
    else
    {
        double cpu_dst;

        TEST_CYCLE() cpu_dst = cv::norm(src, normType);

        SANITY_CHECK(cpu_dst, 1e-6, ERROR_RELATIVE);
    }
}

//////////////////////////////////////////////////////////////////////
// NormDiff

DEF_PARAM_TEST(Sz_Norm, cv::Size, NormType);

PERF_TEST_P(Sz_Norm, NormDiff,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(NormType(cv::NORM_INF), NormType(cv::NORM_L1), NormType(cv::NORM_L2))))
{
    const cv::Size size = GET_PARAM(0);
    const int normType = GET_PARAM(1);

    cv::Mat src1(size, CV_8UC1);
    declare.in(src1, WARMUP_RNG);

    cv::Mat src2(size, CV_8UC1);
    declare.in(src2, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src1(src1);
        const cv::musa::GpuMat d_src2(src2);
        double gpu_dst;

        TEST_CYCLE() gpu_dst = cv::musa::norm(d_src1, d_src2, normType);

        SANITY_CHECK(gpu_dst);

    }
    else
    {
        double cpu_dst;

        TEST_CYCLE() cpu_dst = cv::norm(src1, src2, normType);

        SANITY_CHECK(cpu_dst);
    }
}

//////////////////////////////////////////////////////////////////////
// Sum

DEF_PARAM_TEST(Sz_Depth_Cn, cv::Size, MatDepth, MatCn);

PERF_TEST_P(Sz_Depth_Cn, Sum,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::Scalar gpu_dst;

        TEST_CYCLE() gpu_dst = cv::musa::sum(d_src);

        SANITY_CHECK(gpu_dst, 1e-5, ERROR_RELATIVE);
    }
    else
    {
        cv::Scalar cpu_dst;

        TEST_CYCLE() cpu_dst = cv::sum(src);

        SANITY_CHECK(cpu_dst, 1e-6, ERROR_RELATIVE);
    }
}

//////////////////////////////////////////////////////////////////////
// SumAbs

PERF_TEST_P(Sz_Depth_Cn, SumAbs,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::Scalar gpu_dst;

        TEST_CYCLE() gpu_dst = cv::musa::absSum(d_src);

        SANITY_CHECK(gpu_dst, 1e-6, ERROR_RELATIVE);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// SumSqr

PERF_TEST_P(Sz_Depth_Cn, SumSqr,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values<MatDepth>(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::Scalar gpu_dst;

        TEST_CYCLE() gpu_dst = cv::musa::sqrSum(d_src);

        SANITY_CHECK(gpu_dst, 1e-6, ERROR_RELATIVE);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// MinMax

DEF_PARAM_TEST(Sz_Depth, cv::Size, MatDepth);

PERF_TEST_P(Sz_Depth, MinMax,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F, CV_64F)))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);

    cv::Mat src(size, depth);
    if (depth == CV_8U)
        cv::randu(src, 0, 254);
    else
        declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        double gpu_minVal, gpu_maxVal;

        TEST_CYCLE() cv::musa::minMax(d_src, &gpu_minVal, &gpu_maxVal, cv::musa::GpuMat());

        SANITY_CHECK(gpu_minVal, 1e-10);
        SANITY_CHECK(gpu_maxVal, 1e-10);
    }
    else
    {
        double cpu_minVal, cpu_maxVal;

        TEST_CYCLE() cv::minMaxLoc(src, &cpu_minVal, &cpu_maxVal);

        SANITY_CHECK(cpu_minVal);
        SANITY_CHECK(cpu_maxVal);
    }
}

//////////////////////////////////////////////////////////////////////
// MinMaxLoc

PERF_TEST_P(Sz_Depth, MinMaxLoc,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F, CV_64F)))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);

    cv::Mat src(size, depth);
    if (depth == CV_8U)
        cv::randu(src, 0, 254);
    else
        declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        double gpu_minVal, gpu_maxVal;
        cv::Point gpu_minLoc, gpu_maxLoc;

        TEST_CYCLE() cv::musa::minMaxLoc(d_src, &gpu_minVal, &gpu_maxVal, &gpu_minLoc, &gpu_maxLoc);

        SANITY_CHECK(gpu_minVal, 1e-10);
        SANITY_CHECK(gpu_maxVal, 1e-10);
    }
    else
    {
        double cpu_minVal, cpu_maxVal;
        cv::Point cpu_minLoc, cpu_maxLoc;

        TEST_CYCLE() cv::minMaxLoc(src, &cpu_minVal, &cpu_maxVal, &cpu_minLoc, &cpu_maxLoc);

        SANITY_CHECK(cpu_minVal);
        SANITY_CHECK(cpu_maxVal);
    }
}

//////////////////////////////////////////////////////////////////////
// CountNonZero

PERF_TEST_P(Sz_Depth, CountNonZero,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F, CV_64F)))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);

    cv::Mat src(size, depth);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        int gpu_dst = 0;

        TEST_CYCLE() gpu_dst = cv::musa::countNonZero(d_src);

        SANITY_CHECK(gpu_dst);
    }
    else
    {
        int cpu_dst = 0;

        TEST_CYCLE() cpu_dst = cv::countNonZero(src);

        SANITY_CHECK(cpu_dst);
    }
}

//////////////////////////////////////////////////////////////////////
// Reduce

CV_ENUM(ReduceCode, REDUCE_SUM, REDUCE_AVG, REDUCE_MAX, REDUCE_MIN)

enum {Rows = 0, Cols = 1};
CV_ENUM(ReduceDim, Rows, Cols)

DEF_PARAM_TEST(Sz_Depth_Cn_Code_Dim, cv::Size, MatDepth, MatCn, ReduceCode, ReduceDim);

PERF_TEST_P(Sz_Depth_Cn_Code_Dim, Reduce,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_16S, CV_32F),
                    Values(1, 2, 3, 4),
                    ReduceCode::all(),
                    ReduceDim::all()))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int reduceOp = GET_PARAM(3);
    const int dim = GET_PARAM(4);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::reduce(d_src, dst, dim, reduceOp, CV_32F);

        dst = dst.reshape(dst.channels(), 1);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::reduce(src, dst, dim, reduceOp, CV_32F);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// Normalize

DEF_PARAM_TEST(Sz_Depth_NormType, cv::Size, MatDepth, NormType);

PERF_TEST_P(Sz_Depth_NormType, Normalize,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F, CV_64F),
                    Values(NormType(cv::NORM_INF),
                           NormType(cv::NORM_L1),
                           NormType(cv::NORM_L2),
                           NormType(cv::NORM_MINMAX))))
{
    const cv::Size size = GET_PARAM(0);
    const int type = GET_PARAM(1);
    const int norm_type = GET_PARAM(2);

    const double alpha = 1;
    const double beta = 0;

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::normalize(d_src, dst, alpha, beta, norm_type, type, cv::musa::GpuMat());

        MUSA_SANITY_CHECK(dst, 1e-6);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::normalize(src, dst, alpha, beta, norm_type, type);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// MeanStdDev

PERF_TEST_P(Sz, MeanStdDev,
            MUSA_TYPICAL_MAT_SIZES)
{
    const cv::Size size = GetParam();

    cv::Mat src(size, CV_8UC1);
    declare.in(src, WARMUP_RNG);


    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::Scalar gpu_mean;
        cv::Scalar gpu_stddev;

        TEST_CYCLE() cv::musa::meanStdDev(d_src, gpu_mean, gpu_stddev);

        SANITY_CHECK(gpu_mean);
        SANITY_CHECK(gpu_stddev);
    }
    else
    {
        cv::Scalar cpu_mean;
        cv::Scalar cpu_stddev;

        TEST_CYCLE() cv::meanStdDev(src, cpu_mean, cpu_stddev);

        SANITY_CHECK(cpu_mean);
        SANITY_CHECK(cpu_stddev);
    }
}

//////////////////////////////////////////////////////////////////////
// Integral

PERF_TEST_P(Sz, Integral,
            MUSA_TYPICAL_MAT_SIZES)
{
    const cv::Size size = GetParam();

    cv::Mat src(size, CV_8UC1);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::integral(d_src, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::integral(src, dst);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// IntegralSqr

PERF_TEST_P(Sz, IntegralSqr,
            MUSA_TYPICAL_MAT_SIZES)
{
    const cv::Size size = GetParam();

    cv::Mat src(size, CV_8UC1);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::sqrIntegral(d_src, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

}} // namespace
