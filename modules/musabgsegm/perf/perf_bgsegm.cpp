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

//////////////////////////////////////////////////////
// MOG

#ifdef HAVE_VIDEO_INPUT

DEF_PARAM_TEST(Video_Cn_LearningRate, string, MatCn, double);

PERF_TEST_P(Video_Cn_LearningRate, MOG,
            Combine(Values("cv/video/768x576.avi", "cv/video/1920x1080.avi"),
                    MUSA_CHANNELS_1_3_4,
                    Values(0.0, 0.01)))
{
    const int numIters = 10;

    const string inputFile = perf::TestBase::getDataPath(GET_PARAM(0));
    const int cn = GET_PARAM(1);
    const float learningRate = static_cast<float>(GET_PARAM(2));

    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;

    cap >> frame;
    ASSERT_FALSE(frame.empty());

    if (cn != 3)
    {
        cv::Mat temp;
        if (cn == 1)
            cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
        else
            cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
        cv::swap(temp, frame);
    }

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::BackgroundSubtractor> d_mog = cv::musa::createBackgroundSubtractorMOG();

        cv::musa::GpuMat d_frame(frame);
        cv::musa::GpuMat foreground;

        d_mog->apply(d_frame, foreground, learningRate);

        int i = 0;

        // collect performance data
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            startTimer();
            if(!next())
                break;

            d_mog->apply(d_frame, foreground, learningRate);

            stopTimer();
        }

        // process last frame in sequence to get data for sanity test
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            d_mog->apply(d_frame, foreground, learningRate);
        }

        MUSA_SANITY_CHECK(foreground);
    }
    else
    {
        FAIL_NO_CPU();
    }
}

#endif

//////////////////////////////////////////////////////
// MOG2

#ifdef HAVE_VIDEO_INPUT

DEF_PARAM_TEST(Video_Cn, string, int);

PERF_TEST_P(Video_Cn, DISABLED_MOG2,
            Combine(Values("cv/video/768x576.avi", "cv/video/1920x1080.avi"),
                    MUSA_CHANNELS_1_3_4))
{
    const int numIters = 10;

    const string inputFile = perf::TestBase::getDataPath(GET_PARAM(0));
    const int cn = GET_PARAM(1);

    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;

    cap >> frame;
    ASSERT_FALSE(frame.empty());

    if (cn != 3)
    {
        cv::Mat temp;
        if (cn == 1)
            cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
        else
            cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
        cv::swap(temp, frame);
    }

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::BackgroundSubtractorMOG2> d_mog2 = cv::musa::createBackgroundSubtractorMOG2();
        d_mog2->setDetectShadows(false);

        cv::musa::GpuMat d_frame(frame);
        cv::musa::GpuMat foreground;

        d_mog2->apply(d_frame, foreground);

        int i = 0;

        // collect performance data
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            startTimer();
            if(!next())
                break;

            d_mog2->apply(d_frame, foreground);

            stopTimer();
        }

        // process last frame in sequence to get data for sanity test
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            d_mog2->apply(d_frame, foreground);
        }

        MUSA_SANITY_CHECK(foreground);
    }
    else
    {
        cv::Ptr<cv::BackgroundSubtractorMOG2> mog2 = cv::createBackgroundSubtractorMOG2();
        mog2->setDetectShadows(false);

        cv::Mat foreground;

        mog2->apply(frame, foreground);

        int i = 0;

        // collect performance data
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            startTimer();
            if(!next())
                break;

            mog2->apply(frame, foreground);

            stopTimer();
        }

        // process last frame in sequence to get data for sanity test
        for (; i < numIters; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            mog2->apply(frame, foreground);
        }

        CPU_SANITY_CHECK(foreground);
    }
}

#endif

//////////////////////////////////////////////////////
// MOG2GetBackgroundImage

#ifdef HAVE_VIDEO_INPUT

PERF_TEST_P(Video_Cn, MOG2GetBackgroundImage,
            Combine(Values("cv/video/768x576.avi", "cv/video/1920x1080.avi"),
                    MUSA_CHANNELS_1_3_4))
{
    const string inputFile = perf::TestBase::getDataPath(GET_PARAM(0));
    const int cn = GET_PARAM(1);

    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::BackgroundSubtractor> d_mog2 = cv::musa::createBackgroundSubtractorMOG2();

        cv::musa::GpuMat d_frame;
        cv::musa::GpuMat d_foreground;

        for (int i = 0; i < 10; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            d_mog2->apply(d_frame, d_foreground);
        }

        cv::musa::GpuMat background;

        TEST_CYCLE() d_mog2->getBackgroundImage(background);

        MUSA_SANITY_CHECK(background, 1);
    }
    else
    {
        cv::Ptr<cv::BackgroundSubtractor> mog2 = cv::createBackgroundSubtractorMOG2();
        cv::Mat foreground;

        for (int i = 0; i < 10; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            mog2->apply(frame, foreground);
        }

        cv::Mat background;

        TEST_CYCLE() mog2->getBackgroundImage(background);

        CPU_SANITY_CHECK(background);
    }
}

#endif

}} // namespace
