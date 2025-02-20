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
namespace opencv_test {
    namespace {

#if defined(HAVE_NVCUVID) || defined(HAVE_NVCUVENC)
PARAM_TEST_CASE(CheckSet, cv::musa::DeviceInfo, std::string)
{
};

typedef tuple<std::string, int> check_extra_data_params_t;
PARAM_TEST_CASE(CheckExtraData, cv::musa::DeviceInfo, check_extra_data_params_t)
{
};

PARAM_TEST_CASE(Scaling, cv::musa::DeviceInfo, std::string, Size2f, Rect2f, Rect2f)
{
};

PARAM_TEST_CASE(Video, cv::musa::DeviceInfo, std::string)
{
};

PARAM_TEST_CASE(VideoReadRaw, cv::musa::DeviceInfo, std::string)
{
};

PARAM_TEST_CASE(CheckKeyFrame, cv::musa::DeviceInfo, std::string)
{
};

PARAM_TEST_CASE(CheckDecodeSurfaces, cv::musa::DeviceInfo, std::string)
{
};

PARAM_TEST_CASE(CheckInitParams, cv::musa::DeviceInfo, std::string, bool, bool, bool)
{
};

struct CheckParams : testing::TestWithParam<cv::musa::DeviceInfo>
{
    cv::musa::DeviceInfo devInfo;

    virtual void SetUp()
    {
        devInfo = GetParam();

        cv::musa::setDevice(devInfo.deviceID());
    }
};

#if defined(HAVE_NVCUVID)
//////////////////////////////////////////////////////
// VideoReader

//==========================================================================

MUSA_TEST_P(CheckSet, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());

    if (!videoio_registry::hasBackend(CAP_FFMPEG))
        throw SkipTestException("FFmpeg backend was not found");

    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + +"../" + GET_PARAM(1);
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile);
    double unsupportedVal = -1;
    ASSERT_FALSE(reader->get(cv::musacodec::VideoReaderProps::PROP_NOT_SUPPORTED, unsupportedVal));
    double rawModeVal = -1;
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_RAW_MODE, rawModeVal));
    ASSERT_FALSE(rawModeVal);
    ASSERT_TRUE(reader->set(cv::musacodec::VideoReaderProps::PROP_RAW_MODE,true));
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_RAW_MODE, rawModeVal));
    ASSERT_TRUE(rawModeVal);
    bool rawPacketsAvailable = false;
    while (reader->grab()) {
        double nRawPackages = -1;
        ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_NUMBER_OF_RAW_PACKAGES_SINCE_LAST_GRAB, nRawPackages));
        if (nRawPackages > 0) {
            rawPacketsAvailable = true;
            break;
        }
    }
    ASSERT_TRUE(rawPacketsAvailable);
}

MUSA_TEST_P(CheckExtraData, Reader)
{
    // RTSP streaming is only supported by the FFmpeg back end
    if (!videoio_registry::hasBackend(CAP_FFMPEG))
        throw SkipTestException("FFmpeg backend not found");

    cv::musa::setDevice(GET_PARAM(0).deviceID());
    const string path = get<0>(GET_PARAM(1));
    const int sz = get<1>(GET_PARAM(1));
    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + path;
    cv::musacodec::VideoReaderInitParams params;
    params.rawMode = true;
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
    double extraDataIdx = -1;
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_EXTRA_DATA_INDEX, extraDataIdx));
    ASSERT_EQ(extraDataIdx, 1 );
    ASSERT_TRUE(reader->grab());
    cv::Mat extraData;
    const bool newData = reader->retrieve(extraData, extraDataIdx);
    ASSERT_TRUE(newData && sz || !newData && !sz);
    ASSERT_EQ(extraData.total(), sz);
}

MUSA_TEST_P(CheckKeyFrame, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());

    // RTSP streaming is only supported by the FFmpeg back end
    if (!videoio_registry::hasBackend(CAP_FFMPEG))
        throw SkipTestException("FFmpeg backend not found");

    const string path = GET_PARAM(1);
    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + path;
    cv::musacodec::VideoReaderInitParams params;
    params.rawMode = true;
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
    double rawIdxBase = -1;
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_RAW_PACKAGES_BASE_INDEX, rawIdxBase));
    ASSERT_EQ(rawIdxBase, 2);
    constexpr int maxNPackagesToCheck = 2;
    int nPackages = 0;
    while (nPackages < maxNPackagesToCheck) {
        ASSERT_TRUE(reader->grab());
        double N = -1;
        ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_NUMBER_OF_RAW_PACKAGES_SINCE_LAST_GRAB,N));
        for (int i = rawIdxBase; i < N + rawIdxBase; i++) {
            nPackages++;
            double containsKeyFrame = i;
            ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_LRF_HAS_KEY_FRAME, containsKeyFrame));
            ASSERT_TRUE(nPackages == 1 && containsKeyFrame || nPackages == 2 && !containsKeyFrame) << "nPackage: " << i;
            if (nPackages >= maxNPackagesToCheck)
                break;
        }
    }
}

MUSA_TEST_P(Scaling, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());
    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + GET_PARAM(1);
    const Size2f targetSzIn = GET_PARAM(2);
    const Rect2f srcRoiIn = GET_PARAM(3);
    const Rect2f targetRoiIn = GET_PARAM(4);

    GpuMat frameOr;
    {
        cv::Ptr<cv::musacodec::VideoReader> readerGs = cv::musacodec::createVideoReader(inputFile);
        readerGs->set(musacodec::ColorFormat::GRAY);
        ASSERT_TRUE(readerGs->nextFrame(frameOr));
    }

    musacodec::VideoReaderInitParams params;
    params.targetSz = Size(frameOr.cols * targetSzIn.width, frameOr.rows * targetSzIn.height);
    params.srcRoi = Rect(frameOr.cols * srcRoiIn.x, frameOr.rows * srcRoiIn.y, frameOr.cols * srcRoiIn.width, frameOr.rows * srcRoiIn.height);
    params.targetRoi = Rect(params.targetSz.width * targetRoiIn.x, params.targetSz.height * targetRoiIn.y, params.targetSz.width * targetRoiIn.width,
        params.targetSz.height * targetRoiIn.height);
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
    reader->set(musacodec::ColorFormat::GRAY);
    GpuMat frame;
    ASSERT_TRUE(reader->nextFrame(frame));
    const musacodec::FormatInfo format = reader->format();
    Size targetSzOut;
    targetSzOut.width = params.targetSz.width - params.targetSz.width % 2; targetSzOut.height = params.targetSz.height - params.targetSz.height % 2;
    Rect srcRoiOut, targetRoiOut;
    srcRoiOut.x = params.srcRoi.x - params.srcRoi.x % 4; srcRoiOut.width = params.srcRoi.width - params.srcRoi.width % 4;
    srcRoiOut.y = params.srcRoi.y - params.srcRoi.y % 2; srcRoiOut.height = params.srcRoi.height - params.srcRoi.height % 2;
    targetRoiOut.x = params.targetRoi.x - params.targetRoi.x % 4; targetRoiOut.width = params.targetRoi.width - params.targetRoi.width % 4;
    targetRoiOut.y = params.targetRoi.y - params.targetRoi.y % 2; targetRoiOut.height = params.targetRoi.height - params.targetRoi.height % 2;
    ASSERT_TRUE(format.valid && format.targetSz == targetSzOut && format.srcRoi == srcRoiOut && format.targetRoi == targetRoiOut);
    ASSERT_TRUE(frame.size() == targetSzOut);
    GpuMat frameGs;
    cv::musa::resize(frameOr(srcRoiOut), frameGs, targetRoiOut.size(), 0, 0, INTER_AREA);
    // assert on mean absolute error due to different resize algorithms
    const double mae = cv::musa::norm(frameGs, frame(targetRoiOut), NORM_L1)/frameGs.size().area();
    ASSERT_LT(mae, 2.35);
}

MUSA_TEST_P(Video, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());

    // MUSA demuxer has to fall back to ffmpeg to process "cv/video/768x576.avi"
    if (GET_PARAM(1) == "cv/video/768x576.avi" && !videoio_registry::hasBackend(CAP_FFMPEG))
        throw SkipTestException("FFmpeg backend not found");

#ifdef _WIN32  // handle old FFmpeg backend
    if (GET_PARAM(1) == "/cv/tracking/faceocc2/data/faceocc2.webm")
        throw SkipTestException("Feature not yet supported by Windows FFmpeg shared library!");
#endif

    const std::vector<std::pair< musacodec::ColorFormat, int>> formatsToChannels = {
        {musacodec::ColorFormat::GRAY,1},
        {musacodec::ColorFormat::BGR,3},
        {musacodec::ColorFormat::BGRA,4},
        {musacodec::ColorFormat::YUV,1}
    };

    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + GET_PARAM(1);
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile);
    cv::musacodec::FormatInfo fmt = reader->format();
    cv::musa::GpuMat frame;
    for (int i = 0; i < 10; i++)
    {
        // request a different colour format for each frame
        const std::pair< musacodec::ColorFormat, int>& formatToChannels = formatsToChannels[i % formatsToChannels.size()];
        reader->set(formatToChannels.first);
        double colorFormat;
        ASSERT_TRUE(reader->get(musacodec::VideoReaderProps::PROP_COLOR_FORMAT, colorFormat) && static_cast<musacodec::ColorFormat>(colorFormat) == formatToChannels.first);
        ASSERT_TRUE(reader->nextFrame(frame));
        if(!fmt.valid)
            fmt = reader->format();
        const int height = formatToChannels.first == musacodec::ColorFormat::YUV ? 1.5 * fmt.height : fmt.height;
        ASSERT_TRUE(frame.cols == fmt.width && frame.rows == height);
        ASSERT_FALSE(frame.empty());
        ASSERT_TRUE(frame.channels() == formatToChannels.second);
    }
}

MUSA_TEST_P(VideoReadRaw, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());

    // RTSP streaming is only supported by the FFmpeg back end
    if (!videoio_registry::hasBackend(CAP_FFMPEG))
        throw SkipTestException("FFmpeg backend not found");

    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + GET_PARAM(1);
    const string fileNameOut = tempfile("test_container_stream");
    {
        std::ofstream file(fileNameOut, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        cv::musacodec::VideoReaderInitParams params;
        params.rawMode = true;
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
        double rawIdxBase = -1;
        ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_RAW_PACKAGES_BASE_INDEX, rawIdxBase));
        ASSERT_EQ(rawIdxBase, 2);
        cv::musa::GpuMat frame;
        for (int i = 0; i < 100; i++)
        {
            ASSERT_TRUE(reader->grab());
            ASSERT_TRUE(reader->retrieve(frame));
            ASSERT_FALSE(frame.empty());
            double N = -1;
            ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_NUMBER_OF_RAW_PACKAGES_SINCE_LAST_GRAB,N));
            ASSERT_TRUE(N >= 0) << N << " < 0";
            for (int i = rawIdxBase; i <= N + rawIdxBase; i++) {
                Mat rawPackets;
                reader->retrieve(rawPackets, i);
                file.write((char*)rawPackets.data, rawPackets.total());
            }
        }
    }

    std::cout << "Checking written video stream: " << fileNameOut << std::endl;

    {
        cv::Ptr<cv::musacodec::VideoReader> readerReference = cv::musacodec::createVideoReader(inputFile);
        cv::musacodec::VideoReaderInitParams params;
        params.rawMode = true;
        cv::Ptr<cv::musacodec::VideoReader> readerActual = cv::musacodec::createVideoReader(fileNameOut, {}, params);
        double decodedFrameIdx = -1;
        ASSERT_TRUE(readerActual->get(cv::musacodec::VideoReaderProps::PROP_DECODED_FRAME_IDX, decodedFrameIdx));
        ASSERT_EQ(decodedFrameIdx, 0);
        cv::musa::GpuMat reference, actual;
        cv::Mat referenceHost, actualHost;
        for (int i = 0; i < 100; i++)
        {
            ASSERT_TRUE(readerReference->nextFrame(reference));
            ASSERT_TRUE(readerActual->grab());
            ASSERT_TRUE(readerActual->retrieve(actual, decodedFrameIdx));
            actual.download(actualHost);
            reference.download(referenceHost);
            ASSERT_TRUE(cvtest::norm(actualHost, referenceHost, NORM_INF) == 0);
        }
    }

    ASSERT_EQ(0, remove(fileNameOut.c_str()));
}

MUSA_TEST_P(CheckParams, Reader)
{
    std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../highgui/video/big_buck_bunny.mp4";
    {
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile);
        double msActual = -1;
        ASSERT_FALSE(reader->get(cv::VideoCaptureProperties::CAP_PROP_OPEN_TIMEOUT_MSEC, msActual));
    }

    {
        constexpr int msReference = 3333;
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {
            cv::VideoCaptureProperties::CAP_PROP_OPEN_TIMEOUT_MSEC, msReference });
        double msActual = -1;
        ASSERT_TRUE(reader->get(cv::VideoCaptureProperties::CAP_PROP_OPEN_TIMEOUT_MSEC, msActual));
        ASSERT_EQ(msActual, msReference);
    }

    {
        std::vector<bool> exceptionsThrown = { false,true };
        std::vector<int> capPropFormats = { -1,0 };
        for (int i = 0; i < capPropFormats.size(); i++) {
            bool exceptionThrown = false;
            try {
                cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {
                    cv::VideoCaptureProperties::CAP_PROP_FORMAT, capPropFormats.at(i) });
            }
            catch (cv::Exception &ex) {
                if (ex.code == Error::StsUnsupportedFormat)
                    exceptionThrown = true;
            }
            ASSERT_EQ(exceptionThrown, exceptionsThrown.at(i));
        }
    }
}

MUSA_TEST_P(CheckDecodeSurfaces, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());
    const std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + GET_PARAM(1);
    int ulNumDecodeSurfaces = 0;
    {
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile);
        cv::musacodec::FormatInfo fmt = reader->format();
        if (!fmt.valid) {
            reader->grab();
            fmt = reader->format();
            ASSERT_TRUE(fmt.valid);
        }
        ulNumDecodeSurfaces = fmt.ulNumDecodeSurfaces;
    }

    {
        cv::musacodec::VideoReaderInitParams params;
        params.minNumDecodeSurfaces = ulNumDecodeSurfaces - 1;
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
        cv::musacodec::FormatInfo fmt = reader->format();
        if (!fmt.valid) {
            reader->grab();
            fmt = reader->format();
            ASSERT_TRUE(fmt.valid);
        }
        ASSERT_TRUE(fmt.ulNumDecodeSurfaces == ulNumDecodeSurfaces);
        for (int i = 0; i < 100; i++) ASSERT_TRUE(reader->grab());
    }

    {
        cv::musacodec::VideoReaderInitParams params;
        params.minNumDecodeSurfaces = ulNumDecodeSurfaces + 1;
        cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
        cv::musacodec::FormatInfo fmt = reader->format();
        if (!fmt.valid) {
            reader->grab();
            fmt = reader->format();
            ASSERT_TRUE(fmt.valid);
        }
        ASSERT_TRUE(fmt.ulNumDecodeSurfaces == ulNumDecodeSurfaces + 1);
        for (int i = 0; i < 100; i++) ASSERT_TRUE(reader->grab());
    }
}

MUSA_TEST_P(CheckInitParams, Reader)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());
    const std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "../" + GET_PARAM(1);
    cv::musacodec::VideoReaderInitParams params;
    params.udpSource = GET_PARAM(2);
    params.allowFrameDrop = GET_PARAM(3);
    params.rawMode = GET_PARAM(4);
    double udpSource = 0, allowFrameDrop = 0, rawMode = 0;
    cv::Ptr<cv::musacodec::VideoReader> reader = cv::musacodec::createVideoReader(inputFile, {}, params);
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_UDP_SOURCE, udpSource) && static_cast<bool>(udpSource) == params.udpSource);
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_ALLOW_FRAME_DROP, allowFrameDrop) && static_cast<bool>(allowFrameDrop) == params.allowFrameDrop);
    ASSERT_TRUE(reader->get(cv::musacodec::VideoReaderProps::PROP_RAW_MODE, rawMode) && static_cast<bool>(rawMode) == params.rawMode);
}

#endif // HAVE_NVCUVID

#if defined(_WIN32) && defined(HAVE_NVCUVENC)
//////////////////////////////////////////////////////
// VideoWriter

MUSA_TEST_P(Video, Writer)
{
    cv::musa::setDevice(GET_PARAM(0).deviceID());

    const std::string inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "video/" + GET_PARAM(1);

    std::string outputFile = cv::tempfile(".avi");
    const double FPS = 25.0;

    cv::VideoCapture reader(inputFile);
    ASSERT_TRUE(reader.isOpened());

    cv::Ptr<cv::musacodec::VideoWriter> d_writer;

    cv::Mat frame;
    cv::musa::GpuMat d_frame;

    for (int i = 0; i < 10; ++i)
    {
        reader >> frame;
        ASSERT_FALSE(frame.empty());

        d_frame.upload(frame);

        if (d_writer.empty())
            d_writer = cv::musacodec::createVideoWriter(outputFile, frame.size(), FPS);

        d_writer->write(d_frame);
    }

    reader.release();
    d_writer.release();

    reader.open(outputFile);
    ASSERT_TRUE(reader.isOpened());

    for (int i = 0; i < 5; ++i)
    {
        reader >> frame;
        ASSERT_FALSE(frame.empty());
    }
}

#endif // _WIN32, HAVE_NVCUVENC

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckSet, testing::Combine(
    ALL_DEVICES,
    testing::Values("highgui/video/big_buck_bunny.mp4")));

#define VIDEO_SRC_SCALING "highgui/video/big_buck_bunny.mp4"
#define TARGET_SZ Size2f(1,1), Size2f(0.8,0.9), Size2f(2.3,1.8)
#define SRC_ROI Rect2f(0,0,1,1), Rect2f(0.25,0.25,0.5,0.5)
#define TARGET_ROI Rect2f(0,0,1,1), Rect2f(0.2,0.3,0.6,0.7)
INSTANTIATE_TEST_CASE_P(MUSA_Codec, Scaling, testing::Combine(
    ALL_DEVICES, testing::Values(VIDEO_SRC_SCALING), testing::Values(TARGET_SZ), testing::Values(SRC_ROI), testing::Values(TARGET_ROI)));

#define VIDEO_SRC_R "highgui/video/big_buck_bunny.mp4", "cv/video/768x576.avi", "cv/video/1920x1080.avi", "highgui/video/big_buck_bunny.avi", \
    "highgui/video/big_buck_bunny.h264", "highgui/video/big_buck_bunny.h265", "highgui/video/big_buck_bunny.mpg", \
    "highgui/video/sample_322x242_15frames.yuv420p.libvpx-vp9.mp4", "highgui/video/sample_322x242_15frames.yuv420p.libaom-av1.mp4", \
    "cv/tracking/faceocc2/data/faceocc2.webm"
INSTANTIATE_TEST_CASE_P(MUSA_Codec, Video, testing::Combine(
    ALL_DEVICES,
    testing::Values(VIDEO_SRC_R)));

#define VIDEO_SRC_RW "highgui/video/big_buck_bunny.h264", "highgui/video/big_buck_bunny.h265"
INSTANTIATE_TEST_CASE_P(MUSA_Codec, VideoReadRaw, testing::Combine(
    ALL_DEVICES,
    testing::Values(VIDEO_SRC_RW)));

const check_extra_data_params_t check_extra_data_params[] =
{
    check_extra_data_params_t("highgui/video/big_buck_bunny.mp4", 45),
    check_extra_data_params_t("highgui/video/big_buck_bunny.mov", 45),
    check_extra_data_params_t("highgui/video/big_buck_bunny.mjpg.avi", 0)
};

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckExtraData, testing::Combine(
    ALL_DEVICES,
    testing::ValuesIn(check_extra_data_params)));

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckKeyFrame, testing::Combine(
    ALL_DEVICES,
    testing::Values(VIDEO_SRC_R)));

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckParams, ALL_DEVICES);

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckDecodeSurfaces, testing::Combine(
    ALL_DEVICES,
    testing::Values("highgui/video/big_buck_bunny.mp4")));

INSTANTIATE_TEST_CASE_P(MUSA_Codec, CheckInitParams, testing::Combine(
    ALL_DEVICES,
    testing::Values("highgui/video/big_buck_bunny.mp4"),
    testing::Values(true,false), testing::Values(true,false), testing::Values(true,false)));

#endif // HAVE_NVCUVID || HAVE_NVCUVENC
}} // namespace
