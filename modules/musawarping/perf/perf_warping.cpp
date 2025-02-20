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
// Remap

enum { HALF_SIZE=0, UPSIDE_DOWN, REFLECTION_X, REFLECTION_BOTH };
CV_ENUM(RemapMode, HALF_SIZE, UPSIDE_DOWN, REFLECTION_X, REFLECTION_BOTH)

void generateMap(cv::Mat& map_x, cv::Mat& map_y, int remapMode)
{
    for (int j = 0; j < map_x.rows; ++j)
    {
        for (int i = 0; i < map_x.cols; ++i)
        {
            switch (remapMode)
            {
            case HALF_SIZE:
                if (i > map_x.cols*0.25 && i < map_x.cols*0.75 && j > map_x.rows*0.25 && j < map_x.rows*0.75)
                {
                    map_x.at<float>(j,i) = 2.f * (i - map_x.cols * 0.25f) + 0.5f;
                    map_y.at<float>(j,i) = 2.f * (j - map_x.rows * 0.25f) + 0.5f;
                }
                else
                {
                    map_x.at<float>(j,i) = 0.f;
                    map_y.at<float>(j,i) = 0.f;
                }
                break;
            case UPSIDE_DOWN:
                map_x.at<float>(j,i) = static_cast<float>(i);
                map_y.at<float>(j,i) = static_cast<float>(map_x.rows - j);
                break;
            case REFLECTION_X:
                map_x.at<float>(j,i) = static_cast<float>(map_x.cols - i);
                map_y.at<float>(j,i) = static_cast<float>(j);
                break;
            case REFLECTION_BOTH:
                map_x.at<float>(j,i) = static_cast<float>(map_x.cols - i);
                map_y.at<float>(j,i) = static_cast<float>(map_x.rows - j);
                break;
            } // end of switch
        }
    }
}

DEF_PARAM_TEST(Sz_Depth_Cn_Inter_Border_Mode, cv::Size, MatDepth, MatCn, Interpolation, BorderMode, RemapMode);

PERF_TEST_P(Sz_Depth_Cn_Inter_Border_Mode, Remap,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(Interpolation(cv::INTER_NEAREST), Interpolation(cv::INTER_LINEAR), Interpolation(cv::INTER_CUBIC)),
                    ALL_BORDER_MODES,
                    RemapMode::all()))
{
    declare.time(20.0);

    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = GET_PARAM(3);
    const int borderMode = GET_PARAM(4);
    const int remapMode = GET_PARAM(5);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    cv::Mat xmap(size, CV_32FC1);
    cv::Mat ymap(size, CV_32FC1);
    generateMap(xmap, ymap, remapMode);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        const cv::musa::GpuMat d_xmap(xmap);
        const cv::musa::GpuMat d_ymap(ymap);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::remap(d_src, dst, d_xmap, d_ymap, interpolation, borderMode);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::remap(src, dst, xmap, ymap, interpolation, borderMode);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// Resize

DEF_PARAM_TEST(Sz_Depth_Cn_Inter_Scale, cv::Size, MatDepth, MatCn, Interpolation, double);

PERF_TEST_P(Sz_Depth_Cn_Inter_Scale, Resize,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(Interpolation(cv::INTER_NEAREST), Interpolation(cv::INTER_LINEAR), Interpolation(cv::INTER_CUBIC)),
                    Values(0.5, 0.3, 2.0)))
{
    declare.time(20.0);

    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = GET_PARAM(3);
    const double f = GET_PARAM(4);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::resize(d_src, dst, cv::Size(), f, f, interpolation);

        MUSA_SANITY_CHECK(dst, 1e-3, ERROR_RELATIVE);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::resize(src, dst, cv::Size(), f, f, interpolation);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// ResizeArea

DEF_PARAM_TEST(Sz_Depth_Cn_Scale, cv::Size, MatDepth, MatCn, double);

PERF_TEST_P(Sz_Depth_Cn_Scale, ResizeArea,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(0.2, 0.1, 0.05)))
{
    declare.time(1.0);

    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = cv::INTER_AREA;
    const double f = GET_PARAM(3);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::resize(d_src, dst, cv::Size(), f, f, interpolation);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::resize(src, dst, cv::Size(), f, f, interpolation);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// WarpAffine

DEF_PARAM_TEST(Sz_Depth_Cn_Inter_Border, cv::Size, MatDepth, MatCn, Interpolation, BorderMode);

PERF_TEST_P(Sz_Depth_Cn_Inter_Border, WarpAffine,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(Interpolation(cv::INTER_NEAREST), Interpolation(cv::INTER_LINEAR), Interpolation(cv::INTER_CUBIC)),
                    ALL_BORDER_MODES))
{
    declare.time(20.0);

    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = GET_PARAM(3);
    const int borderMode = GET_PARAM(4);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    const double aplha = CV_PI / 4;
    const double mat[2 * 3] =
    {
        std::cos(aplha), -std::sin(aplha), static_cast<double>(src.cols) / 2.0,
        std::sin(aplha),  std::cos(aplha), 0
    };
    const cv::Mat M(2, 3, CV_64F, (void*) mat);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::warpAffine(d_src, dst, M, size, interpolation, borderMode);

        MUSA_SANITY_CHECK(dst, 1);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::warpAffine(src, dst, M, size, interpolation, borderMode);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// WarpPerspective

PERF_TEST_P(Sz_Depth_Cn_Inter_Border, WarpPerspective,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(Interpolation(cv::INTER_NEAREST), Interpolation(cv::INTER_LINEAR), Interpolation(cv::INTER_CUBIC)),
                    ALL_BORDER_MODES))
{
    declare.time(20.0);

    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = GET_PARAM(3);
    const int borderMode = GET_PARAM(4);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    const double aplha = CV_PI / 4;
    double mat[3][3] = { {std::cos(aplha), -std::sin(aplha), static_cast<double>(src.cols) / 2.0},
                         {std::sin(aplha),  std::cos(aplha), 0},
                         {0.0,              0.0,             1.0}};
    const cv::Mat M(3, 3, CV_64F, (void*) mat);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::warpPerspective(d_src, dst, M, size, interpolation, borderMode);

        MUSA_SANITY_CHECK(dst, 1);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::warpPerspective(src, dst, M, size, interpolation, borderMode);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// Rotate

DEF_PARAM_TEST(Sz_Depth_Cn_Inter, cv::Size, MatDepth, MatCn, Interpolation);

PERF_TEST_P(Sz_Depth_Cn_Inter, Rotate,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16U, CV_32F),
                    MUSA_CHANNELS_1_3_4,
                    Values(Interpolation(cv::INTER_NEAREST), Interpolation(cv::INTER_LINEAR), Interpolation(cv::INTER_CUBIC))))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);
    const int channels = GET_PARAM(2);
    const int interpolation = GET_PARAM(3);

    const int type = CV_MAKE_TYPE(depth, channels);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::rotate(d_src, dst, size, 30.0, 0, 0, interpolation);

        MUSA_SANITY_CHECK(dst, 1e-3, ERROR_RELATIVE);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// PyrDown

DEF_PARAM_TEST(Sz_Depth_Cn, cv::Size, MatDepth, MatCn);

PERF_TEST_P(Sz_Depth_Cn, PyrDown,
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
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::pyrDown(d_src, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::pyrDown(src, dst);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// PyrUp

PERF_TEST_P(Sz_Depth_Cn, PyrUp,
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
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::pyrUp(d_src, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::pyrUp(src, dst);

        CPU_SANITY_CHECK(dst);
    }
}

}} // namespace
