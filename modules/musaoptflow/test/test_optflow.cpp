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

//////////////////////////////////////////////////////
// BroxOpticalFlow

//#define BROX_DUMP

struct BroxOpticalFlow : testing::TestWithParam<cv::musa::DeviceInfo>
{
    cv::musa::DeviceInfo devInfo;

    virtual void SetUp()
    {
        devInfo = GetParam();

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(BroxOpticalFlow, Regression)
{
    cv::Mat frame0 = readGpuImageType("opticalflow/frame0.png", CV_32FC1);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImageType("opticalflow/frame1.png", CV_32FC1);
    ASSERT_FALSE(frame1.empty());

    cv::Ptr<cv::musa::BroxOpticalFlow> brox =
            cv::musa::BroxOpticalFlow::create(0.197 /*alpha*/, 50.0 /*gamma*/, 0.8 /*scale_factor*/,
                                              10 /*inner_iterations*/, 77 /*outer_iterations*/, 10 /*solver_iterations*/);

    cv::musa::GpuMat flow;
    brox->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);

    cv::musa::GpuMat flows[2];
    cv::musa::split(flow, flows);

    cv::musa::GpuMat u = flows[0];
    cv::musa::GpuMat v = flows[1];

    std::string fname(cvtest::TS::ptr()->get_data_path());
    if (devInfo.majorVersion() >= 2)
        fname += "opticalflow/brox_optical_flow_cc20.bin";
    else
        fname += "opticalflow/brox_optical_flow.bin";

#ifndef BROX_DUMP
    std::ifstream f(fname.c_str(), std::ios_base::binary);

    int rows, cols;

    f.read((char*) &rows, sizeof(rows));
    f.read((char*) &cols, sizeof(cols));

    cv::Mat u_gold(rows, cols, CV_32FC1);

    for (int i = 0; i < u_gold.rows; ++i)
        f.read(u_gold.ptr<char>(i), u_gold.cols * sizeof(float));

    cv::Mat v_gold(rows, cols, CV_32FC1);

    for (int i = 0; i < v_gold.rows; ++i)
        f.read(v_gold.ptr<char>(i), v_gold.cols * sizeof(float));

    EXPECT_MAT_SIMILAR(u_gold, u, 1e-3);
    EXPECT_MAT_SIMILAR(v_gold, v, 1e-3);
#else
    std::ofstream f(fname.c_str(), std::ios_base::binary);

    f.write((char*) &u.rows, sizeof(u.rows));
    f.write((char*) &u.cols, sizeof(u.cols));

    cv::Mat h_u(u);
    cv::Mat h_v(v);

    for (int i = 0; i < u.rows; ++i)
        f.write(h_u.ptr<char>(i), u.cols * sizeof(float));

    for (int i = 0; i < v.rows; ++i)
        f.write(h_v.ptr<char>(i), v.cols * sizeof(float));
#endif
}

MUSA_TEST_P(BroxOpticalFlow, OpticalFlowNan)
{
    cv::Mat frame0 = readGpuImageType("opticalflow/frame0.png", CV_32FC1);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImageType("opticalflow/frame1.png", CV_32FC1);
    ASSERT_FALSE(frame1.empty());

    cv::Mat r_frame0, r_frame1;
    cv::resize(frame0, r_frame0, cv::Size(1380,1000));
    cv::resize(frame1, r_frame1, cv::Size(1380,1000));

    cv::Ptr<cv::musa::BroxOpticalFlow> brox =
            cv::musa::BroxOpticalFlow::create(0.197 /*alpha*/, 50.0 /*gamma*/, 0.8 /*scale_factor*/,
                                              10 /*inner_iterations*/, 77 /*outer_iterations*/, 10 /*solver_iterations*/);

    cv::musa::GpuMat flow;
    brox->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);

    cv::musa::GpuMat flows[2];
    cv::musa::split(flow, flows);

    cv::musa::GpuMat u = flows[0];
    cv::musa::GpuMat v = flows[1];

    cv::Mat h_u, h_v;
    u.download(h_u);
    v.download(h_v);

    EXPECT_TRUE(cv::checkRange(h_u));
    EXPECT_TRUE(cv::checkRange(h_v));
};

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, BroxOpticalFlow, ALL_DEVICES);

//////////////////////////////////////////////////////
// PyrLKOpticalFlow

namespace
{
    IMPLEMENT_PARAM_CLASS(Chan, int)
    IMPLEMENT_PARAM_CLASS(DataType, int)
}

PARAM_TEST_CASE(PyrLKOpticalFlow, cv::musa::DeviceInfo, Chan, DataType)
{
    cv::musa::DeviceInfo devInfo;
    int channels;
    int dataType;
    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        channels = GET_PARAM(1);
        dataType = GET_PARAM(2);
        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(PyrLKOpticalFlow, Sparse)
{
    cv::Mat frame0 = readGpuImage("opticalflow/frame0.png", channels == 1 ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/frame1.png", channels == 1 ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
    ASSERT_FALSE(frame1.empty());

    cv::Mat gray_frame;
    if (channels == 1)
        gray_frame = frame0;
    else
        cv::cvtColor(frame0, gray_frame, cv::COLOR_BGR2GRAY);

    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gray_frame, pts, 1000, 0.01, 0.0);

    cv::musa::GpuMat d_pts;
    cv::Mat pts_mat(1, (int) pts.size(), CV_32FC2, (void*) &pts[0]);
    d_pts.upload(pts_mat);

    cv::Ptr<cv::musa::SparsePyrLKOpticalFlow> pyrLK =
            cv::musa::SparsePyrLKOpticalFlow::create();

    std::vector<cv::Point2f> nextPts_gold;
    std::vector<unsigned char> status_gold;
    cv::calcOpticalFlowPyrLK(frame0, frame1, pts, nextPts_gold, status_gold, cv::noArray());


    cv::musa::GpuMat d_nextPts;
    cv::musa::GpuMat d_status;
    cv::Mat converted0, converted1;
    if(channels == 4)
    {
        cv::cvtColor(frame0, frame0, cv::COLOR_BGR2BGRA);
        cv::cvtColor(frame1, frame1, cv::COLOR_BGR2BGRA);
    }
    frame0.convertTo(converted0, dataType);
    frame1.convertTo(converted1, dataType);

    pyrLK->calc(musaLoadMat(converted0), musaLoadMat(converted1), d_pts, d_nextPts, d_status);

    std::vector<cv::Point2f> nextPts(d_nextPts.cols);
    cv::Mat nextPts_mat(1, d_nextPts.cols, CV_32FC2, (void*)&nextPts[0]);
    d_nextPts.download(nextPts_mat);

    std::vector<unsigned char> status(d_status.cols);
    cv::Mat status_mat(1, d_status.cols, CV_8UC1, (void*)&status[0]);
    d_status.download(status_mat);

    ASSERT_EQ(nextPts_gold.size(), nextPts.size());
    ASSERT_EQ(status_gold.size(), status.size());

    size_t mistmatch = 0;
    for (size_t i = 0; i < nextPts.size(); ++i)
    {
        cv::Point2i a = nextPts[i];
        cv::Point2i b = nextPts_gold[i];

        if (status[i] != status_gold[i])
        {
            ++mistmatch;
            continue;
        }

        if (status[i])
        {
            bool eq = std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;

            if (!eq)
                ++mistmatch;
        }
    }

    double bad_ratio = static_cast<double>(mistmatch) / nextPts.size();

    ASSERT_LE(bad_ratio, 0.01);


}

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, PyrLKOpticalFlow, testing::Combine(
    ALL_DEVICES,
    testing::Values(Chan(1), Chan(3), Chan(4)),
    testing::Values(DataType(CV_8U), DataType(CV_16U), DataType(CV_32S), DataType(CV_32F))));



//////////////////////////////////////////////////////
// FarnebackOpticalFlow

namespace
{
    IMPLEMENT_PARAM_CLASS(PyrScale, double)
    IMPLEMENT_PARAM_CLASS(PolyN, int)
    CV_FLAGS(FarnebackOptFlowFlags, 0, OPTFLOW_FARNEBACK_GAUSSIAN)
    IMPLEMENT_PARAM_CLASS(UseInitFlow, bool)
}

PARAM_TEST_CASE(FarnebackOpticalFlow, cv::musa::DeviceInfo, PyrScale, PolyN, FarnebackOptFlowFlags, UseInitFlow)
{
    cv::musa::DeviceInfo devInfo;
    double pyrScale;
    int polyN;
    int flags;
    bool useInitFlow;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        pyrScale = GET_PARAM(1);
        polyN = GET_PARAM(2);
        flags = GET_PARAM(3);
        useInitFlow = GET_PARAM(4);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(FarnebackOpticalFlow, Accuracy)
{
    cv::Mat frame0 = readGpuImage("opticalflow/rubberwhale1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/rubberwhale2.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    double polySigma = polyN <= 5 ? 1.1 : 1.5;

    cv::Ptr<cv::musa::FarnebackOpticalFlow> farn =
            cv::musa::FarnebackOpticalFlow::create();
    farn->setPyrScale(pyrScale);
    farn->setPolyN(polyN);
    farn->setPolySigma(polySigma);
    farn->setFlags(flags);

    cv::musa::GpuMat d_flow;
    farn->calc(musaLoadMat(frame0), musaLoadMat(frame1), d_flow);

    cv::Mat flow;
    if (useInitFlow)
    {
        d_flow.download(flow);

        farn->setFlags(farn->getFlags() | cv::OPTFLOW_USE_INITIAL_FLOW);
        farn->calc(musaLoadMat(frame0), musaLoadMat(frame1), d_flow);
    }

    cv::calcOpticalFlowFarneback(
        frame0, frame1, flow, farn->getPyrScale(), farn->getNumLevels(), farn->getWinSize(),
        farn->getNumIters(), farn->getPolyN(), farn->getPolySigma(), farn->getFlags());

    // Relax test limit when the flag is set
    if (farn->getFlags() & cv::OPTFLOW_FARNEBACK_GAUSSIAN)
    {
        EXPECT_MAT_SIMILAR(flow, d_flow, 2e-2);
    }
    else
    {
        EXPECT_MAT_SIMILAR(flow, d_flow, 1e-4);
    }
}

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, FarnebackOpticalFlow, testing::Combine(
    ALL_DEVICES,
    testing::Values(PyrScale(0.3), PyrScale(0.5), PyrScale(0.8)),
    testing::Values(PolyN(5), PolyN(7)),
    testing::Values(FarnebackOptFlowFlags(0), FarnebackOptFlowFlags(cv::OPTFLOW_FARNEBACK_GAUSSIAN)),
    testing::Values(UseInitFlow(false), UseInitFlow(true))));

//////////////////////////////////////////////////////
// OpticalFlowDual_TVL1

namespace
{
    IMPLEMENT_PARAM_CLASS(Gamma, double)
}

PARAM_TEST_CASE(OpticalFlowDual_TVL1, cv::musa::DeviceInfo, Gamma)
{
    cv::musa::DeviceInfo devInfo;
    double gamma;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        gamma = GET_PARAM(1);

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(OpticalFlowDual_TVL1, Accuracy)
{
    cv::Mat frame0 = readGpuImage("opticalflow/rubberwhale1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/rubberwhale2.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    cv::Ptr<cv::musa::OpticalFlowDual_TVL1> d_alg =
            cv::musa::OpticalFlowDual_TVL1::create();
    d_alg->setNumIterations(10);
    d_alg->setGamma(gamma);

    cv::musa::GpuMat d_flow;
    d_alg->calc(musaLoadMat(frame0), musaLoadMat(frame1), d_flow);

    cv::Ptr<cv::optflow::DualTVL1OpticalFlow> alg = cv::optflow::createOptFlow_DualTVL1();
    alg->setMedianFiltering(1);
    alg->setInnerIterations(1);
    alg->setOuterIterations(d_alg->getNumIterations());
    alg->setGamma(gamma);

    cv::Mat flow;
    alg->calc(frame0, frame1, flow);

    EXPECT_MAT_SIMILAR(flow, d_flow, 4e-3);
}

class TVL1AsyncParallelLoopBody : public cv::ParallelLoopBody
{
public:
    TVL1AsyncParallelLoopBody(const cv::musa::GpuMat& d_img1_, const cv::musa::GpuMat& d_img2_, cv::musa::GpuMat* d_flow_, int iterations_, double gamma_)
        : d_img1(d_img1_), d_img2(d_img2_), d_flow(d_flow_), iterations(iterations_), gamma(gamma_) {}
    ~TVL1AsyncParallelLoopBody() {}
    void operator()(const cv::Range& r) const
    {
        for (int i = r.start; i < r.end; i++) {
            cv::musa::Stream stream;
            cv::Ptr<cv::musa::OpticalFlowDual_TVL1> d_alg = cv::musa::OpticalFlowDual_TVL1::create();
            d_alg->setNumIterations(iterations);
            d_alg->setGamma(gamma);
            d_alg->calc(d_img1, d_img2, d_flow[i], stream);
            stream.waitForCompletion();
        }
    }
protected:
    const cv::musa::GpuMat& d_img1;
    const cv::musa::GpuMat& d_img2;
    cv::musa::GpuMat* d_flow;
    int iterations;
    double gamma;
};

#define NUM_STREAMS 16

MUSA_TEST_P(OpticalFlowDual_TVL1, Async)
{
    cv::Mat frame0 = readGpuImage("opticalflow/rubberwhale1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/rubberwhale2.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    const int iterations = 10;

    // Synchronous call
    cv::Ptr<cv::musa::OpticalFlowDual_TVL1> d_alg =
            cv::musa::OpticalFlowDual_TVL1::create();
    d_alg->setNumIterations(iterations);
    d_alg->setGamma(gamma);

    cv::musa::GpuMat d_flow_gold;
    d_alg->calc(musaLoadMat(frame0), musaLoadMat(frame1), d_flow_gold);

    // Asynchronous call
    cv::musa::GpuMat d_flow[NUM_STREAMS];
    cv::parallel_for_(cv::Range(0, NUM_STREAMS), TVL1AsyncParallelLoopBody(musaLoadMat(frame0), musaLoadMat(frame1), d_flow, iterations, gamma));

    // Compare the results of synchronous call and asynchronous call
    for (int i = 0; i < NUM_STREAMS; i++)
        EXPECT_MAT_NEAR(d_flow_gold, d_flow[i], 0.0);
}

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, OpticalFlowDual_TVL1, testing::Combine(
    ALL_DEVICES,
    testing::Values(Gamma(0.0), Gamma(1.0))));


//////////////////////////////////////////////////////
// NvidiaOpticalFlow_1_0

struct NvidiaOpticalFlow_1_0 : testing::TestWithParam<cv::musa::DeviceInfo>
{
    cv::musa::DeviceInfo devInfo;

    virtual void SetUp()
    {
        devInfo = GetParam();

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(NvidiaOpticalFlow_1_0, Regression)
{
    cv::Mat frame0 = readGpuImage("opticalflow/frame0.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/frame1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    cv::Ptr<cv::musa::NvidiaOpticalFlow_1_0> d_nvof;
    try
    {
        d_nvof = cv::musa::NvidiaOpticalFlow_1_0::create(frame0.size(),
            cv::musa::NvidiaOpticalFlow_1_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_SLOW);
    }
    catch (const cv::Exception& e)
    {
        if (e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
            throw SkipTestException("Current configuration is not supported");
        throw;
    }
    const int gridSize = d_nvof->getGridSize();

    Mat flow, upsampledFlow;
    d_nvof->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);
    d_nvof->upSampler(flow, frame0.size(), gridSize, upsampledFlow);

    std::string fname(cvtest::TS::ptr()->get_data_path());
    fname += "opticalflow/nvofGolden.flo";
    cv::Mat golden = cv::readOpticalFlow(fname.c_str());
    ASSERT_FALSE(golden.empty());

    EXPECT_MAT_SIMILAR(golden, upsampledFlow, 1e-10);
    d_nvof->collectGarbage();
}

MUSA_TEST_P(NvidiaOpticalFlow_1_0, OpticalFlowNan)
{
    cv::Mat frame0 = readGpuImage("opticalflow/rubberwhale1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/rubberwhale2.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    cv::Mat r_frame0, r_frame1;

    cv::Ptr<cv::musa::NvidiaOpticalFlow_1_0> d_nvof;
    try
    {
        d_nvof = cv::musa::NvidiaOpticalFlow_1_0::create(frame0.size(),
            cv::musa::NvidiaOpticalFlow_1_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_SLOW);
    }
    catch (const cv::Exception& e)
    {
        if (e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
            throw SkipTestException("Current configuration is not supported");
        throw;
    }

    Mat flow, flowx, flowy;
    d_nvof->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);

    Mat planes[] = { flowx, flowy };
    split(flow, planes);
    flowx = planes[0]; flowy = planes[1];

    EXPECT_TRUE(cv::checkRange(flowx));
    EXPECT_TRUE(cv::checkRange(flowy));
    d_nvof->collectGarbage();
};

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, NvidiaOpticalFlow_1_0, ALL_DEVICES);

//////////////////////////////////////////////////////
// NvidiaOpticalFlow_2_0

struct NvidiaOpticalFlow_2_0 : testing::TestWithParam<cv::musa::DeviceInfo>
{
    cv::musa::DeviceInfo devInfo;

    virtual void SetUp()
    {
        devInfo = GetParam();

        cv::musa::setDevice(devInfo.deviceID());
    }
};

MUSA_TEST_P(NvidiaOpticalFlow_2_0, Regression)
{
    cv::Mat frame0 = readGpuImage("opticalflow/frame0.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/frame1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    cv::Ptr<cv::musa::NvidiaOpticalFlow_2_0> d_nvof;
    try
    {
        d_nvof = cv::musa::NvidiaOpticalFlow_2_0::create(frame0.size(),
            cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_SLOW);
    }
    catch (const cv::Exception& e)
    {
        if (e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
            throw SkipTestException("Current configuration is not supported");
        throw;
    }

    Mat flow, upsampledFlow;
    d_nvof->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);
    d_nvof->convertToFloat(flow, upsampledFlow);

    std::string fname(cvtest::TS::ptr()->get_data_path());
    fname += "opticalflow/nvofGolden_2.flo";
    cv::Mat golden = cv::readOpticalFlow(fname.c_str());
    ASSERT_FALSE(golden.empty());

    EXPECT_MAT_SIMILAR(golden, upsampledFlow, 1e-10);
}

MUSA_TEST_P(NvidiaOpticalFlow_2_0, OpticalFlowNan)
{
    cv::Mat frame0 = readGpuImage("opticalflow/rubberwhale1.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame0.empty());

    cv::Mat frame1 = readGpuImage("opticalflow/rubberwhale2.png", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(frame1.empty());

    cv::Mat r_frame0, r_frame1;

    cv::Ptr<cv::musa::NvidiaOpticalFlow_2_0> d_nvof;
    try
    {
        d_nvof = cv::musa::NvidiaOpticalFlow_2_0::create(frame0.size(),
            cv::musa::NvidiaOpticalFlow_2_0::NVIDIA_OF_PERF_LEVEL::NV_OF_PERF_LEVEL_SLOW);
    }
    catch (const cv::Exception& e)
    {
        if (e.code == Error::StsBadFunc || e.code == Error::StsBadArg || e.code == Error::StsNullPtr)
            throw SkipTestException("Current configuration is not supported");
        throw;
    }

    Mat flow, flowx, flowy;
    d_nvof->calc(musaLoadMat(frame0), musaLoadMat(frame1), flow);

    Mat planes[] = { flowx, flowy };
    split(flow, planes);
    flowx = planes[0]; flowy = planes[1];

    EXPECT_TRUE(cv::checkRange(flowx));
    EXPECT_TRUE(cv::checkRange(flowy));
};

INSTANTIATE_TEST_CASE_P(MUSA_OptFlow, NvidiaOpticalFlow_2_0, ALL_DEVICES);

}} // namespace
#endif // HAVE_MUSA
