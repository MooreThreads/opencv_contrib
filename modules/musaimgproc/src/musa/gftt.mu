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

#if !defined MUSA_DISABLER

#include <thrust/device_ptr.h>
#include <thrust/sort.h>

#include "opencv2/core/musa/common.hpp"
#include "opencv2/core/musa/utility.hpp"
#include <thrust/execution_policy.h>
namespace cv {
namespace musa {
namespace device {
namespace gfft {
template <class Mask>
__global__ void findCorners(float threshold, const Mask mask, float2 *corners,
                            int max_count, int rows, int cols,
                            musaTextureObject_t eigTex, int *g_counter) {
  const int j = blockIdx.x * blockDim.x + threadIdx.x;
  const int i = blockIdx.y * blockDim.y + threadIdx.y;

  if (i > 0 && i < rows - 1 && j > 0 && j < cols - 1 && mask(i, j)) {
    float val = tex2D<float>(eigTex, j, i);

    if (val > threshold) {
      float maxVal = val;

      maxVal = ::fmax(tex2D<float>(eigTex, j - 1, i - 1), maxVal);
      maxVal = ::fmax(tex2D<float>(eigTex, j, i - 1), maxVal);
      maxVal = ::fmax(tex2D<float>(eigTex, j + 1, i - 1), maxVal);

      maxVal = ::fmax(tex2D<float>(eigTex, j - 1, i), maxVal);
      maxVal = ::fmax(tex2D<float>(eigTex, j + 1, i), maxVal);

      maxVal = ::fmax(tex2D<float>(eigTex, j - 1, i + 1), maxVal);
      maxVal = ::fmax(tex2D<float>(eigTex, j, i + 1), maxVal);
      maxVal = ::fmax(tex2D<float>(eigTex, j + 1, i + 1), maxVal);

      if (val == maxVal) {
        const int ind = ::atomicAdd(g_counter, 1);

        if (ind < max_count) corners[ind] = make_float2(j, i);
      }
    }
  }
}

int findCorners_gpu(const musaTextureObject_t &eigTex, const int &rows,
                    const int &cols, float threshold, PtrStepSzb mask,
                    float2 *corners, int max_count, int *counterPtr,
                    musaStream_t stream) {
  musaSafeCall(musaMemsetAsync(counterPtr, 0, sizeof(int), stream));

  dim3 block(16, 16);
  dim3 grid(divUp(cols, block.x), divUp(rows, block.y));

  if (mask.data)
    findCorners<<<grid, block, 0, stream>>>(threshold, SingleMask(mask),
                                            corners, max_count, rows, cols,
                                            eigTex, counterPtr);
  else
    findCorners<<<grid, block, 0, stream>>>(threshold, WithOutMask(), corners,
                                            max_count, rows, cols, eigTex,
                                            counterPtr);

  musaSafeCall(musaGetLastError());

  int count;
  musaSafeCall(musaMemcpyAsync(&count, counterPtr, sizeof(int),
                               musaMemcpyDeviceToHost, stream));
  if (stream)
    musaSafeCall(musaStreamSynchronize(stream));
  else
    musaSafeCall(musaDeviceSynchronize());
  return std::min(count, max_count);
}

class EigGreater {
 public:
  EigGreater(const musaTextureObject_t &eigTex_) : eigTex(eigTex_) {}
  __device__ __forceinline__ bool operator()(float2 a, float2 b) const {
    return tex2D<float>(eigTex, a.x, a.y) > tex2D<float>(eigTex, b.x, b.y);
  }

  musaTextureObject_t eigTex;
};

void sortCorners_gpu(const musaTextureObject_t &eigTex, float2 *corners,
                     int count, musaStream_t stream) {
  thrust::device_ptr<float2> ptr(corners);
#if THRUST_VERSION >= 100802
  if (stream)
    thrust::sort(thrust::musa::par(ThrustAllocator::getAllocator()).on(stream),
                 ptr, ptr + count, EigGreater(eigTex));
  else
    thrust::sort(thrust::musa::par(ThrustAllocator::getAllocator()), ptr,
                 ptr + count, EigGreater(eigTex));
#else
  thrust::sort(ptr, ptr + count, EigGreater(eigTex));
#endif
}
}  // namespace gfft
}  // namespace device
}  // namespace musa
}  // namespace cv

#endif /* MUSA_DISABLER */
