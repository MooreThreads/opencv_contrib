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

#include "opencv2/opencv_modules.hpp"

#ifndef HAVE_OPENCV_MUDEV

#error "opencv_mudev is required"

#else

#include "opencv2/musaarithm.hpp"
#include "opencv2/mudev.hpp"
#include "opencv2/core/private.musa.hpp"

using namespace cv;
using namespace cv::musa;
using namespace cv::mudev;

void cv::musa::transpose(InputArray _src, OutputArray _dst, Stream& stream)
{
    GpuMat src = getInputMat(_src, stream);

    const size_t elemSize = src.elemSize();

    CV_Assert( elemSize == 1 || elemSize == 4 || elemSize == 8 );

    GpuMat dst = getOutputMat(_dst, src.cols, src.rows, src.type(), stream);

    if (elemSize == 1)
    {
        MUppStreamHandler h(StreamAccessor::getStream(stream));

        MUppiSize sz;
        sz.width  = src.cols;
        sz.height = src.rows;

        muppSafeCall( muppiTranspose_8u_C1R(src.ptr<MUpp8u>(), static_cast<int>(src.step),
            dst.ptr<MUpp8u>(), static_cast<int>(dst.step), sz) );

        if (!stream)
            CV_MUDEV_SAFE_CALL( musaDeviceSynchronize() );
    }
    else if (elemSize == 4)
    {
        gridTranspose(globPtr<int>(src), globPtr<int>(dst), stream);
    }
    else // if (elemSize == 8)
    {
        gridTranspose(globPtr<double>(src), globPtr<double>(dst), stream);
    }

    syncOutput(dst, _dst, stream);
}

#endif
