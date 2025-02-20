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

#include "opencv2/mudev.hpp"
#include "opencv2/core/private.musa.hpp"

using namespace cv::mudev;

void bitScalar(const GpuMat& src, cv::Scalar value, bool, GpuMat& dst,
               const GpuMat& mask, double, Stream& stream, int op);

namespace {
template <template <typename> class Op, typename T>
void bitScalarOp(const GpuMat& src, uint value, GpuMat& dst, Stream& stream) {
  gridTransformUnary(globPtr<T>(src), globPtr<T>(dst), bind2nd(Op<T>(), value),
                     stream);
}

typedef void (*bit_scalar_func_t)(const GpuMat& src, uint value, GpuMat& dst,
                                  Stream& stream);

template <typename T, bit_scalar_func_t func>
struct BitScalar {
  static void call(const GpuMat& src, cv::Scalar value, GpuMat& dst,
                   Stream& stream) {
    func(src, cv::saturate_cast<T>(value[0]), dst, stream);
  }
};

template <bit_scalar_func_t func>
struct BitScalar4 {
  static void call(const GpuMat& src, cv::Scalar value, GpuMat& dst,
                   Stream& stream) {
    uint packedVal = 0;

    packedVal |= cv::saturate_cast<uchar>(value[0]);
    packedVal |= cv::saturate_cast<uchar>(value[1]) << 8;
    packedVal |= cv::saturate_cast<uchar>(value[2]) << 16;
    packedVal |= cv::saturate_cast<uchar>(value[3]) << 24;

    func(src, packedVal, dst, stream);
  }
};

template <int DEPTH, int cn>
struct MUppBitwiseCFunc {
  typedef typename MUPPTypeTraits<DEPTH>::mupp_type mupp_type;

  typedef MUppStatus (*func_t)(const mupp_type* pSrc1, int nSrc1Step,
                               const mupp_type* pConstants, mupp_type* pDst,
                               int nDstStep, MUppiSize oSizeROI);
};

template <int DEPTH, int cn, typename MUppBitwiseCFunc<DEPTH, cn>::func_t func>
struct MUppBitwiseC {
  typedef typename MUppBitwiseCFunc<DEPTH, cn>::mupp_type mupp_type;

  static void call(const GpuMat& src, cv::Scalar value, GpuMat& dst,
                   Stream& _stream) {
    musaStream_t stream = StreamAccessor::getStream(_stream);
    MUppStreamHandler h(stream);

    MUppiSize oSizeROI;
    oSizeROI.width = src.cols;
    oSizeROI.height = src.rows;

    const mupp_type pConstants[] = {cv::saturate_cast<mupp_type>(value[0]),
                                    cv::saturate_cast<mupp_type>(value[1]),
                                    cv::saturate_cast<mupp_type>(value[2]),
                                    cv::saturate_cast<mupp_type>(value[3])};

    muppSafeCall(func(src.ptr<mupp_type>(), static_cast<int>(src.step),
                      pConstants, dst.ptr<mupp_type>(),
                      static_cast<int>(dst.step), oSizeROI));

    if (stream == 0) CV_MUDEV_SAFE_CALL(musaDeviceSynchronize());
  }
};
}  // namespace

void bitScalar(const GpuMat& src, cv::Scalar value, bool, GpuMat& dst,
               const GpuMat& mask, double, Stream& stream, int op) {
  CV_UNUSED(mask);

  typedef void (*func_t)(const GpuMat& src, cv::Scalar value, GpuMat& dst,
                         Stream& stream);
  static const func_t funcs[3][6][4] = {
      {{BitScalar<uchar, bitScalarOp<bit_and, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiAndC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_and, uint> >::call},
       {BitScalar<uchar, bitScalarOp<bit_and, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiAndC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_and, uint> >::call},
       {BitScalar<ushort, bitScalarOp<bit_and, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiAndC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiAndC_16u_C4R>::call},
       {BitScalar<ushort, bitScalarOp<bit_and, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiAndC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiAndC_16u_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_and, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiAndC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiAndC_32s_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_and, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiAndC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiAndC_32s_C4R>::call}},
      {{BitScalar<uchar, bitScalarOp<bit_or, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiOrC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_or, uint> >::call},
       {BitScalar<uchar, bitScalarOp<bit_or, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiOrC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_or, uint> >::call},
       {BitScalar<ushort, bitScalarOp<bit_or, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiOrC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiOrC_16u_C4R>::call},
       {BitScalar<ushort, bitScalarOp<bit_or, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiOrC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiOrC_16u_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_or, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiOrC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiOrC_32s_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_or, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiOrC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiOrC_32s_C4R>::call}},
      {{BitScalar<uchar, bitScalarOp<bit_xor, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiXorC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_xor, uint> >::call},
       {BitScalar<uchar, bitScalarOp<bit_xor, uchar> >::call, 0,
        MUppBitwiseC<CV_8U, 3, muppiXorC_8u_C3R>::call,
        BitScalar4<bitScalarOp<bit_xor, uint> >::call},
       {BitScalar<ushort, bitScalarOp<bit_xor, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiXorC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiXorC_16u_C4R>::call},
       {BitScalar<ushort, bitScalarOp<bit_xor, ushort> >::call, 0,
        MUppBitwiseC<CV_16U, 3, muppiXorC_16u_C3R>::call,
        MUppBitwiseC<CV_16U, 4, muppiXorC_16u_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_xor, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiXorC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiXorC_32s_C4R>::call},
       {BitScalar<uint, bitScalarOp<bit_xor, uint> >::call, 0,
        MUppBitwiseC<CV_32S, 3, muppiXorC_32s_C3R>::call,
        MUppBitwiseC<CV_32S, 4, muppiXorC_32s_C4R>::call}}};

  const int depth = src.depth();
  const int cn = src.channels();

  CV_DbgAssert(depth <= CV_32F);
  CV_DbgAssert(cn == 1 || cn == 3 || cn == 4);
  CV_DbgAssert(mask.empty());
  CV_DbgAssert(op >= 0 && op < 3);

  funcs[op][depth][cn - 1](src, value, dst, stream);
}

#endif
