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
// GoodFeaturesToTrack

DEF_PARAM_TEST(Image_MinDistance, string, double);

PERF_TEST_P(Image_MinDistance, GoodFeaturesToTrack,
            Combine(Values<string>("gpu/perf/aloe.png"),
                    Values(0.0, 3.0)))
{
    const string fileName = GET_PARAM(0);
    const double minDistance = GET_PARAM(1);

    const cv::Mat image = readGpuImage(fileName, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(image.empty());

    const int maxCorners = 8000;
    const double qualityLevel = 0.01;

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::CornersDetector> d_detector = cv::musa::createGoodFeaturesToTrackDetector(image.type(), maxCorners, qualityLevel, minDistance);

        const cv::musa::GpuMat d_image(image);
        cv::musa::GpuMat pts;

        TEST_CYCLE() d_detector->detect(d_image, pts);

        MUSA_SANITY_CHECK(pts);
    }
    else
    {
        cv::Mat pts;

        TEST_CYCLE() cv::goodFeaturesToTrack(image, pts, maxCorners, qualityLevel, minDistance);

        CPU_SANITY_CHECK(pts);
    }
}

}} // namespace
