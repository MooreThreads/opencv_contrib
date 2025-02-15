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

#if !defined HAVE_MUSA || defined(MUSA_DISABLER)

void cv::musa::warpAffine(InputArray, OutputArray, InputArray, Size, int, int, Scalar, Stream&) { throw_no_musa(); }
void cv::musa::buildWarpAffineMaps(InputArray, bool, Size, OutputArray, OutputArray, Stream&) { throw_no_musa(); }

void cv::musa::warpPerspective(InputArray, OutputArray, InputArray, Size, int, int, Scalar, Stream&) { throw_no_musa(); }
void cv::musa::buildWarpPerspectiveMaps(InputArray, bool, Size, OutputArray, OutputArray, Stream&) { throw_no_musa(); }

void cv::musa::rotate(InputArray, OutputArray, Size, double, double, double, int, Stream&) { throw_no_musa(); }

#else // HAVE_MUSA

namespace cv { namespace musa { namespace device
{
    namespace imgproc
    {
        void buildWarpAffineMaps_gpu(float coeffs[2 * 3], PtrStepSzf xmap, PtrStepSzf ymap, musaStream_t stream);

        template <typename T>
        void warpAffine_gpu(PtrStepSzb src, PtrStepSzb srcWhole, int xoff, int yoff, float coeffs[2 * 3], PtrStepSzb dst, int interpolation,
                            int borderMode, const float* borderValue, musaStream_t stream, bool cc20);

        void buildWarpPerspectiveMaps_gpu(float coeffs[3 * 3], PtrStepSzf xmap, PtrStepSzf ymap, musaStream_t stream);

        template <typename T>
        void warpPerspective_gpu(PtrStepSzb src, PtrStepSzb srcWhole, int xoff, int yoff, float coeffs[3 * 3], PtrStepSzb dst, int interpolation,
                            int borderMode, const float* borderValue, musaStream_t stream, bool cc20);
    }
}}}

void cv::musa::buildWarpAffineMaps(InputArray _M, bool inverse, Size dsize, OutputArray _xmap, OutputArray _ymap, Stream& stream)
{
    using namespace cv::musa::device::imgproc;

    Mat M = _M.getMat();

    CV_Assert( M.rows == 2 && M.cols == 3 );

    _xmap.create(dsize, CV_32FC1);
    _ymap.create(dsize, CV_32FC1);

    GpuMat xmap = _xmap.getMUSAGpuMat();
    GpuMat ymap = _ymap.getMUSAGpuMat();

    float coeffs[2 * 3];
    Mat coeffsMat(2, 3, CV_32F, (void*)coeffs);

    if (inverse)
        M.convertTo(coeffsMat, coeffsMat.type());
    else
    {
        cv::Mat iM;
        invertAffineTransform(M, iM);
        iM.convertTo(coeffsMat, coeffsMat.type());
    }

    buildWarpAffineMaps_gpu(coeffs, xmap, ymap, StreamAccessor::getStream(stream));
}

void cv::musa::buildWarpPerspectiveMaps(InputArray _M, bool inverse, Size dsize, OutputArray _xmap, OutputArray _ymap, Stream& stream)
{
    using namespace cv::musa::device::imgproc;

    Mat M = _M.getMat();

    CV_Assert( M.rows == 3 && M.cols == 3 );

    _xmap.create(dsize, CV_32FC1);
    _ymap.create(dsize, CV_32FC1);

    GpuMat xmap = _xmap.getMUSAGpuMat();
    GpuMat ymap = _ymap.getMUSAGpuMat();

    float coeffs[3 * 3];
    Mat coeffsMat(3, 3, CV_32F, (void*)coeffs);

    if (inverse)
        M.convertTo(coeffsMat, coeffsMat.type());
    else
    {
        cv::Mat iM;
        invert(M, iM);
        iM.convertTo(coeffsMat, coeffsMat.type());
    }

    buildWarpPerspectiveMaps_gpu(coeffs, xmap, ymap, StreamAccessor::getStream(stream));
}

namespace
{
    template <int DEPTH> struct MUppWarpFunc
    {
        typedef typename MUPPTypeTraits<DEPTH>::mupp_type mupp_type;

        typedef MUppStatus (*func_t)(const mupp_type* pSrc, MUppiSize srcSize, int srcStep, MUppiRect srcRoi, mupp_type* pDst,
                                    int dstStep, MUppiRect dstRoi, const double coeffs[][3],
                                    int interpolation);
    };

    template <int DEPTH, typename MUppWarpFunc<DEPTH>::func_t func> struct MUppWarp
    {
        typedef typename MUppWarpFunc<DEPTH>::mupp_type mupp_type;

        static void call(const cv::musa::GpuMat& src, cv::musa::GpuMat& dst, double coeffs[][3], int interpolation, musaStream_t stream)
        {
            static const int mupp_inter[] = {MUPPI_INTER_NN, MUPPI_INTER_LINEAR, MUPPI_INTER_CUBIC};

            MUppiSize srcsz;
            srcsz.height = src.rows;
            srcsz.width = src.cols;

            MUppiRect srcroi;
            srcroi.x = 0;
            srcroi.y = 0;
            srcroi.height = src.rows;
            srcroi.width = src.cols;

            MUppiRect dstroi;
            dstroi.x = 0;
            dstroi.y = 0;
            dstroi.height = dst.rows;
            dstroi.width = dst.cols;

            cv::musa::MUppStreamHandler h(stream);

            muppSafeCall( func(src.ptr<mupp_type>(), srcsz, static_cast<int>(src.step), srcroi,
                              dst.ptr<mupp_type>(), static_cast<int>(dst.step), dstroi,
                              coeffs, mupp_inter[interpolation]) );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }
    };
}

void cv::musa::warpAffine(InputArray _src, OutputArray _dst, InputArray _M, Size dsize, int flags, int borderMode, Scalar borderValue, Stream& stream)
{
    GpuMat src = _src.getMUSAGpuMat();
    Mat M = _M.getMat();

    CV_Assert( M.rows == 2 && M.cols == 3 );

    const int interpolation = flags & INTER_MAX;

    CV_Assert( src.depth() <= CV_32F && src.channels() <= 4 );
    CV_Assert( interpolation == INTER_NEAREST || interpolation == INTER_LINEAR || interpolation == INTER_CUBIC );
    CV_Assert( borderMode == BORDER_REFLECT101 || borderMode == BORDER_REPLICATE || borderMode == BORDER_CONSTANT || borderMode == BORDER_REFLECT || borderMode == BORDER_WRAP );

    _dst.create(dsize, src.type());
    GpuMat dst = _dst.getMUSAGpuMat();

    Size wholeSize;
    Point ofs;
    src.locateROI(wholeSize, ofs);

    static const bool useMUppTab[6][4][3] =
    {
        {
            {false, false, true},
            {false, false, false},
            {false, true, true},
            {false, false, false}
        },
        {
            {false, false, false},
            {false, false, false},
            {false, false, false},
            {false, false, false}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, false}
        },
        {
            {false, false, false},
            {false, false, false},
            {false, false, false},
            {false, false, false}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, true}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, true}
        }
    };

    bool useMUpp = borderMode == BORDER_CONSTANT && ofs.x == 0 && ofs.y == 0 && useMUppTab[src.depth()][src.channels() - 1][interpolation];
    // NPP bug on float data - MUPP tested OK
    // useMUpp = useMUpp && src.depth() != CV_32F;

    if (useMUpp)
    {
        typedef void (*func_t)(const cv::musa::GpuMat& src, cv::musa::GpuMat& dst, double coeffs[][3], int flags, musaStream_t stream);

        static const func_t funcs[2][6][4] =
        {
            {
                {MUppWarp<CV_8U, muppiWarpAffine_8u_C1R>::call, 0, MUppWarp<CV_8U, muppiWarpAffine_8u_C3R>::call, MUppWarp<CV_8U, muppiWarpAffine_8u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_16U, muppiWarpAffine_16u_C1R>::call, 0, MUppWarp<CV_16U, muppiWarpAffine_16u_C3R>::call, MUppWarp<CV_16U, muppiWarpAffine_16u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_32S, muppiWarpAffine_32s_C1R>::call, 0, MUppWarp<CV_32S, muppiWarpAffine_32s_C3R>::call, MUppWarp<CV_32S, muppiWarpAffine_32s_C4R>::call},
                {MUppWarp<CV_32F, muppiWarpAffine_32f_C1R>::call, 0, MUppWarp<CV_32F, muppiWarpAffine_32f_C3R>::call, MUppWarp<CV_32F, muppiWarpAffine_32f_C4R>::call}
            },
            {
                {MUppWarp<CV_8U, muppiWarpAffineBack_8u_C1R>::call, 0, MUppWarp<CV_8U, muppiWarpAffineBack_8u_C3R>::call, MUppWarp<CV_8U, muppiWarpAffineBack_8u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_16U, muppiWarpAffineBack_16u_C1R>::call, 0, MUppWarp<CV_16U, muppiWarpAffineBack_16u_C3R>::call, MUppWarp<CV_16U, muppiWarpAffineBack_16u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_32S, muppiWarpAffineBack_32s_C1R>::call, 0, MUppWarp<CV_32S, muppiWarpAffineBack_32s_C3R>::call, MUppWarp<CV_32S, muppiWarpAffineBack_32s_C4R>::call},
                {MUppWarp<CV_32F, muppiWarpAffineBack_32f_C1R>::call, 0, MUppWarp<CV_32F, muppiWarpAffineBack_32f_C3R>::call, MUppWarp<CV_32F, muppiWarpAffineBack_32f_C4R>::call}
            }
        };

        dst.setTo(borderValue, stream);

        double coeffs[2][3];
        Mat coeffsMat(2, 3, CV_64F, (void*)coeffs);
        M.convertTo(coeffsMat, coeffsMat.type());

        const func_t func = funcs[(flags & WARP_INVERSE_MAP) != 0][src.depth()][src.channels() - 1];
        CV_Assert(func != 0);

        func(src, dst, coeffs, interpolation, StreamAccessor::getStream(stream));
    }
    else
    {
        using namespace cv::musa::device::imgproc;

        typedef void (*func_t)(PtrStepSzb src, PtrStepSzb srcWhole, int xoff, int yoff, float coeffs[2 * 3], PtrStepSzb dst, int interpolation,
            int borderMode, const float* borderValue, musaStream_t stream, bool cc20);

        static const func_t funcs[6][4] =
        {
            {warpAffine_gpu<uchar>      , 0 /*warpAffine_gpu<uchar2>*/ , warpAffine_gpu<uchar3>     , warpAffine_gpu<uchar4>     },
            {0 /*warpAffine_gpu<schar>*/, 0 /*warpAffine_gpu<char2>*/  , 0 /*warpAffine_gpu<char3>*/, 0 /*warpAffine_gpu<char4>*/},
            {warpAffine_gpu<ushort>     , 0 /*warpAffine_gpu<ushort2>*/, warpAffine_gpu<ushort3>    , warpAffine_gpu<ushort4>    },
            {warpAffine_gpu<short>      , 0 /*warpAffine_gpu<short2>*/ , warpAffine_gpu<short3>     , warpAffine_gpu<short4>     },
            {0 /*warpAffine_gpu<int>*/  , 0 /*warpAffine_gpu<int2>*/   , 0 /*warpAffine_gpu<int3>*/ , 0 /*warpAffine_gpu<int4>*/ },
            {warpAffine_gpu<float>      , 0 /*warpAffine_gpu<float2>*/ , warpAffine_gpu<float3>     , warpAffine_gpu<float4>     }
        };

        const func_t func = funcs[src.depth()][src.channels() - 1];
        CV_Assert(func != 0);

        float coeffs[2 * 3];
        Mat coeffsMat(2, 3, CV_32F, (void*)coeffs);

        if (flags & WARP_INVERSE_MAP)
            M.convertTo(coeffsMat, coeffsMat.type());
        else
        {
            cv::Mat iM;
            invertAffineTransform(M, iM);
            iM.convertTo(coeffsMat, coeffsMat.type());
        }

        Scalar_<float> borderValueFloat;
        borderValueFloat = borderValue;

        func(src, PtrStepSzb(wholeSize.height, wholeSize.width, src.datastart, src.step), ofs.x, ofs.y, coeffs,
            dst, interpolation, borderMode, borderValueFloat.val, StreamAccessor::getStream(stream), true);
    }
}

void cv::musa::warpPerspective(InputArray _src, OutputArray _dst, InputArray _M, Size dsize, int flags, int borderMode, Scalar borderValue, Stream& stream)
{
    GpuMat src = _src.getMUSAGpuMat();
    Mat M = _M.getMat();

    CV_Assert( M.rows == 3 && M.cols == 3 );

    const int interpolation = flags & INTER_MAX;

    CV_Assert( src.depth() <= CV_32F && src.channels() <= 4 );
    CV_Assert( interpolation == INTER_NEAREST || interpolation == INTER_LINEAR || interpolation == INTER_CUBIC );
    CV_Assert( borderMode == BORDER_REFLECT101 || borderMode == BORDER_REPLICATE || borderMode == BORDER_CONSTANT || borderMode == BORDER_REFLECT || borderMode == BORDER_WRAP) ;

    _dst.create(dsize, src.type());
    GpuMat dst = _dst.getMUSAGpuMat();

    Size wholeSize;
    Point ofs;
    src.locateROI(wholeSize, ofs);

    static const bool useMUppTab[6][4][3] =
    {
        {
            {false, false, true},
            {false, false, false},
            {false, true, true},
            {false, false, false}
        },
        {
            {false, false, false},
            {false, false, false},
            {false, false, false},
            {false, false, false}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, false}
        },
        {
            {false, false, false},
            {false, false, false},
            {false, false, false},
            {false, false, false}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, true}
        },
        {
            {false, true, true},
            {false, false, false},
            {false, true, true},
            {false, false, true}
        }
    };

    bool useMUpp = borderMode == BORDER_CONSTANT && ofs.x == 0 && ofs.y == 0 && useMUppTab[src.depth()][src.channels() - 1][interpolation];
    // NPP bug on float data - MUPP tested OK
    // useMUpp = useMUpp && src.depth() != CV_32F;

    if (useMUpp)
    {
        typedef void (*func_t)(const cv::musa::GpuMat& src, cv::musa::GpuMat& dst, double coeffs[][3], int flags, musaStream_t stream);

        static const func_t funcs[2][6][4] =
        {
            {
                {MUppWarp<CV_8U, muppiWarpPerspective_8u_C1R>::call, 0, MUppWarp<CV_8U, muppiWarpPerspective_8u_C3R>::call, MUppWarp<CV_8U, muppiWarpPerspective_8u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_16U, muppiWarpPerspective_16u_C1R>::call, 0, MUppWarp<CV_16U, muppiWarpPerspective_16u_C3R>::call, MUppWarp<CV_16U, muppiWarpPerspective_16u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_32S, muppiWarpPerspective_32s_C1R>::call, 0, MUppWarp<CV_32S, muppiWarpPerspective_32s_C3R>::call, MUppWarp<CV_32S, muppiWarpPerspective_32s_C4R>::call},
                {MUppWarp<CV_32F, muppiWarpPerspective_32f_C1R>::call, 0, MUppWarp<CV_32F, muppiWarpPerspective_32f_C3R>::call, MUppWarp<CV_32F, muppiWarpPerspective_32f_C4R>::call}
            },
            {
                {MUppWarp<CV_8U, muppiWarpPerspectiveBack_8u_C1R>::call, 0, MUppWarp<CV_8U, muppiWarpPerspectiveBack_8u_C3R>::call, MUppWarp<CV_8U, muppiWarpPerspectiveBack_8u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_16U, muppiWarpPerspectiveBack_16u_C1R>::call, 0, MUppWarp<CV_16U, muppiWarpPerspectiveBack_16u_C3R>::call, MUppWarp<CV_16U, muppiWarpPerspectiveBack_16u_C4R>::call},
                {0, 0, 0, 0},
                {MUppWarp<CV_32S, muppiWarpPerspectiveBack_32s_C1R>::call, 0, MUppWarp<CV_32S, muppiWarpPerspectiveBack_32s_C3R>::call, MUppWarp<CV_32S, muppiWarpPerspectiveBack_32s_C4R>::call},
                {MUppWarp<CV_32F, muppiWarpPerspectiveBack_32f_C1R>::call, 0, MUppWarp<CV_32F, muppiWarpPerspectiveBack_32f_C3R>::call, MUppWarp<CV_32F, muppiWarpPerspectiveBack_32f_C4R>::call}
            }
        };

        dst.setTo(borderValue, stream);

        double coeffs[3][3];
        Mat coeffsMat(3, 3, CV_64F, (void*)coeffs);
        M.convertTo(coeffsMat, coeffsMat.type());

        const func_t func = funcs[(flags & WARP_INVERSE_MAP) != 0][src.depth()][src.channels() - 1];
        CV_Assert(func != 0);

        func(src, dst, coeffs, interpolation, StreamAccessor::getStream(stream));
    }
    else
    {
        using namespace cv::musa::device::imgproc;

        typedef void (*func_t)(PtrStepSzb src, PtrStepSzb srcWhole, int xoff, int yoff, float coeffs[2 * 3], PtrStepSzb dst, int interpolation,
            int borderMode, const float* borderValue, musaStream_t stream, bool cc20);

        static const func_t funcs[6][4] =
        {
            {warpPerspective_gpu<uchar>      , 0 /*warpPerspective_gpu<uchar2>*/ , warpPerspective_gpu<uchar3>     , warpPerspective_gpu<uchar4>     },
            {0 /*warpPerspective_gpu<schar>*/, 0 /*warpPerspective_gpu<char2>*/  , 0 /*warpPerspective_gpu<char3>*/, 0 /*warpPerspective_gpu<char4>*/},
            {warpPerspective_gpu<ushort>     , 0 /*warpPerspective_gpu<ushort2>*/, warpPerspective_gpu<ushort3>    , warpPerspective_gpu<ushort4>    },
            {warpPerspective_gpu<short>      , 0 /*warpPerspective_gpu<short2>*/ , warpPerspective_gpu<short3>     , warpPerspective_gpu<short4>     },
            {0 /*warpPerspective_gpu<int>*/  , 0 /*warpPerspective_gpu<int2>*/   , 0 /*warpPerspective_gpu<int3>*/ , 0 /*warpPerspective_gpu<int4>*/ },
            {warpPerspective_gpu<float>      , 0 /*warpPerspective_gpu<float2>*/ , warpPerspective_gpu<float3>     , warpPerspective_gpu<float4>     }
        };

        const func_t func = funcs[src.depth()][src.channels() - 1];
        CV_Assert(func != 0);

        float coeffs[3 * 3];
        Mat coeffsMat(3, 3, CV_32F, (void*)coeffs);

        if (flags & WARP_INVERSE_MAP)
            M.convertTo(coeffsMat, coeffsMat.type());
        else
        {
            cv::Mat iM;
            invert(M, iM);
            iM.convertTo(coeffsMat, coeffsMat.type());
        }

        Scalar_<float> borderValueFloat;
        borderValueFloat = borderValue;

        func(src, PtrStepSzb(wholeSize.height, wholeSize.width, src.datastart, src.step), ofs.x, ofs.y, coeffs,
            dst, interpolation, borderMode, borderValueFloat.val, StreamAccessor::getStream(stream), true);
    }
}

////////////////////////////////////////////////////////////////////////
// rotate

namespace
{
    template <int DEPTH> struct MUppRotateFunc
    {
        typedef typename MUPPTypeTraits<DEPTH>::mupp_type mupp_type;

        typedef MUppStatus (*func_t)(const mupp_type* pSrc, MUppiSize oSrcSize, int nSrcStep, MUppiRect oSrcROI,
                                    mupp_type* pDst, int nDstStep, MUppiRect oDstROI,
                                    double nAngle, double nShiftX, double nShiftY, int eInterpolation);
    };

    template <int DEPTH, typename MUppRotateFunc<DEPTH>::func_t func> struct MUppRotate
    {
        typedef typename MUppRotateFunc<DEPTH>::mupp_type mupp_type;

        static void call(const GpuMat& src, GpuMat& dst, Size dsize, double angle, double xShift, double yShift, int interpolation, musaStream_t stream)
        {
            CV_UNUSED(dsize);
            static const int mupp_inter[] = {MUPPI_INTER_NN, MUPPI_INTER_LINEAR, MUPPI_INTER_CUBIC};

            MUppStreamHandler h(stream);

            MUppiSize srcsz;
            srcsz.height = src.rows;
            srcsz.width = src.cols;
            MUppiRect srcroi;
            srcroi.x = srcroi.y = 0;
            srcroi.height = src.rows;
            srcroi.width = src.cols;
            MUppiRect dstroi;
            dstroi.x = dstroi.y = 0;
            dstroi.height = dst.rows;
            dstroi.width = dst.cols;

            muppSafeCall( func(src.ptr<mupp_type>(), srcsz, static_cast<int>(src.step), srcroi,
                dst.ptr<mupp_type>(), static_cast<int>(dst.step), dstroi, angle, xShift, yShift, mupp_inter[interpolation]) );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }
    };
}

void cv::musa::rotate(InputArray _src, OutputArray _dst, Size dsize, double angle, double xShift, double yShift, int interpolation, Stream& stream)
{
    typedef void (*func_t)(const GpuMat& src, GpuMat& dst, Size dsize, double angle, double xShift, double yShift, int interpolation, musaStream_t stream);
    static const func_t funcs[6][4] =
    {
        {MUppRotate<CV_8U, muppiRotate_8u_C1R>::call, 0, MUppRotate<CV_8U, muppiRotate_8u_C3R>::call, MUppRotate<CV_8U, muppiRotate_8u_C4R>::call},
        {0,0,0,0},
        {MUppRotate<CV_16U, muppiRotate_16u_C1R>::call, 0, MUppRotate<CV_16U, muppiRotate_16u_C3R>::call, MUppRotate<CV_16U, muppiRotate_16u_C4R>::call},
        {0,0,0,0},
        {0,0,0,0},
        {MUppRotate<CV_32F, muppiRotate_32f_C1R>::call, 0, MUppRotate<CV_32F, muppiRotate_32f_C3R>::call, MUppRotate<CV_32F, muppiRotate_32f_C4R>::call}
    };

    GpuMat src = _src.getMUSAGpuMat();

    CV_Assert( src.depth() == CV_8U || src.depth() == CV_16U || src.depth() == CV_32F );
    CV_Assert( src.channels() == 1 || src.channels() == 3 || src.channels() == 4 );
    CV_Assert( interpolation == INTER_NEAREST || interpolation == INTER_LINEAR || interpolation == INTER_CUBIC );

    _dst.create(dsize, src.type());
    GpuMat dst = _dst.getMUSAGpuMat();

    dst.setTo(Scalar::all(0), stream);

    funcs[src.depth()][src.channels() - 1](src, dst, dsize, angle, xShift, yShift, interpolation, StreamAccessor::getStream(stream));
}

#endif // HAVE_MUSA
