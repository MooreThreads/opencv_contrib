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
#include "opencv2/core/musa/utility.hpp"
#include "opencv2/core/musa/reduce.hpp"
#include "opencv2/core/musa/limits.hpp"
#include "opencv2/core/musa/vec_distance.hpp"
#include "opencv2/core/musa/datamov_utils.hpp"
#include "opencv2/core/musa/warp_shuffle.hpp"

namespace cv { namespace musa { namespace device
{
    namespace bf_knnmatch
    {
        ///////////////////////////////////////////////////////////////////////////////
        // Reduction

        template <int BLOCK_SIZE>
        __device__ void findBestMatch(float& bestDistance1, float& bestDistance2,
                                      int& bestTrainIdx1, int& bestTrainIdx2,
                                      float* s_distance, int* s_trainIdx)
        {
            CV_UNUSED(s_distance);
            CV_UNUSED(s_trainIdx);

            float d1, d2;
            int i1, i2;

            #pragma unroll
            for (int i = BLOCK_SIZE / 2; i >= 1; i /= 2)
            {
                d1 = shfl_down(bestDistance1, i, BLOCK_SIZE);
                d2 = shfl_down(bestDistance2, i, BLOCK_SIZE);
                i1 = shfl_down(bestTrainIdx1, i, BLOCK_SIZE);
                i2 = shfl_down(bestTrainIdx2, i, BLOCK_SIZE);

                if (bestDistance1 < d1)
                {
                    if (d1 < bestDistance2)
                    {
                        bestDistance2 = d1;
                        bestTrainIdx2 = i1;
                    }
                }
                else
                {
                    bestDistance2 = bestDistance1;
                    bestTrainIdx2 = bestTrainIdx1;

                    bestDistance1 = d1;
                    bestTrainIdx1 = i1;

                    if (d2 < bestDistance2)
                    {
                        bestDistance2 = d2;
                        bestTrainIdx2 = i2;
                    }
                }
            }
        }

        template <int BLOCK_SIZE>
        __device__ void findBestMatch(float& bestDistance1, float& bestDistance2,
                                       int& bestTrainIdx1, int& bestTrainIdx2,
                                       int& bestImgIdx1, int& bestImgIdx2,
                                       float* s_distance, int* s_trainIdx, int* s_imgIdx)
        {
            CV_UNUSED(s_distance);
            CV_UNUSED(s_trainIdx);
            CV_UNUSED(s_imgIdx);

            float d1, d2;
            int i1, i2;
            int j1, j2;

            #pragma unroll
            for (int i = BLOCK_SIZE / 2; i >= 1; i /= 2)
            {
                d1 = shfl_down(bestDistance1, i, BLOCK_SIZE);
                d2 = shfl_down(bestDistance2, i, BLOCK_SIZE);
                i1 = shfl_down(bestTrainIdx1, i, BLOCK_SIZE);
                i2 = shfl_down(bestTrainIdx2, i, BLOCK_SIZE);
                j1 = shfl_down(bestImgIdx1, i, BLOCK_SIZE);
                j2 = shfl_down(bestImgIdx2, i, BLOCK_SIZE);

                if (bestDistance1 < d1)
                {
                    if (d1 < bestDistance2)
                    {
                        bestDistance2 = d1;
                        bestTrainIdx2 = i1;
                        bestImgIdx2 = j1;
                    }
                }
                else
                {
                    bestDistance2 = bestDistance1;
                    bestTrainIdx2 = bestTrainIdx1;
                    bestImgIdx2 = bestImgIdx1;

                    bestDistance1 = d1;
                    bestTrainIdx1 = i1;
                    bestImgIdx1 = j1;

                    if (d2 < bestDistance2)
                    {
                        bestDistance2 = d2;
                        bestTrainIdx2 = i2;
                        bestImgIdx2 = j2;
                    }
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Match Unrolled Cached

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename T, typename U>
        __device__ void loadQueryToSmem(int queryIdx, const PtrStepSz<T>& query, U* s_query)
        {
            #pragma unroll
            for (int i = 0; i < MAX_DESC_LEN / BLOCK_SIZE; ++i)
            {
                const int loadX = threadIdx.x + i * BLOCK_SIZE;
                s_query[threadIdx.y * MAX_DESC_LEN + loadX] = loadX < query.cols ? query.ptr(::min(queryIdx, query.rows - 1))[loadX] : 0;
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __device__ void loopUnrolledCached(int queryIdx, const PtrStepSz<T>& query, int imgIdx, const PtrStepSz<T>& train, const Mask& mask,
                                           typename Dist::value_type* s_query, typename Dist::value_type* s_train,
                                           float& bestDistance1, float& bestDistance2,
                                           int& bestTrainIdx1, int& bestTrainIdx2,
                                           int& bestImgIdx1, int& bestImgIdx2)
        {
            for (int t = 0, endt = (train.rows + BLOCK_SIZE - 1) / BLOCK_SIZE; t < endt; ++t)
            {
                Dist dist;

                #pragma unroll
                for (int i = 0; i < MAX_DESC_LEN / BLOCK_SIZE; ++i)
                {
                    const int loadX = threadIdx.x + i * BLOCK_SIZE;

                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = 0;

                    if (loadX < train.cols)
                    {
                        T val;

                        ForceGlob<T>::Load(train.ptr(::min(t * BLOCK_SIZE + threadIdx.y, train.rows - 1)), loadX, val);
                        s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = val;
                    }

                    __syncthreads();

                    #pragma unroll
                    for (int j = 0; j < BLOCK_SIZE; ++j)
                        dist.reduceIter(s_query[threadIdx.y * MAX_DESC_LEN + i * BLOCK_SIZE + j], s_train[j * BLOCK_SIZE + threadIdx.x]);

                    __syncthreads();
                }

                typename Dist::result_type distVal = dist;

                const int trainIdx = t * BLOCK_SIZE + threadIdx.x;

                if (queryIdx < query.rows && trainIdx < train.rows && mask(queryIdx, trainIdx))
                {
                    if (distVal < bestDistance1)
                    {
                        bestImgIdx2   = bestImgIdx1;
                        bestDistance2 = bestDistance1;
                        bestTrainIdx2 = bestTrainIdx1;

                        bestImgIdx1   = imgIdx;
                        bestDistance1 = distVal;
                        bestTrainIdx1 = trainIdx;
                    }
                    else if (distVal < bestDistance2)
                    {
                        bestImgIdx2   = imgIdx;
                        bestDistance2 = distVal;
                        bestTrainIdx2 = trainIdx;
                    }
                }
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __global__ void matchUnrolledCached(const PtrStepSz<T> query, const PtrStepSz<T> train, const Mask mask, int2* bestTrainIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * MAX_DESC_LEN);

            loadQueryToSmem<BLOCK_SIZE, MAX_DESC_LEN>(queryIdx, query, s_query);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;

            loopUnrolledCached<BLOCK_SIZE, MAX_DESC_LEN, Dist>(queryIdx, query, 0, train, mask, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestTrainIdx1, myBestTrainIdx2);

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, s_distance, s_trainIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        void matchUnrolledCached(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask,
                                 const PtrStepSz<int2>& trainIdx, const PtrStepSz<float2>& distance,
                                 musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (BLOCK_SIZE * (MAX_DESC_LEN >= BLOCK_SIZE ? MAX_DESC_LEN : BLOCK_SIZE) + BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            matchUnrolledCached<BLOCK_SIZE, MAX_DESC_LEN, Dist><<<grid, block, smemSize, stream>>>(query, train, mask, trainIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __global__ void matchUnrolledCached(const PtrStepSz<T> query, const PtrStepSz<T>* trains, int n, const Mask mask, int2* bestTrainIdx, int2* bestImgIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * MAX_DESC_LEN);

            loadQueryToSmem<BLOCK_SIZE, MAX_DESC_LEN>(queryIdx, query, s_query);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;
            int myBestImgIdx1 = -1;
            int myBestImgIdx2 = -1;

            Mask m = mask;

            for (int imgIdx = 0; imgIdx < n; ++imgIdx)
            {
                const PtrStepSz<T> train = trains[imgIdx];
                m.next();
                loopUnrolledCached<BLOCK_SIZE, MAX_DESC_LEN, Dist>(queryIdx, query, imgIdx, train, m, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2);
            }

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);
            int* s_imgIdx = (int*)(smem + 2 * BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2, s_distance, s_trainIdx, s_imgIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestImgIdx[queryIdx] = make_int2(myBestImgIdx1, myBestImgIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        void matchUnrolledCached(const PtrStepSz<T>& query, const PtrStepSz<T>* trains, int n, const Mask& mask,
                                 const PtrStepSz<int2>& trainIdx, const PtrStepSz<int2>& imgIdx, const PtrStepSz<float2>& distance,
                                 musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (BLOCK_SIZE * (MAX_DESC_LEN >= 2 * BLOCK_SIZE ? MAX_DESC_LEN : 2 * BLOCK_SIZE) + BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            matchUnrolledCached<BLOCK_SIZE, MAX_DESC_LEN, Dist><<<grid, block, smemSize, stream>>>(query, trains, n, mask, trainIdx.data, imgIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Match Unrolled

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __device__ void loopUnrolled(int queryIdx, const PtrStepSz<T>& query, int imgIdx, const PtrStepSz<T>& train, const Mask& mask,
                                     typename Dist::value_type* s_query, typename Dist::value_type* s_train,
                                     float& bestDistance1, float& bestDistance2,
                                     int& bestTrainIdx1, int& bestTrainIdx2,
                                     int& bestImgIdx1, int& bestImgIdx2)
        {
            for (int t = 0, endt = (train.rows + BLOCK_SIZE - 1) / BLOCK_SIZE; t < endt; ++t)
            {
                Dist dist;

                #pragma unroll
                for (int i = 0; i < MAX_DESC_LEN / BLOCK_SIZE; ++i)
                {
                    const int loadX = threadIdx.x + i * BLOCK_SIZE;

                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = 0;
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = 0;

                    if (loadX < query.cols)
                    {
                        T val;

                        ForceGlob<T>::Load(query.ptr(::min(queryIdx, query.rows - 1)), loadX, val);
                        s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = val;

                        ForceGlob<T>::Load(train.ptr(::min(t * BLOCK_SIZE + threadIdx.y, train.rows - 1)), loadX, val);
                        s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = val;
                    }

                    __syncthreads();

                    #pragma unroll
                    for (int j = 0; j < BLOCK_SIZE; ++j)
                        dist.reduceIter(s_query[threadIdx.y * BLOCK_SIZE + j], s_train[j * BLOCK_SIZE + threadIdx.x]);

                    __syncthreads();
                }

                typename Dist::result_type distVal = dist;

                const int trainIdx = t * BLOCK_SIZE + threadIdx.x;

                if (queryIdx < query.rows && trainIdx < train.rows && mask(queryIdx, trainIdx))
                {
                    if (distVal < bestDistance1)
                    {
                        bestImgIdx2   = bestImgIdx1;
                        bestDistance2 = bestDistance1;
                        bestTrainIdx2 = bestTrainIdx1;

                        bestImgIdx1   = imgIdx;
                        bestDistance1 = distVal;
                        bestTrainIdx1 = trainIdx;
                    }
                    else if (distVal < bestDistance2)
                    {
                        bestImgIdx2   = imgIdx;
                        bestDistance2 = distVal;
                        bestTrainIdx2 = trainIdx;
                    }
                }
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __global__ void matchUnrolled(const PtrStepSz<T> query, const PtrStepSz<T> train, const Mask mask, int2* bestTrainIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;

            loopUnrolled<BLOCK_SIZE, MAX_DESC_LEN, Dist>(queryIdx, query, 0, train, mask, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestTrainIdx1, myBestTrainIdx2);

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, s_distance, s_trainIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        void matchUnrolled(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask,
                           const PtrStepSz<int2>& trainIdx, const PtrStepSz<float2>& distance,
                           musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (2 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            matchUnrolled<BLOCK_SIZE, MAX_DESC_LEN, Dist><<<grid, block, smemSize, stream>>>(query, train, mask, trainIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __global__ void matchUnrolled(const PtrStepSz<T> query, const PtrStepSz<T>* trains, int n, const Mask mask, int2* bestTrainIdx, int2* bestImgIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;
            int myBestImgIdx1 = -1;
            int myBestImgIdx2 = -1;

            Mask m = mask;

            for (int imgIdx = 0; imgIdx < n; ++imgIdx)
            {
                const PtrStepSz<T> train = trains[imgIdx];
                m.next();
                loopUnrolled<BLOCK_SIZE, MAX_DESC_LEN, Dist>(queryIdx, query, imgIdx, train, m, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2);
            }

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);
            int* s_imgIdx = (int*)(smem + 2 * BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2, s_distance, s_trainIdx, s_imgIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestImgIdx[queryIdx] = make_int2(myBestImgIdx1, myBestImgIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        void matchUnrolled(const PtrStepSz<T>& query, const PtrStepSz<T>* trains, int n, const Mask& mask,
                           const PtrStepSz<int2>& trainIdx, const PtrStepSz<int2>& imgIdx, const PtrStepSz<float2>& distance,
                           musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (3 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            matchUnrolled<BLOCK_SIZE, MAX_DESC_LEN, Dist><<<grid, block, smemSize, stream>>>(query, trains, n, mask, trainIdx.data, imgIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Match

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        __device__ void loop(int queryIdx, const PtrStepSz<T>& query, int imgIdx, const PtrStepSz<T>& train, const Mask& mask,
                             typename Dist::value_type* s_query, typename Dist::value_type* s_train,
                             float& bestDistance1, float& bestDistance2,
                             int& bestTrainIdx1, int& bestTrainIdx2,
                             int& bestImgIdx1, int& bestImgIdx2)
        {
            for (int t = 0, endt = (train.rows + BLOCK_SIZE - 1) / BLOCK_SIZE; t < endt; ++t)
            {
                Dist dist;

                for (int i = 0, endi = (query.cols + BLOCK_SIZE - 1) / BLOCK_SIZE; i < endi; ++i)
                {
                    const int loadX = threadIdx.x + i * BLOCK_SIZE;

                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = 0;
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = 0;

                    if (loadX < query.cols)
                    {
                        T val;

                        ForceGlob<T>::Load(query.ptr(::min(queryIdx, query.rows - 1)), loadX, val);
                        s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = val;

                        ForceGlob<T>::Load(train.ptr(::min(t * BLOCK_SIZE + threadIdx.y, train.rows - 1)), loadX, val);
                        s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = val;
                    }

                    __syncthreads();

                    #pragma unroll
                    for (int j = 0; j < BLOCK_SIZE; ++j)
                        dist.reduceIter(s_query[threadIdx.y * BLOCK_SIZE + j], s_train[j * BLOCK_SIZE + threadIdx.x]);

                    __syncthreads();
                }

                typename Dist::result_type distVal = dist;

                const int trainIdx = t * BLOCK_SIZE + threadIdx.x;

                if (queryIdx < query.rows && trainIdx < train.rows && mask(queryIdx, trainIdx))
                {
                    if (distVal < bestDistance1)
                    {
                        bestImgIdx2   = bestImgIdx1;
                        bestDistance2 = bestDistance1;
                        bestTrainIdx2 = bestTrainIdx1;

                        bestImgIdx1   = imgIdx;
                        bestDistance1 = distVal;
                        bestTrainIdx1 = trainIdx;
                    }
                    else if (distVal < bestDistance2)
                    {
                        bestImgIdx2   = imgIdx;
                        bestDistance2 = distVal;
                        bestTrainIdx2 = trainIdx;
                    }
                }
            }
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        __global__ void match(const PtrStepSz<T> query, const PtrStepSz<T> train, const Mask mask, int2* bestTrainIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;

            loop<BLOCK_SIZE, Dist>(queryIdx, query, 0, train, mask, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestTrainIdx1, myBestTrainIdx2);

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, s_distance, s_trainIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        void match(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask,
                   const PtrStepSz<int2>& trainIdx, const PtrStepSz<float2>& distance,
                   musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (2 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            match<BLOCK_SIZE, Dist><<<grid, block, smemSize, stream>>>(query, train, mask, trainIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        __global__ void match(const PtrStepSz<T> query, const PtrStepSz<T>* trains, int n, const Mask mask, int2* bestTrainIdx, int2* bestImgIdx, float2* bestDistance)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.x * BLOCK_SIZE + threadIdx.y;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            float myBestDistance1 = numeric_limits<float>::max();
            float myBestDistance2 = numeric_limits<float>::max();
            int myBestTrainIdx1 = -1;
            int myBestTrainIdx2 = -1;
            int myBestImgIdx1 = -1;
            int myBestImgIdx2 = -1;

            Mask m = mask;

            for (int imgIdx = 0; imgIdx < n; ++imgIdx)
            {
                const PtrStepSz<T> train = trains[imgIdx];
                m.next();
                loop<BLOCK_SIZE, Dist>(queryIdx, query, imgIdx, train, m, s_query, s_train, myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2);
            }

            __syncthreads();

            float* s_distance = (float*)(smem);
            int* s_trainIdx = (int*)(smem + BLOCK_SIZE * BLOCK_SIZE);
            int* s_imgIdx = (int*)(smem + 2 * BLOCK_SIZE * BLOCK_SIZE);

            findBestMatch<BLOCK_SIZE>(myBestDistance1, myBestDistance2, myBestTrainIdx1, myBestTrainIdx2, myBestImgIdx1, myBestImgIdx2, s_distance, s_trainIdx, s_imgIdx);

            if (queryIdx < query.rows && threadIdx.x == 0)
            {
                bestTrainIdx[queryIdx] = make_int2(myBestTrainIdx1, myBestTrainIdx2);
                bestImgIdx[queryIdx] = make_int2(myBestImgIdx1, myBestImgIdx2);
                bestDistance[queryIdx] = make_float2(myBestDistance1, myBestDistance2);
            }
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        void match(const PtrStepSz<T>& query, const PtrStepSz<T>* trains, int n, const Mask& mask,
                   const PtrStepSz<int2>& trainIdx, const PtrStepSz<int2>& imgIdx, const PtrStepSz<float2>& distance,
                   musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (3 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            match<BLOCK_SIZE, Dist><<<grid, block, smemSize, stream>>>(query, trains, n, mask, trainIdx.data, imgIdx.data, distance.data);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        ///////////////////////////////////////////////////////////////////////////////
        // knnMatch 2 dispatcher

        template <typename Dist, typename T, typename Mask>
        void match2Dispatcher(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask,
                              const PtrStepSzb& trainIdx, const PtrStepSzb& distance,
                              musaStream_t stream)
        {
            if (query.cols <= 64)
            {
                matchUnrolledCached<16, 64, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 128)
            {
                matchUnrolledCached<16, 128, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            /*else if (query.cols <= 256)
            {
                matchUnrolled<16, 256, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 512)
            {
                matchUnrolled<16, 512, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 1024)
            {
                matchUnrolled<16, 1024, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }*/
            else
            {
                match<16, Dist>(query, train, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
        }

        template <typename Dist, typename T, typename Mask>
        void match2Dispatcher(const PtrStepSz<T>& query, const PtrStepSz<T>* trains, int n, const Mask& mask,
                              const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance,
                              musaStream_t stream)
        {
            if (query.cols <= 64)
            {
                matchUnrolledCached<16, 64, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 128)
            {
                matchUnrolledCached<16, 128, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            /*else if (query.cols <= 256)
            {
                matchUnrolled<16, 256, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 512)
            {
                matchUnrolled<16, 512, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
            else if (query.cols <= 1024)
            {
                matchUnrolled<16, 1024, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }*/
            else
            {
                match<16, Dist>(query, trains, n, mask, static_cast< PtrStepSz<int2> >(trainIdx), static_cast< PtrStepSz<int2> >(imgIdx), static_cast< PtrStepSz<float2> > (distance), stream);
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Calc distance kernel

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        __global__ void calcDistanceUnrolled(const PtrStepSz<T> query, const PtrStepSz<T> train, const Mask mask, PtrStepf allDist)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.y * BLOCK_SIZE + threadIdx.y;
            const int trainIdx = blockIdx.x * BLOCK_SIZE + threadIdx.x;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            Dist dist;

            #pragma unroll
            for (int i = 0; i < MAX_DESC_LEN / BLOCK_SIZE; ++i)
            {
                const int loadX = threadIdx.x + i * BLOCK_SIZE;

                if (loadX < query.cols)
                {
                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = query.ptr(::min(queryIdx, query.rows - 1))[loadX];
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = train.ptr(::min(blockIdx.x * BLOCK_SIZE + threadIdx.y, train.rows - 1))[loadX];
                }
                else
                {
                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = 0;
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = 0;
                }

                __syncthreads();

                #pragma unroll
                for (int j = 0; j < BLOCK_SIZE; ++j)
                    dist.reduceIter(s_query[threadIdx.y * BLOCK_SIZE + j], s_train[j * BLOCK_SIZE + threadIdx.x]);

                __syncthreads();
            }

            if (queryIdx < query.rows && trainIdx < train.rows)
            {
                float distVal = numeric_limits<float>::max();

                if (mask(queryIdx, trainIdx))
                    distVal = (typename Dist::result_type)dist;

                allDist.ptr(queryIdx)[trainIdx] = distVal;
            }
        }

        template <int BLOCK_SIZE, int MAX_DESC_LEN, typename Dist, typename T, typename Mask>
        void calcDistanceUnrolled(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask, const PtrStepSzf& allDist, musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(train.rows, BLOCK_SIZE), divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (2 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            calcDistanceUnrolled<BLOCK_SIZE, MAX_DESC_LEN, Dist><<<grid, block, smemSize, stream>>>(query, train, mask, allDist);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        __global__ void calcDistance(const PtrStepSz<T> query, const PtrStepSz<T> train, const Mask mask, PtrStepf allDist)
        {
            extern __shared__ int smem[];

            const int queryIdx = blockIdx.y * BLOCK_SIZE + threadIdx.y;
            const int trainIdx = blockIdx.x * BLOCK_SIZE + threadIdx.x;

            typename Dist::value_type* s_query = (typename Dist::value_type*)(smem);
            typename Dist::value_type* s_train = (typename Dist::value_type*)(smem + BLOCK_SIZE * BLOCK_SIZE);

            Dist dist;

            for (int i = 0, endi = (query.cols + BLOCK_SIZE - 1) / BLOCK_SIZE; i < endi; ++i)
            {
                const int loadX = threadIdx.x + i * BLOCK_SIZE;

                if (loadX < query.cols)
                {
                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = query.ptr(::min(queryIdx, query.rows - 1))[loadX];
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = train.ptr(::min(blockIdx.x * BLOCK_SIZE + threadIdx.y, train.rows - 1))[loadX];
                }
                else
                {
                    s_query[threadIdx.y * BLOCK_SIZE + threadIdx.x] = 0;
                    s_train[threadIdx.x * BLOCK_SIZE + threadIdx.y] = 0;
                }

                __syncthreads();

                #pragma unroll
                for (int j = 0; j < BLOCK_SIZE; ++j)
                    dist.reduceIter(s_query[threadIdx.y * BLOCK_SIZE + j], s_train[j * BLOCK_SIZE + threadIdx.x]);

                __syncthreads();
            }

            if (queryIdx < query.rows && trainIdx < train.rows)
            {
                float distVal = numeric_limits<float>::max();

                if (mask(queryIdx, trainIdx))
                    distVal = (typename Dist::result_type)dist;

                allDist.ptr(queryIdx)[trainIdx] = distVal;
            }
        }

        template <int BLOCK_SIZE, typename Dist, typename T, typename Mask>
        void calcDistance(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask, const PtrStepSzf& allDist, musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, BLOCK_SIZE);
            const dim3 grid(divUp(train.rows, BLOCK_SIZE), divUp(query.rows, BLOCK_SIZE));

            const size_t smemSize = (2 * BLOCK_SIZE * BLOCK_SIZE) * sizeof(int);

            calcDistance<BLOCK_SIZE, Dist><<<grid, block, smemSize, stream>>>(query, train, mask, allDist);
            musaSafeCall( musaGetLastError() );

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Calc Distance dispatcher

        template <typename Dist, typename T, typename Mask>
        void calcDistanceDispatcher(const PtrStepSz<T>& query, const PtrStepSz<T>& train, const Mask& mask,
                                    const PtrStepSzf& allDist,
                                    musaStream_t stream)
        {
            if (query.cols <= 64)
            {
                calcDistanceUnrolled<16, 64, Dist>(query, train, mask, allDist, stream);
            }
            else if (query.cols <= 128)
            {
                calcDistanceUnrolled<16, 128, Dist>(query, train, mask, allDist, stream);
            }
            /*else if (query.cols <= 256)
            {
                calcDistanceUnrolled<16, 256, Dist>(query, train, mask, allDist, stream);
            }
            else if (query.cols <= 512)
            {
                calcDistanceUnrolled<16, 512, Dist>(query, train, mask, allDist, stream);
            }
            else if (query.cols <= 1024)
            {
                calcDistanceUnrolled<16, 1024, Dist>(query, train, mask, allDist, stream);
            }*/
            else
            {
                calcDistance<16, Dist>(query, train, mask, allDist, stream);
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // find knn match kernel

        template <int BLOCK_SIZE>
        __global__ void findBestMatch(PtrStepSzf allDist, int i, PtrStepi trainIdx, PtrStepf distance)
        {
            const int SMEM_SIZE = BLOCK_SIZE > 64 ? BLOCK_SIZE : 64;
            __shared__ float s_dist[SMEM_SIZE];
            __shared__ int s_trainIdx[SMEM_SIZE];

            const int queryIdx = blockIdx.x;

            float* allDistRow = allDist.ptr(queryIdx);

            float dist = numeric_limits<float>::max();
            int bestIdx = -1;

            for (int i = threadIdx.x; i < allDist.cols; i += BLOCK_SIZE)
            {
                float reg = allDistRow[i];
                if (reg < dist)
                {
                    dist = reg;
                    bestIdx = i;
                }
            }

            s_dist[threadIdx.x] = dist;
            s_trainIdx[threadIdx.x] = bestIdx;
            __syncthreads();

            reduceKeyVal<BLOCK_SIZE>(s_dist, dist, s_trainIdx, bestIdx, threadIdx.x, less<float>());

            if (threadIdx.x == 0)
            {
                if (dist < numeric_limits<float>::max())
                {
                    allDistRow[bestIdx] = numeric_limits<float>::max();
                    trainIdx.ptr(queryIdx)[i] = bestIdx;
                    distance.ptr(queryIdx)[i] = dist;
                }
            }
        }

        template <int BLOCK_SIZE>
        void findKnnMatch(int k, const PtrStepSzi& trainIdx, const PtrStepSzf& distance, const PtrStepSzf& allDist, musaStream_t stream)
        {
            const dim3 block(BLOCK_SIZE, 1, 1);
            const dim3 grid(trainIdx.rows, 1, 1);

            for (int i = 0; i < k; ++i)
            {
                findBestMatch<BLOCK_SIZE><<<grid, block, 0, stream>>>(allDist, i, trainIdx, distance);
                musaSafeCall( musaGetLastError() );
            }

            if (stream == 0)
                musaSafeCall( musaDeviceSynchronize() );
        }

        void findKnnMatchDispatcher(int k, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream)
        {
            findKnnMatch<256>(k, static_cast<PtrStepSzi>(trainIdx), static_cast<PtrStepSzf>(distance), allDist, stream);
        }

        ///////////////////////////////////////////////////////////////////////////////
        // knn match Dispatcher

        template <typename Dist, typename T, typename Mask>
        void matchDispatcher(const PtrStepSz<T>& query, const PtrStepSz<T>& train, int k, const Mask& mask,
            const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist,
            musaStream_t stream)
        {
            calcDistanceDispatcher<Dist>(query, train, mask, allDist, stream);
            findKnnMatchDispatcher(k, trainIdx, distance, allDist, stream);
        }

        ///////////////////////////////////////////////////////////////////////////////
        // knn match caller

        template <typename T> void matchL1_gpu(const PtrStepSzb& query, const PtrStepSzb& train, int k, const PtrStepSzb& mask,
            const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist,
            musaStream_t stream)
        {
            if (mask.data)
                matchDispatcher< L1Dist<T> >(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, SingleMask(mask), trainIdx, distance, allDist, stream);
            else
                matchDispatcher< L1Dist<T> >(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, WithOutMask(), trainIdx, distance, allDist, stream);
        }

        template void matchL1_gpu<uchar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchL1_gpu<schar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchL1_gpu<ushort>(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchL1_gpu<short >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchL1_gpu<int   >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchL1_gpu<float >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);

        template <typename T> void matchL2_gpu(const PtrStepSzb& query, const PtrStepSzb& train, int k, const PtrStepSzb& mask,
            const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist,
            musaStream_t stream)
        {
            if (mask.data)
                matchDispatcher<L2Dist>(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, SingleMask(mask), trainIdx, distance, allDist, stream);
            else
                matchDispatcher<L2Dist>(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, WithOutMask(), trainIdx, distance, allDist, stream);
        }

        //template void matchL2_gpu<uchar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchL2_gpu<schar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchL2_gpu<ushort>(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchL2_gpu<short >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchL2_gpu<int   >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchL2_gpu<float >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);

        template <typename T> void matchHamming_gpu(const PtrStepSzb& query, const PtrStepSzb& train, int k, const PtrStepSzb& mask,
            const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist,
            musaStream_t stream)
        {
            if (mask.data)
                matchDispatcher<HammingDist>(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, SingleMask(mask), trainIdx, distance, allDist, stream);
            else
                matchDispatcher<HammingDist>(static_cast< PtrStepSz<T> >(query), static_cast< PtrStepSz<T> >(train), k, WithOutMask(), trainIdx, distance, allDist, stream);
        }

        template void matchHamming_gpu<uchar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchHamming_gpu<schar >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchHamming_gpu<ushort>(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        //template void matchHamming_gpu<short >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);
        template void matchHamming_gpu<int   >(const PtrStepSzb& queryDescs, const PtrStepSzb& trainDescs, int k, const PtrStepSzb& mask, const PtrStepSzb& trainIdx, const PtrStepSzb& distance, const PtrStepSzf& allDist, musaStream_t stream);

        template <typename T> void match2L1_gpu(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks,
            const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance,
            musaStream_t stream)
        {
            if (masks.data)
                match2Dispatcher< L1Dist<T> >(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, MaskCollection(masks.data), trainIdx, imgIdx, distance, stream);
            else
                match2Dispatcher< L1Dist<T> >(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, WithOutMask(), trainIdx, imgIdx, distance,  stream);
        }

        template void match2L1_gpu<uchar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2L1_gpu<schar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2L1_gpu<ushort>(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2L1_gpu<short >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2L1_gpu<int   >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2L1_gpu<float >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);

        template <typename T> void match2L2_gpu(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks,
            const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance,
            musaStream_t stream)
        {
            if (masks.data)
                match2Dispatcher<L2Dist>(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, MaskCollection(masks.data), trainIdx, imgIdx, distance, stream);
            else
                match2Dispatcher<L2Dist>(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, WithOutMask(), trainIdx, imgIdx, distance, stream);
        }

        //template void match2L2_gpu<uchar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2L2_gpu<schar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2L2_gpu<ushort>(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2L2_gpu<short >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2L2_gpu<int   >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzi& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2L2_gpu<float >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);

        template <typename T> void match2Hamming_gpu(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks,
            const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance,
            musaStream_t stream)
        {
            if (masks.data)
                match2Dispatcher<HammingDist>(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, MaskCollection(masks.data), trainIdx, imgIdx, distance, stream);
            else
                match2Dispatcher<HammingDist>(static_cast< PtrStepSz<T> >(query), (const PtrStepSz<T>*)trains.ptr(), trains.cols, WithOutMask(), trainIdx, imgIdx, distance, stream);
        }

        template void match2Hamming_gpu<uchar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2Hamming_gpu<schar >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2Hamming_gpu<ushort>(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        //template void match2Hamming_gpu<short >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
        template void match2Hamming_gpu<int   >(const PtrStepSzb& query, const PtrStepSzb& trains, const PtrStepSz<PtrStepb>& masks, const PtrStepSzb& trainIdx, const PtrStepSzb& imgIdx, const PtrStepSzb& distance, musaStream_t stream);
    } // namespace bf_knnmatch
}}} // namespace cv { namespace musa { namespace cudev {


#endif /* MUSA_DISABLER */
