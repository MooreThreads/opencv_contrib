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

#include "opencv2/core/musa/common.hpp"
#include "opencv2/core/musa/emulation.hpp"

namespace cv {
namespace musa {
namespace device {
namespace hough {
template <int PIXELS_PER_THREAD>
__global__ void buildPointList(const PtrStepSzb src, unsigned int* list,
                               int* counterPtr) {
  __shared__ unsigned int s_queues[4][32 * PIXELS_PER_THREAD];
  __shared__ int s_qsize[4];
  __shared__ int s_globStart[4];

  const int x = blockIdx.x * blockDim.x * PIXELS_PER_THREAD + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;

  if (threadIdx.x == 0) s_qsize[threadIdx.y] = 0;
  __syncthreads();

  if (y < src.rows) {
    // fill the queue
    const uchar* srcRow = src.ptr(y);
    for (int i = 0, xx = x; i < PIXELS_PER_THREAD && xx < src.cols;
         ++i, xx += blockDim.x) {
      if (srcRow[xx]) {
        const unsigned int val = (y << 16) | xx;
        const int qidx = Emulation::smem::atomicAdd(&s_qsize[threadIdx.y], 1);
        s_queues[threadIdx.y][qidx] = val;
      }
    }
  }

  __syncthreads();

  // let one thread reserve the space required in the global list
  if (threadIdx.x == 0 && threadIdx.y == 0) {
    // find how many items are stored in each list
    int totalSize = 0;
    for (int i = 0; i < blockDim.y; ++i) {
      s_globStart[i] = totalSize;
      totalSize += s_qsize[i];
    }

    // calculate the offset in the global list
    const int globalOffset = atomicAdd(counterPtr, totalSize);
    for (int i = 0; i < blockDim.y; ++i) s_globStart[i] += globalOffset;
  }

  __syncthreads();

  // copy local queues to global queue
  const int qsize = s_qsize[threadIdx.y];
  int gidx = s_globStart[threadIdx.y] + threadIdx.x;
  for (int i = threadIdx.x; i < qsize; i += blockDim.x, gidx += blockDim.x)
    list[gidx] = s_queues[threadIdx.y][i];
}

int buildPointList_gpu(PtrStepSzb src, unsigned int* list, int* counterPtr,
                       musaStream_t stream) {
  const int PIXELS_PER_THREAD = 16;

  musaSafeCall(musaMemsetAsync(counterPtr, 0, sizeof(int), stream));

  const dim3 block(32, 4);
  const dim3 grid(divUp(src.cols, block.x * PIXELS_PER_THREAD),
                  divUp(src.rows, block.y));

  // L1 cache not configurable on mp_21
  // musaSafeCall(musaFuncSetCacheConfig(buildPointList<PIXELS_PER_THREAD>, musaFuncCachePreferShared));

  buildPointList<PIXELS_PER_THREAD>
      <<<grid, block, 0, stream>>>(src, list, counterPtr);
  musaSafeCall(musaGetLastError());

  int totalCount;
  musaSafeCall(musaMemcpyAsync(&totalCount, counterPtr, sizeof(int),
                               musaMemcpyDeviceToHost, stream));
  musaSafeCall(musaStreamSynchronize(stream));

  return totalCount;
}
}  // namespace hough
}  // namespace device
}  // namespace musa
}  // namespace cv

#endif /* MUSA_DISABLER */
