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

typedef pair<string, string> pair_string;

DEF_PARAM_TEST_1(ImagePair, pair_string);

//////////////////////////////////////////////////////
// BroxOpticalFlow

PERF_TEST_P(ImagePair, BroxOpticalFlow,
            Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")))
{
    declare.time(300);

    cv::Mat frame0 = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage(GetParam().second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    frame0.convertTo(frame0, CV_32FC1, 1.0 / 255.0);
    frame1.convertTo(frame1, CV_32FC1, 1.0 / 255.0);

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat flow;

        cv::Ptr<cv::musa::BroxOpticalFlow> d_alg =
                cv::musa::BroxOpticalFlow::create(0.197 /*alpha*/, 50.0 /*gamma*/, 0.8 /*scale_factor*/,
                                                  10 /*inner_iterations*/, 77 /*outer_iterations*/, 10 /*solver_iterations*/);

        TEST_CYCLE() d_alg->calc(d_frame0, d_frame1, flow);

        cv::musa::GpuMat flows[2];
        cv::musa::split(flow, flows);

        cv::musa::GpuMat u = flows[0];
        cv::musa::GpuMat v = flows[1];

        MUSA_SANITY_CHECK(u, 1e-1);
        MUSA_SANITY_CHECK(v, 1e-1);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////
// PyrLKOpticalFlowSparse

DEF_PARAM_TEST(ImagePair_Gray_NPts_WinSz_Levels_Iters, pair_string, bool, int, int, int, int);

PERF_TEST_P(ImagePair_Gray_NPts_WinSz_Levels_Iters, PyrLKOpticalFlowSparse,
            Combine(Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")),
                    Bool(),
                    Values(8000),
                    Values(21),
                    Values(1, 3),
                    Values(1, 30)))
{
    declare.time(20.0);

    const pair_string imagePair = GET_PARAM(0);
    const bool useGray = GET_PARAM(1);
    const int points = GET_PARAM(2);
    const int winSize = GET_PARAM(3);
    const int levels = GET_PARAM(4);
    const int iters = GET_PARAM(5);

    cv::Mat frame0 = readGpuImage(imagePair.first, useGray ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage(imagePair.second, useGray ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
    ASSERT_FALSE(frame1.empty());

    cv::Mat gray_frame;
    if (useGray)
        gray_frame = frame0;
    else
        cv::cvtColor(frame0, gray_frame, cv::COLOR_BGR2GRAY);

    cv::Mat pts;
    cv::goodFeaturesToTrack(gray_frame, pts, points, 0.01, 0.0);

    frame0.convertTo(frame0, CV_32F);
    frame1.convertTo(frame1, CV_32F);
    if(!useGray)
    {
        cv::cvtColor(frame0, frame0, cv::COLOR_BGR2BGRA);
        cv::cvtColor(frame1, frame1, cv::COLOR_BGR2BGRA);
    }

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_pts(pts.reshape(2, 1));

        cv::Ptr<cv::musa::SparsePyrLKOpticalFlow> d_pyrLK =
                cv::musa::SparsePyrLKOpticalFlow::create(cv::Size(winSize, winSize),
                                                         levels - 1,
                                                         iters);

        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat nextPts;
        cv::musa::GpuMat status;

        TEST_CYCLE() d_pyrLK->calc(d_frame0, d_frame1, d_pts, nextPts, status);

        MUSA_SANITY_CHECK(nextPts);
        MUSA_SANITY_CHECK(status);
    }
    else
    {
        cv::Mat nextPts;
        cv::Mat status;

        TEST_CYCLE()
        {
            cv::calcOpticalFlowPyrLK(frame0, frame1, pts, nextPts, status, cv::noArray(),
                                     cv::Size(winSize, winSize), levels - 1,
                                     cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, iters, 0.01));
        }

        CPU_SANITY_CHECK(nextPts);
        CPU_SANITY_CHECK(status);
    }
}

//////////////////////////////////////////////////////
// PyrLKOpticalFlowDense

DEF_PARAM_TEST(ImagePair_WinSz_Levels_Iters, pair_string, int, int, int);

PERF_TEST_P(ImagePair_WinSz_Levels_Iters, PyrLKOpticalFlowDense,
            Combine(Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")),
                    Values(3, 5, 7, 9, 13, 17, 21),
                    Values(1, 3),
                    Values(1, 10)))
{
    declare.time(30);

    const pair_string imagePair = GET_PARAM(0);
    const int winSize = GET_PARAM(1);
    const int levels = GET_PARAM(2);
    const int iters = GET_PARAM(3);

    const cv::Mat frame0 = readGpuImage(imagePair.first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    const cv::Mat frame1 = readGpuImage(imagePair.second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat flow;

        cv::Ptr<cv::musa::DensePyrLKOpticalFlow> d_pyrLK =
                cv::musa::DensePyrLKOpticalFlow::create(cv::Size(winSize, winSize),
                                                        levels - 1,
                                                        iters);

        TEST_CYCLE() d_pyrLK->calc(d_frame0, d_frame1, flow);

        cv::musa::GpuMat flows[2];
        cv::musa::split(flow, flows);

        cv::musa::GpuMat u = flows[0];
        cv::musa::GpuMat v = flows[1];

        SANITY_CHECK_NOTHING();
    }
    else
    {
        FAIL_NO_CPU();
    }
}

//////////////////////////////////////////////////////
// FarnebackOpticalFlow

PERF_TEST_P(ImagePair, FarnebackOpticalFlow,
            Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")))
{
    declare.time(10);

    const cv::Mat frame0 = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    const cv::Mat frame1 = readGpuImage(GetParam().second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    const int numLevels = 5;
    const double pyrScale = 0.5;
    const int winSize = 13;
    const int numIters = 10;
    const int polyN = 5;
    const double polySigma = 1.1;
    const int flags = 0;

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat flow;

        cv::Ptr<cv::musa::FarnebackOpticalFlow> d_farneback =
                cv::musa::FarnebackOpticalFlow::create(numLevels, pyrScale, false, winSize,
                                                       numIters, polyN, polySigma, flags);

        TEST_CYCLE() d_farneback->calc(d_frame0, d_frame1, flow);

        cv::musa::GpuMat flows[2];
        cv::musa::split(flow, flows);

        cv::musa::GpuMat u = flows[0];
        cv::musa::GpuMat v = flows[1];

        MUSA_SANITY_CHECK(u, 1e-4);
        MUSA_SANITY_CHECK(v, 1e-4);
    }
    else
    {
        cv::Mat flow;

        TEST_CYCLE() cv::calcOpticalFlowFarneback(frame0, frame1, flow, pyrScale, numLevels, winSize, numIters, polyN, polySigma, flags);

        CPU_SANITY_CHECK(flow);
    }
}

//////////////////////////////////////////////////////
// OpticalFlowDual_TVL1

PERF_TEST_P(ImagePair, OpticalFlowDual_TVL1,
            Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")))
{
    declare.time(20);

    const cv::Mat frame0 = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    const cv::Mat frame1 = readGpuImage(GetParam().second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat flow;

        cv::Ptr<cv::musa::OpticalFlowDual_TVL1> d_alg =
                cv::musa::OpticalFlowDual_TVL1::create();

        TEST_CYCLE() d_alg->calc(d_frame0, d_frame1, flow);

        cv::musa::GpuMat flows[2];
        cv::musa::split(flow, flows);

        cv::musa::GpuMat u = flows[0];
        cv::musa::GpuMat v = flows[1];

        MUSA_SANITY_CHECK(u, 1e-1);
        MUSA_SANITY_CHECK(v, 1e-1);
    }
    else
    {
        cv::Mat flow;

        cv::Ptr<cv::optflow::DualTVL1OpticalFlow> alg = cv::optflow::createOptFlow_DualTVL1();
        alg->setMedianFiltering(1);
        alg->setInnerIterations(1);
        alg->setOuterIterations(300);
        TEST_CYCLE() alg->calc(frame0, frame1, flow);

        CPU_SANITY_CHECK(flow);
    }
}

//////////////////////////////////////////////////////
// NvidiaOpticalFlow_1_0

PERF_TEST_P(ImagePair, NvidiaOpticalFlow_1_0,
    Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")))
{
    declare.time(10);

    const cv::Mat frame0 = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    const cv::Mat frame1 = readGpuImage(GetParam().second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());
    cv::musa::Stream inputStream;
    cv::musa::Stream outputStream;

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat d_flow;
        cv::Ptr<cv::musa::NvidiaOpticalFlow_1_0> d_nvof;
        try
        {
            d_nvof = cv::musa::NvidiaOpticalFlow_1_0::create(frame0.size(),
                cv::musa::NvidiaOpticalFlow_1_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_FAST,
                false, false, false, 0, inputStream, outputStream);
        }
        catch (const cv::Exception& e)
        {
            if(e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
                throw SkipTestException("Current configuration is not supported");
            throw;
        }

        TEST_CYCLE() d_nvof->calc(d_frame0, d_frame1, d_flow);

        cv::musa::GpuMat flow[2];
        cv::musa::split(d_flow, flow);

        cv::musa::GpuMat u = flow[0];
        cv::musa::GpuMat v = flow[1];

        MUSA_SANITY_CHECK(u, 1e-10);
        MUSA_SANITY_CHECK(v, 1e-10);

        d_nvof->collectGarbage();
    }
}

//////////////////////////////////////////////////////
// NvidiaOpticalFlow_2_0

PERF_TEST_P(ImagePair, NvidiaOpticalFlow_2_0,
    Values<pair_string>(make_pair("gpu/opticalflow/frame0.png", "gpu/opticalflow/frame1.png")))
{
    declare.time(10);

    const cv::Mat frame0 = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    const cv::Mat frame1 = readGpuImage(GetParam().second, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    const cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_OUTPUT_VECTOR_GRID_SIZE outGridSize
        = cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_OUTPUT_VECTOR_GRID_SIZE::NV_OF_OUTPUT_VECTOR_GRID_SIZE_1;
    const cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_HINT_VECTOR_GRID_SIZE hintGridSize
        = cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_HINT_VECTOR_GRID_SIZE::NV_OF_HINT_VECTOR_GRID_SIZE_1;
    cv::musa::Stream inputStream;
    cv::musa::Stream outputStream;

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_frame0(frame0);
        const cv::musa::GpuMat d_frame1(frame1);
        cv::musa::GpuMat d_flow;
        cv::Ptr<cv::musa::NvidiaOpticalFlow_2_0> d_nvof;
        try
        {
            d_nvof = cv::musa::NvidiaOpticalFlow_2_0::create(frame0.size(),
                cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_FAST, outGridSize, hintGridSize,
              false, false, false, 0, inputStream, outputStream);
        }
        catch (const cv::Exception& e)
        {
            if (e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
                throw SkipTestException("Current configuration is not supported");
            throw;
        }

        TEST_CYCLE() d_nvof->calc(d_frame0, d_frame1, d_flow);

        cv::musa::GpuMat flow[2];
        cv::musa::split(d_flow, flow);

        cv::musa::GpuMat u = flow[0];
        cv::musa::GpuMat v = flow[1];

        MUSA_SANITY_CHECK(u, 1e-10);
        MUSA_SANITY_CHECK(v, 1e-10);

        d_nvof->collectGarbage();
    }
}

}} // namespace
