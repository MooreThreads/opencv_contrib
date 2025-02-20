namespace cv { namespace musa { namespace device
{
    namespace disp_bilateral_filter
    {
        template<typename T>
        void disp_bilateral_filter(PtrStepSz<T> disp, PtrStepSzb img, int channels, int iters, const float *, const float *, size_t, int radius, short edge_disc, short max_disc, musaStream_t stream);
    }
}}}
