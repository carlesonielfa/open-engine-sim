#ifndef ATG_ENGINE_SIM_CONVOLUTION_FILTER_H
#define ATG_ENGINE_SIM_CONVOLUTION_FILTER_H

#include "filter.h"

#include <vector>

// Zero-latency streaming FIR convolution.
//
// The first `PartitionSize` taps are evaluated directly for every sample. The
// remaining taps are split into equal partitions and evaluated with a
// frequency-domain delay line (uniform partitioned overlap-save), one block at
// a time. The result is identical to a plain FIR filter up to float rounding,
// but the per-sample cost is a small fraction of an O(N) dot product, which is
// what keeps long exhaust impulse responses affordable on phones.
class ConvolutionFilter : public Filter {
    public:
        static constexpr int PartitionSize = 128;
        static constexpr int FftSize = PartitionSize * 2;

        ConvolutionFilter();
        virtual ~ConvolutionFilter();

        void initialize(int samples);
        // Must be called after the impulse response has been written. It is
        // also invoked lazily by the first call to f().
        void prepare();
        // Forget all input history; the impulse response is kept.
        void reset();
        virtual float f(float sample) override;
        void destroy() override;

        int getSampleCount() const { return m_sampleCount; }
        float *getImpulseResponse() { return m_impulseResponse; }

    protected:
        void processBlock();

        float *m_impulseResponse;
        int m_sampleCount;
        bool m_prepared;

        // Direct section: reversed first taps and a mirrored sample history so
        // the newest PartitionSize samples are always contiguous.
        std::vector<float> m_directTaps;
        std::vector<float> m_history;
        int m_historyPosition;
        int m_directTapCount;

        // Partitioned section.
        int m_partitionCount;
        std::vector<float> m_partitionRe;
        std::vector<float> m_partitionIm;
        std::vector<float> m_delayLineRe;
        std::vector<float> m_delayLineIm;
        int m_delayLineHead;

        std::vector<float> m_previousBlock;
        std::vector<float> m_currentBlock;
        std::vector<float> m_tail;
        int m_blockFill;

        // Radix-2 FFT work area.
        std::vector<float> m_twiddleRe;
        std::vector<float> m_twiddleIm;
        std::vector<int> m_bitReverse;
        std::vector<float> m_fftRe;
        std::vector<float> m_fftIm;
        std::vector<float> m_accRe;
        std::vector<float> m_accIm;

    private:
        void fft(float *re, float *im, bool inverse) const;
};

#endif /* ATG_ENGINE_SIM_CONVOLUTION_FILTER_H */
