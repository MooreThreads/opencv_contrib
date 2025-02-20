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

///////////////////////////////////////////////////////////////
// HOG

DEF_PARAM_TEST_1(Image, string);

PERF_TEST_P(Image, ObjDetect_HOG,
            Values<string>("gpu/hog/road.png",
                           "gpu/caltech/image_00000009_0.png",
                           "gpu/caltech/image_00000032_0.png",
                           "gpu/caltech/image_00000165_0.png",
                           "gpu/caltech/image_00000261_0.png",
                           "gpu/caltech/image_00000469_0.png",
                           "gpu/caltech/image_00000527_0.png",
                           "gpu/caltech/image_00000574_0.png"))
{
    declare.time(300.0);

    const cv::Mat img = readGpuImage(GetParam(), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());

    if (PERF_RUN_MUSA())
    {
        const cv::musa::GpuMat d_img(img);
        std::vector<cv::Rect> gpu_found_locations;

        cv::Ptr<cv::musa::HOG> d_hog = cv::musa::HOG::create();
        d_hog->setSVMDetector(d_hog->getDefaultPeopleDetector());

        TEST_CYCLE() d_hog->detectMultiScale(d_img, gpu_found_locations);

        SANITY_CHECK(gpu_found_locations);
    }
    else
    {
        std::vector<cv::Rect> cpu_found_locations;

        cv::Ptr<cv::musa::HOG> d_hog = cv::musa::HOG::create();

        cv::HOGDescriptor hog;
        hog.setSVMDetector(d_hog->getDefaultPeopleDetector());

        TEST_CYCLE() hog.detectMultiScale(img, cpu_found_locations);

        SANITY_CHECK(cpu_found_locations);
    }
}

///////////////////////////////////////////////////////////////
// HaarClassifier

typedef pair<string, string> pair_string;
DEF_PARAM_TEST_1(ImageAndCascade, pair_string);

PERF_TEST_P(ImageAndCascade, ObjDetect_HaarClassifier,
            Values<pair_string>(make_pair("gpu/haarcascade/group_1_640x480_VGA.pgm", "gpu/perf/haarcascade_frontalface_alt.xml")))
{
    const cv::Mat img = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::CascadeClassifier> d_cascade =
                cv::musa::CascadeClassifier::create(perf::TestBase::getDataPath(GetParam().second));

        const cv::musa::GpuMat d_img(img);
        cv::musa::GpuMat objects_buffer;

        TEST_CYCLE() d_cascade->detectMultiScale(d_img, objects_buffer);

        std::vector<cv::Rect> gpu_rects;
        d_cascade->convert(objects_buffer, gpu_rects);

        cv::groupRectangles(gpu_rects, 3, 0.2);
        SANITY_CHECK(gpu_rects);
    }
    else
    {
        cv::CascadeClassifier cascade;
        ASSERT_TRUE(cascade.load(perf::TestBase::getDataPath("gpu/perf/haarcascade_frontalface_alt.xml")));

        std::vector<cv::Rect> cpu_rects;

        TEST_CYCLE() cascade.detectMultiScale(img, cpu_rects);

        SANITY_CHECK(cpu_rects);
    }
}

///////////////////////////////////////////////////////////////
// LBP cascade

PERF_TEST_P(ImageAndCascade, ObjDetect_LBPClassifier,
            Values<pair_string>(make_pair("gpu/haarcascade/group_1_640x480_VGA.pgm", "gpu/lbpcascade/lbpcascade_frontalface.xml")))
{
    const cv::Mat img = readGpuImage(GetParam().first, cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());

    if (PERF_RUN_MUSA())
    {
        cv::Ptr<cv::musa::CascadeClassifier> d_cascade =
                cv::musa::CascadeClassifier::create(perf::TestBase::getDataPath(GetParam().second));

        const cv::musa::GpuMat d_img(img);
        cv::musa::GpuMat objects_buffer;

        TEST_CYCLE() d_cascade->detectMultiScale(d_img, objects_buffer);

        std::vector<cv::Rect> gpu_rects;
        d_cascade->convert(objects_buffer, gpu_rects);

        cv::groupRectangles(gpu_rects, 3, 0.2);
        SANITY_CHECK(gpu_rects);
    }
    else
    {
        cv::CascadeClassifier cascade;
        ASSERT_TRUE(cascade.load(perf::TestBase::getDataPath("gpu/lbpcascade/lbpcascade_frontalface.xml")));

        std::vector<cv::Rect> cpu_rects;

        TEST_CYCLE() cascade.detectMultiScale(img, cpu_rects);

        SANITY_CHECK(cpu_rects);
    }
}

}} // namespace
