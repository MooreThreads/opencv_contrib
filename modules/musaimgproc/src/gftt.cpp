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

#include "precomp.hpp"

using namespace cv;
using namespace cv::musa;

#if !defined (HAVE_MUSA) || defined (MUSA_DISABLER) || !defined(HAVE_OPENCV_MUSAARITHM)

Ptr<musa::CornersDetector> cv::musa::createGoodFeaturesToTrackDetector(int, int, double, double, int, bool, double) { throw_no_musa(); return Ptr<musa::CornersDetector>(); }

#else /* !defined (HAVE_MUSA) */

namespace cv { namespace musa { namespace device
{
    namespace gfft
    {
        int findCorners_gpu(const musaTextureObject_t &eigTex_, const int &rows, const int &cols, float threshold, PtrStepSzb mask, float2* corners, int max_count, int* counterPtr, musaStream_t stream);
        void sortCorners_gpu(const musaTextureObject_t &eigTex_, float2* corners, int count, musaStream_t stream);
    }
}}}

namespace
{
    class GoodFeaturesToTrackDetector : public CornersDetector
    {
    public:
        GoodFeaturesToTrackDetector(int srcType, int maxCorners, double qualityLevel, double minDistance,
                                    int blockSize, bool useHarrisDetector, double harrisK);
        ~GoodFeaturesToTrackDetector();
        void detect(InputArray image, OutputArray corners, InputArray mask, Stream& stream);

    private:
        int maxCorners_;
        double qualityLevel_;
        double minDistance_;

        Ptr<musa::CornernessCriteria> cornerCriteria_;

        GpuMat Dx_;
        GpuMat Dy_;
        GpuMat buf_;
        GpuMat eig_;
        GpuMat tmpCorners_;

        int* counterPtr_;
    };

    GoodFeaturesToTrackDetector::GoodFeaturesToTrackDetector(int srcType, int maxCorners, double qualityLevel, double minDistance,
                                                             int blockSize, bool useHarrisDetector, double harrisK) :
        maxCorners_(maxCorners), qualityLevel_(qualityLevel), minDistance_(minDistance)
    {
        CV_Assert( qualityLevel_ > 0 && minDistance_ >= 0 && maxCorners_ >= 0 );

        cornerCriteria_ = useHarrisDetector ?
                    musa::createHarrisCorner(srcType, blockSize, 3, harrisK) :
                    musa::createMinEigenValCorner(srcType, blockSize, 3);
        musaSafeCall(musaMalloc(&counterPtr_, sizeof(int)));
    }

    GoodFeaturesToTrackDetector::~GoodFeaturesToTrackDetector()
    {
        musaSafeCall(musaFree(counterPtr_));
    }

    void GoodFeaturesToTrackDetector::detect(InputArray _image, OutputArray _corners, InputArray _mask, Stream& stream)
    {
        using namespace cv::musa::device::gfft;

        GpuMat image = _image.getMUSAGpuMat();
        GpuMat mask = _mask.getMUSAGpuMat();

        CV_Assert( mask.empty() || (mask.type() == CV_8UC1 && mask.size() == image.size()) );

        ensureSizeIsEnough(image.size(), CV_32FC1, eig_);
        cornerCriteria_->compute(image, eig_, stream);

        double maxVal = 0;
        musa::minMax(eig_, 0, &maxVal);
        musaStream_t stream_ = StreamAccessor::getStream(stream);
        ensureSizeIsEnough(1, std::max(1000, static_cast<int>(image.size().area() * 0.05)), CV_32FC2, tmpCorners_);

        //create texture object for findCorners_gpu and sortCorners_gpu
        musaTextureDesc texDesc;
        memset(&texDesc, 0, sizeof(texDesc));
        texDesc.readMode = musaReadModeElementType;
        texDesc.filterMode = musaFilterModePoint;
        texDesc.addressMode[0] = musaAddressModeClamp;
        texDesc.addressMode[1] = musaAddressModeClamp;
        texDesc.addressMode[2] = musaAddressModeClamp;

        musaTextureObject_t eigTex_;
        PtrStepSzf eig = eig_;
        cv::musa::device::createTextureObjectPitch2D<float>(&eigTex_, eig, texDesc);

        int total = findCorners_gpu(eigTex_, eig_.rows, eig_.cols, static_cast<float>(maxVal * qualityLevel_), mask, tmpCorners_.ptr<float2>(), tmpCorners_.cols, counterPtr_, stream_);

        if (total == 0)
        {
            _corners.release();
            musaSafeCall( musaDestroyTextureObject(eigTex_) );
            return;
        }

        sortCorners_gpu(eigTex_, tmpCorners_.ptr<float2>(), total, stream_);

        musaSafeCall( musaDestroyTextureObject(eigTex_) );

        if (minDistance_ < 1)
        {
            tmpCorners_.colRange(0, maxCorners_ > 0 ? std::min(maxCorners_, total) : total).copyTo(_corners, stream);
        }
        else
        {
            std::vector<Point2f> tmp(total);
            Mat tmpMat(1, total, CV_32FC2, (void*)&tmp[0]);
            tmpCorners_.colRange(0, total).download(tmpMat, stream);
            stream.waitForCompletion();
            std::vector<Point2f> tmp2;
            tmp2.reserve(total);

            const int cell_size = cvRound(minDistance_);
            const int grid_width = (image.cols + cell_size - 1) / cell_size;
            const int grid_height = (image.rows + cell_size - 1) / cell_size;

            std::vector< std::vector<Point2f> > grid(grid_width * grid_height);

            for (int i = 0; i < total; ++i)
            {
                Point2f p = tmp[i];

                bool good = true;

                int x_cell = static_cast<int>(p.x / cell_size);
                int y_cell = static_cast<int>(p.y / cell_size);

                int x1 = x_cell - 1;
                int y1 = y_cell - 1;
                int x2 = x_cell + 1;
                int y2 = y_cell + 1;

                // boundary check
                x1 = std::max(0, x1);
                y1 = std::max(0, y1);
                x2 = std::min(grid_width - 1, x2);
                y2 = std::min(grid_height - 1, y2);

                for (int yy = y1; yy <= y2; yy++)
                {
                    for (int xx = x1; xx <= x2; xx++)
                    {
                        std::vector<Point2f>& m = grid[yy * grid_width + xx];

                        if (!m.empty())
                        {
                            for(size_t j = 0; j < m.size(); j++)
                            {
                                float dx = p.x - m[j].x;
                                float dy = p.y - m[j].y;

                                if (dx * dx + dy * dy < minDistance_ * minDistance_)
                                {
                                    good = false;
                                    goto break_out;
                                }
                            }
                        }
                    }
                }

                break_out:

                if(good)
                {
                    grid[y_cell * grid_width + x_cell].push_back(p);

                    tmp2.push_back(p);

                    if (maxCorners_ > 0 && tmp2.size() == static_cast<size_t>(maxCorners_))
                        break;
                }
            }

            _corners.create(1, static_cast<int>(tmp2.size()), CV_32FC2);
            GpuMat corners = _corners.getMUSAGpuMat();

            corners.upload(Mat(1, static_cast<int>(tmp2.size()), CV_32FC2, &tmp2[0]), stream);
        }
    }
}

Ptr<musa::CornersDetector> cv::musa::createGoodFeaturesToTrackDetector(int srcType, int maxCorners, double qualityLevel, double minDistance,
                                                                     int blockSize, bool useHarrisDetector, double harrisK)
{
    return Ptr<musa::CornersDetector>(
        new GoodFeaturesToTrackDetector(srcType, maxCorners, qualityLevel, minDistance, blockSize, useHarrisDetector, harrisK));
}

#endif /* !defined (HAVE_MUSA) */
