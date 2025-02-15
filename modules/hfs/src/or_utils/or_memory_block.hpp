// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#ifndef _OPENCV_OR_MEMORY_BLOCK_HPP_
#define _OPENCV_OR_MEMORY_BLOCK_HPP_

#ifdef _HFS_CUDA_ON_
#include "opencv2/core/cuda/common.hpp"
#endif

#ifdef _HFS_MUSA_ON_
#include "opencv2/core/musa/common.hpp"
#endif

#include "stddef.h"

namespace cv { namespace hfs { namespace orutils {

template <typename T>
class MemoryBlock
{
protected:
    T* data_cpu;

#ifdef _HFS_CUDA_ON_
    T* data_cuda;
#endif

#ifdef _HFS_MUSA_ON_
    T* data_musa;
#endif

public:

    size_t dataSize;
    inline T* getCpuData()
    {
        return data_cpu;
    }

    inline const T* getCpuData() const
    {
        return data_cpu;
    }

    MemoryBlock(size_t dataSize_)
    {
        Allocate(dataSize_);
        clear();
    }

    void clear(unsigned char defaultValue = 0)
    {
        memset(data_cpu, defaultValue, dataSize * sizeof(T));
#ifdef _HFS_CUDA_ON_
        cudaSafeCall(cudaMemset(data_cuda,
                defaultValue, dataSize * sizeof(T)));
#endif

#ifdef _HFS_MUSA_ON_
        musaSafeCall(musaMemset(data_musa,
                defaultValue, dataSize * sizeof(T)));
#endif
    }

#ifdef _HFS_CUDA_ON_
    enum MemoryCopyDirection
    {
        CPU_TO_CPU, CPU_TO_CUDA, CUDA_TO_CPU, CUDA_TO_CUDA
    };
    inline const T* getGpuData() const
    {
        return data_cuda;
    }

    inline T* getGpuData()
    {
        return data_cuda;
    }
    void updateDeviceFromHost()
    {
        cudaSafeCall(cudaMemcpy(data_cuda,
                data_cpu, dataSize * sizeof(T), cudaMemcpyHostToDevice));
    }
    void updateHostFromDevice()
    {
        cudaSafeCall(cudaMemcpy(data_cpu,
                data_cuda, dataSize * sizeof(T), cudaMemcpyDeviceToHost));
    }
    void setFrom(const MemoryBlock<T> *source,
        MemoryCopyDirection memoryCopyDirection)
    {
        switch (memoryCopyDirection)
        {
        case CPU_TO_CPU:
            memcpy(this->data_cpu, source->data_cpu,
                source->dataSize * sizeof(T));
            break;
        case CPU_TO_CUDA:
            cudaSafeCall(cudaMemcpyAsync(this->data_cuda, source->data_cpu,
                source->dataSize * sizeof(T), cudaMemcpyHostToDevice));
            break;
        case CUDA_TO_CPU:
            cudaSafeCall(cudaMemcpy(this->data_cpu, source->data_cuda,
                source->dataSize * sizeof(T), cudaMemcpyDeviceToHost));
            break;
        case CUDA_TO_CUDA:
            cudaSafeCall(cudaMemcpyAsync(this->data_cuda, source->data_cuda,
                source->dataSize * sizeof(T), cudaMemcpyDeviceToDevice));
            break;
        default: break;
        }
    }
#endif

#ifdef _HFS_MUSA_ON_
    enum MemoryCopyDirection
    {
        CPU_TO_CPU, CPU_TO_MUSA, MUSA_TO_CPU, MUSA_TO_MUSA
    };
    inline const T* getGpuData() const
    {
        return data_musa;
    }

    inline T* getGpuData()
    {
        return data_musa;
    }
    void updateDeviceFromHost()
    {
        musaSafeCall(musaMemcpy(data_musa,
                data_cpu, dataSize * sizeof(T), musaMemcpyHostToDevice));
    }
    void updateHostFromDevice()
    {
        musaSafeCall(musaMemcpy(data_cpu,
                data_musa, dataSize * sizeof(T), musaMemcpyDeviceToHost));
    }
    void setFrom(const MemoryBlock<T> *source,
        MemoryCopyDirection memoryCopyDirection)
    {
        switch (memoryCopyDirection)
        {
        case CPU_TO_CPU:
            memcpy(this->data_cpu, source->data_cpu,
                source->dataSize * sizeof(T));
            break;
        case CPU_TO_MUSA:
            musaSafeCall(musaMemcpyAsync(this->data_musa, source->data_cpu,
                source->dataSize * sizeof(T), musaMemcpyHostToDevice));
            break;
        case MUSA_TO_CPU:
            musaSafeCall(musaMemcpy(this->data_cpu, source->data_musa,
                source->dataSize * sizeof(T), musaMemcpyDeviceToHost));
            break;
        case MUSA_TO_MUSA:
            musaSafeCall(musaMemcpyAsync(this->data_musa, source->data_musa,
                source->dataSize * sizeof(T), musaMemcpyDeviceToDevice));
            break;
        default: break;
        }
    }
#endif

    virtual ~MemoryBlock() { this->Free(); }

    void Allocate(size_t dataSize_)
    {
        //Free();
        this->dataSize = dataSize_;
#if defined(_HFS_CUDA_ON_)
        cudaSafeCall(cudaMallocHost((void**)&data_cpu, dataSize_ * sizeof(T)));
        cudaSafeCall(cudaMalloc((void**)&data_cuda, dataSize_ * sizeof(T)));
#elif defined(_HFS_MUSA_ON_)
        musaSafeCall(musaMallocHost((void**)&data_cpu, dataSize_ * sizeof(T)));
        musaSafeCall(musaMalloc((void**)&data_musa, dataSize_ * sizeof(T)));
#else
        data_cpu = new T[dataSize_];
#endif
    }

    void Free() {
#if defined(_HFS_CUDA_ON_)
        cudaSafeCall(cudaFreeHost(data_cpu));
        cudaSafeCall(cudaFree(data_cuda));
#elif defined(_HFS_MUSA_ON_)
        musaSafeCall(musaFreeHost(data_cpu));
        musaSafeCall(musaFree(data_musa));
#else
        delete[] data_cpu;
#endif
    }

    MemoryBlock(const MemoryBlock&);
    MemoryBlock& operator=(const MemoryBlock&);
};


}}}


#endif
