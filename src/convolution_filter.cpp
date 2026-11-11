#include "../include/convolution_filter.h"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <string.h>

namespace {
constexpr int B = ConvolutionFilter::PartitionSize;
constexpr int M = ConvolutionFilter::FftSize;
constexpr int Bins = B + 1;
constexpr double Pi = 3.14159265358979323846;

// Eight independent accumulators let the compiler map this onto SSE/AVX/NEON
// without needing fast-math reassociation.
inline float dotProduct(const float *a, const float *b, int count) {
    float acc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int i = 0;
    for (; i + 8 <= count; i += 8) {
        for (int lane = 0; lane < 8; ++lane) {
            acc[lane] += a[i + lane] * b[i + lane];
        }
    }

    float result =
        ((acc[0] + acc[4]) + (acc[1] + acc[5]))
        + ((acc[2] + acc[6]) + (acc[3] + acc[7]));
    for (; i < count; ++i) result += a[i] * b[i];
    return result;
}
}

ConvolutionFilter::ConvolutionFilter() {
    m_impulseResponse = nullptr;
    m_sampleCount = 0;
    m_prepared = false;
    m_historyPosition = 0;
    m_directTapCount = 0;
    m_partitionCount = 0;
    m_delayLineHead = 0;
    m_blockFill = 0;
}

ConvolutionFilter::~ConvolutionFilter() {
    assert(m_impulseResponse == nullptr);
}

void ConvolutionFilter::initialize(int samples) {
    destroy();

    m_sampleCount = samples;
    m_impulseResponse = new float[samples];
    memset(m_impulseResponse, 0, sizeof(float) * samples);
    m_prepared = false;
}

void ConvolutionFilter::destroy() {
    delete[] m_impulseResponse;
    m_impulseResponse = nullptr;
    m_sampleCount = 0;
    m_prepared = false;

    m_directTaps = {};
    m_history = {};
    m_partitionRe = {};
    m_partitionIm = {};
    m_delayLineRe = {};
    m_delayLineIm = {};
    m_previousBlock = {};
    m_currentBlock = {};
    m_tail = {};
    m_twiddleRe = {};
    m_twiddleIm = {};
    m_bitReverse = {};
    m_fftRe = {};
    m_fftIm = {};
    m_accRe = {};
    m_accIm = {};

    m_historyPosition = 0;
    m_directTapCount = 0;
    m_partitionCount = 0;
    m_delayLineHead = 0;
    m_blockFill = 0;
}

void ConvolutionFilter::prepare() {
    m_prepared = true;
    if (m_impulseResponse == nullptr || m_sampleCount <= 0) {
        m_directTapCount = 0;
        m_partitionCount = 0;
        return;
    }

    const float *h = m_impulseResponse;

    // Direct section. The window handed to the dot product is chronological
    // (oldest first), so the taps are stored reversed.
    m_directTapCount = std::min(m_sampleCount, B);
    m_directTaps.assign(B, 0.0f);
    for (int k = 0; k < m_directTapCount; ++k) {
        m_directTaps[B - 1 - k] = h[k];
    }
    m_history.assign(2 * B, 0.0f);
    m_historyPosition = 0;

    // Partitioned section covers taps B .. N-1.
    const int tailTaps = m_sampleCount - B;
    m_partitionCount = tailTaps > 0 ? (tailTaps + B - 1) / B : 0;
    m_blockFill = 0;
    m_delayLineHead = 0;
    if (m_partitionCount == 0) return;

    m_twiddleRe.resize(M / 2);
    m_twiddleIm.resize(M / 2);
    for (int k = 0; k < M / 2; ++k) {
        const double angle = -2.0 * Pi * k / M;
        m_twiddleRe[k] = static_cast<float>(std::cos(angle));
        m_twiddleIm[k] = static_cast<float>(std::sin(angle));
    }

    m_bitReverse.resize(M);
    int bits = 0;
    while ((1 << bits) < M) ++bits;
    for (int i = 0; i < M; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) {
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        }
        m_bitReverse[i] = r;
    }

    m_fftRe.assign(M, 0.0f);
    m_fftIm.assign(M, 0.0f);
    m_accRe.assign(Bins, 0.0f);
    m_accIm.assign(Bins, 0.0f);

    m_partitionRe.assign(static_cast<size_t>(m_partitionCount) * Bins, 0.0f);
    m_partitionIm.assign(static_cast<size_t>(m_partitionCount) * Bins, 0.0f);
    for (int p = 1; p <= m_partitionCount; ++p) {
        std::fill(m_fftRe.begin(), m_fftRe.end(), 0.0f);
        std::fill(m_fftIm.begin(), m_fftIm.end(), 0.0f);

        const int first = p * B;
        const int count = std::min(B, m_sampleCount - first);
        for (int r = 0; r < count; ++r) m_fftRe[r] = h[first + r];

        fft(m_fftRe.data(), m_fftIm.data(), false);

        float *dstRe = &m_partitionRe[static_cast<size_t>(p - 1) * Bins];
        float *dstIm = &m_partitionIm[static_cast<size_t>(p - 1) * Bins];
        for (int k = 0; k < Bins; ++k) {
            dstRe[k] = m_fftRe[k];
            dstIm[k] = m_fftIm[k];
        }
    }

    m_delayLineRe.assign(static_cast<size_t>(m_partitionCount) * Bins, 0.0f);
    m_delayLineIm.assign(static_cast<size_t>(m_partitionCount) * Bins, 0.0f);
    m_previousBlock.assign(B, 0.0f);
    m_currentBlock.assign(B, 0.0f);
    m_tail.assign(B, 0.0f);
}

void ConvolutionFilter::reset() {
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    std::fill(m_delayLineRe.begin(), m_delayLineRe.end(), 0.0f);
    std::fill(m_delayLineIm.begin(), m_delayLineIm.end(), 0.0f);
    std::fill(m_previousBlock.begin(), m_previousBlock.end(), 0.0f);
    std::fill(m_currentBlock.begin(), m_currentBlock.end(), 0.0f);
    std::fill(m_tail.begin(), m_tail.end(), 0.0f);
    m_historyPosition = 0;
    m_delayLineHead = 0;
    m_blockFill = 0;
}

void ConvolutionFilter::fft(float *re, float *im, bool inverse) const {
    for (int i = 0; i < M; ++i) {
        const int j = m_bitReverse[i];
        if (j > i) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    const float sign = inverse ? -1.0f : 1.0f;
    for (int length = 2; length <= M; length <<= 1) {
        const int half = length / 2;
        const int step = M / length;
        for (int start = 0; start < M; start += length) {
            for (int k = 0; k < half; ++k) {
                const float wr = m_twiddleRe[k * step];
                const float wi = sign * m_twiddleIm[k * step];

                float *aRe = re + start + k;
                float *aIm = im + start + k;
                float *bRe = aRe + half;
                float *bIm = aIm + half;

                const float tr = *bRe * wr - *bIm * wi;
                const float ti = *bRe * wi + *bIm * wr;

                *bRe = *aRe - tr;
                *bIm = *aIm - ti;
                *aRe += tr;
                *aIm += ti;
            }
        }
    }
}

void ConvolutionFilter::processBlock() {
    const int K = m_partitionCount;

    // Overlap-save segment: previous block followed by the block just finished.
    for (int i = 0; i < B; ++i) {
        m_fftRe[i] = m_previousBlock[i];
        m_fftRe[B + i] = m_currentBlock[i];
    }
    std::fill(m_fftIm.begin(), m_fftIm.end(), 0.0f);
    fft(m_fftRe.data(), m_fftIm.data(), false);

    const int newest = m_delayLineHead;
    m_delayLineHead = (newest + 1 == K) ? 0 : newest + 1;
    {
        float *dstRe = &m_delayLineRe[static_cast<size_t>(newest) * Bins];
        float *dstIm = &m_delayLineIm[static_cast<size_t>(newest) * Bins];
        for (int k = 0; k < Bins; ++k) {
            dstRe[k] = m_fftRe[k];
            dstIm[k] = m_fftIm[k];
        }
    }

    float *__restrict accRe = m_accRe.data();
    float *__restrict accIm = m_accIm.data();
    std::fill(m_accRe.begin(), m_accRe.end(), 0.0f);
    std::fill(m_accIm.begin(), m_accIm.end(), 0.0f);

    // Partition p (1-based) pairs with the spectrum of the segment that ended
    // p-1 blocks before the newest one.
    int slot = newest;
    for (int p = 0; p < K; ++p) {
        const float *__restrict xr = &m_delayLineRe[static_cast<size_t>(slot) * Bins];
        const float *__restrict xi = &m_delayLineIm[static_cast<size_t>(slot) * Bins];
        const float *__restrict hr = &m_partitionRe[static_cast<size_t>(p) * Bins];
        const float *__restrict hi = &m_partitionIm[static_cast<size_t>(p) * Bins];

        for (int k = 0; k < Bins; ++k) {
            accRe[k] += xr[k] * hr[k] - xi[k] * hi[k];
            accIm[k] += xr[k] * hi[k] + xi[k] * hr[k];
        }

        slot = (slot == 0) ? K - 1 : slot - 1;
    }

    // Rebuild the conjugate-symmetric spectrum of a real signal.
    m_fftRe[0] = accRe[0];
    m_fftIm[0] = 0.0f;
    m_fftRe[B] = accRe[B];
    m_fftIm[B] = 0.0f;
    for (int k = 1; k < B; ++k) {
        m_fftRe[k] = accRe[k];
        m_fftIm[k] = accIm[k];
        m_fftRe[M - k] = accRe[k];
        m_fftIm[M - k] = -accIm[k];
    }
    fft(m_fftRe.data(), m_fftIm.data(), true);

    constexpr float scale = 1.0f / static_cast<float>(M);
    for (int j = 0; j < B; ++j) m_tail[j] = m_fftRe[B + j] * scale;

    m_previousBlock.swap(m_currentBlock);
    m_blockFill = 0;
}

float ConvolutionFilter::f(float sample) {
    if (!m_prepared) prepare();
    if (m_directTaps.empty()) return 0.0f;

    const int position = m_historyPosition;
    m_history[position] = sample;
    m_history[position + B] = sample;
    float result = dotProduct(m_directTaps.data(), &m_history[position + 1], B);
    m_historyPosition = (position + 1 == B) ? 0 : position + 1;

    if (m_partitionCount > 0) {
        result += m_tail[m_blockFill];
        m_currentBlock[m_blockFill] = sample;
        if (++m_blockFill == B) processBlock();
    }

    return result;
}
