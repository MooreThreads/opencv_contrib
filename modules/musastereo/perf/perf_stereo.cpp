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
// StereoBM

typedef tuple<string, string> pair_string;
DEF_PARAM_TEST_1(ImagePair, pair_string);

PERF_TEST_P(ImagePair, StereoBM,
            Values(pair_string("gpu/perf/aloe.png", "gpu/perf/aloeR.png")))
{
    declare.time(300.0);

    const cv::Mat imgLeft = readGpuImage(GET_PARAM(0), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgLeft.empty());

    const cv::Mat imgRight = readGpuImage(GET_PARAM(1), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgRight.empty());

    const int ndisp = 256;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::StereoBM> d_bm = cv::musa::createStereoBM(ndisp);

        const cv::musa::GpuMat d_imgLeft(imgLeft);
        const cv::musa::GpuMat d_imgRight(imgRight);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_bm->compute(d_imgLeft, d_imgRight, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Ptr<cv::StereoBM> bm = cv::StereoBM::create(ndisp);

        cv::Mat dst;

        TEST_CYCLE() bm->compute(imgLeft, imgRight, dst);

        CPU_SANITY_CHECK(dst);
    }
}

PERF_TEST_P(ImagePair, StereoBMwithUniqueness,
            Values(pair_string("gpu/perf/aloe.png", "gpu/perf/aloeR.png")))
{
    declare.time(300.0);

    const cv::Mat imgLeft = readGpuImage(GET_PARAM(0), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgLeft.empty());

    const cv::Mat imgRight = readGpuImage(GET_PARAM(1), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgRight.empty());

    const int ndisp = 256;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::StereoBM> d_bm = cv::musa::createStereoBM(ndisp);
        d_bm->setUniquenessRatio(10);

        const cv::musa::GpuMat d_imgLeft(imgLeft);
        const cv::musa::GpuMat d_imgRight(imgRight);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_bm->compute(d_imgLeft, d_imgRight, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Ptr<cv::StereoBM> bm = cv::StereoBM::create(ndisp);
        bm->setUniquenessRatio(10);

        cv::Mat dst;

        TEST_CYCLE() bm->compute(imgLeft, imgRight, dst);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// StereoBeliefPropagation

PERF_TEST_P(ImagePair, StereoBeliefPropagation,
            Values(pair_string("gpu/stereobp/aloe-L.png", "gpu/stereobp/aloe-R.png")))
{
    declare.time(300.0);

    const cv::Mat imgLeft = readGpuImage(GET_PARAM(0));
    ASSERT_FALSE(imgLeft.empty());

    const cv::Mat imgRight = readGpuImage(GET_PARAM(1));
    ASSERT_FALSE(imgRight.empty());

    const int ndisp = 64;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::StereoBeliefPropagation> d_bp = cv::musa::createStereoBeliefPropagation(ndisp);

        const cv::musa::GpuMat d_imgLeft(imgLeft);
        const cv::musa::GpuMat d_imgRight(imgRight);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_bp->compute(d_imgLeft, d_imgRight, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// StereoConstantSpaceBP

PERF_TEST_P(ImagePair, StereoConstantSpaceBP,
            Values(pair_string("gpu/stereobm/aloe-L.png", "gpu/stereobm/aloe-R.png")))
{
    declare.time(300.0);

    const cv::Mat imgLeft = readGpuImage(GET_PARAM(0), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgLeft.empty());

    const cv::Mat imgRight = readGpuImage(GET_PARAM(1), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgRight.empty());

    const int ndisp = 128;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::StereoConstantSpaceBP> d_csbp = cv::musa::createStereoConstantSpaceBP(ndisp);

        const cv::musa::GpuMat d_imgLeft(imgLeft);
        const cv::musa::GpuMat d_imgRight(imgRight);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_csbp->compute(d_imgLeft, d_imgRight, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// DisparityBilateralFilter

PERF_TEST_P(ImagePair, DisparityBilateralFilter,
            Values(pair_string("gpu/stereobm/aloe-L.png", "gpu/stereobm/aloe-disp.png")))
{
    const cv::Mat img = readGpuImage(GET_PARAM(0), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());

    const cv::Mat disp = readGpuImage(GET_PARAM(1), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(disp.empty());

    const int ndisp = 128;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::DisparityBilateralFilter> d_filter = cv::musa::createDisparityBilateralFilter(ndisp);

        const cv::musa::GpuMat d_img(img);
        const cv::musa::GpuMat d_disp(disp);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_filter->apply(d_disp, d_img, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// ReprojectImageTo3D

DEF_PARAM_TEST(Sz_Depth, cv::Size, MatDepth);

PERF_TEST_P(Sz_Depth, ReprojectImageTo3D,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16S)))
{
    const cv::Size size = GET_PARAM(0);
    const int depth = GET_PARAM(1);

    cv::Mat src(size, depth);
    declare.in(src, WARMUP_RNG);

    cv::Mat Q(4, 4, CV_32FC1);
    cv::randu(Q, 0.1, 1.0);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::reprojectImageTo3D(d_src, dst, Q);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        cv::Mat dst;

        TEST_CYCLE() cv::reprojectImageTo3D(src, dst, Q);

        CPU_SANITY_CHECK(dst);
    }
}

//////////////////////////////////////////////////////////////////////
// DrawColorDisp

PERF_TEST_P(Sz_Depth, DrawColorDisp,
            Combine(MUSA_TYPICAL_MAT_SIZES,
                    Values(CV_8U, CV_16S)))
{
    const cv::Size size = GET_PARAM(0);
    const int type = GET_PARAM(1);

    cv::Mat src(size, type);
    declare.in(src, WARMUP_RNG);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_src(src);
        cv::musa::GpuMat dst;

        TEST_CYCLE() cv::musa::drawColorDisp(d_src, dst, 255);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////////////////////
// StereoSGM

PERF_TEST_P(ImagePair, StereoSGM,
    Values(pair_string("gpu/perf/aloe.png", "gpu/perf/aloeR.png")))
{
    declare.time(300.0);

    const cv::Mat imgLeft = readGpuImage(GET_PARAM(0), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgLeft.empty());

    const cv::Mat imgRight = readGpuImage(GET_PARAM(1), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(imgRight.empty());

    const int ndisp = 128;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::StereoSGM> d_sgm = cv::musa::createStereoSGM(0, ndisp);

        const cv::musa::GpuMat d_imgLeft(imgLeft);
        const cv::musa::GpuMat d_imgRight(imgRight);
        cv::musa::GpuMat dst;

        TEST_CYCLE() d_sgm->compute(d_imgLeft, d_imgRight, dst);

        MUSA_SANITY_CHECK(dst);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

}} // namespace
