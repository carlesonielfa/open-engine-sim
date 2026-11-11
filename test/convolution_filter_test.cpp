#include <gtest/gtest.h>

#include "../include/convolution_filter.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {
std::vector<double> referenceConvolution(
    const std::vector<float> &impulse, const std::vector<float> &input)
{
    std::vector<double> output(input.size(), 0.0);
    for (size_t n = 0; n < input.size(); ++n) {
        double sum = 0.0;
        const size_t taps = std::min(impulse.size(), n + 1);
        for (size_t k = 0; k < taps; ++k) {
            sum += static_cast<double>(impulse[k]) * input[n - k];
        }
        output[n] = sum;
    }
    return output;
}

void expectMatchesDirectFir(int taps, int samples) {
    std::mt19937 rng(1234 + taps);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<float> impulse(taps);
    for (int i = 0; i < taps; ++i) {
        // Decaying response, like a real exhaust measurement.
        impulse[i] = dist(rng) * std::exp(-4.0f * i / std::max(1, taps));
    }
    std::vector<float> input(samples);
    for (float &s : input) s = dist(rng) * 20000.0f;

    ConvolutionFilter filter;
    filter.initialize(taps);
    std::copy(impulse.begin(), impulse.end(), filter.getImpulseResponse());

    const std::vector<double> expected = referenceConvolution(impulse, input);
    double peak = 1.0;
    for (double v : expected) peak = std::max(peak, std::abs(v));

    double worst = 0.0;
    for (int n = 0; n < samples; ++n) {
        const double actual = filter.f(input[n]);
        worst = std::max(worst, std::abs(actual - expected[n]));
    }
    EXPECT_LT(worst / peak, 2e-5) << "taps=" << taps;

    filter.destroy();
}
}

TEST(ConvolutionFilterTest, MatchesDirectFirForShortResponses) {
    expectMatchesDirectFir(1, 600);
    expectMatchesDirectFir(37, 900);
    expectMatchesDirectFir(ConvolutionFilter::PartitionSize, 900);
}

TEST(ConvolutionFilterTest, MatchesDirectFirAcrossPartitionBoundaries) {
    expectMatchesDirectFir(ConvolutionFilter::PartitionSize + 1, 2000);
    expectMatchesDirectFir(ConvolutionFilter::PartitionSize * 2, 3000);
    expectMatchesDirectFir(ConvolutionFilter::PartitionSize * 3 - 5, 3000);
    expectMatchesDirectFir(1000, 6000);
}

TEST(ConvolutionFilterTest, MatchesDirectFirForLongExhaustResponse) {
    expectMatchesDirectFir(10000, 30000);
}

TEST(ConvolutionFilterTest, ImpulseReproducesTheResponseSampleForSample) {
    constexpr int taps = 700;
    ConvolutionFilter filter;
    filter.initialize(taps);
    for (int i = 0; i < taps; ++i) {
        filter.getImpulseResponse()[i] = static_cast<float>(i + 1);
    }

    for (int n = 0; n < taps + 300; ++n) {
        const float out = filter.f(n == 0 ? 1.0f : 0.0f);
        const float expected = n < taps ? static_cast<float>(n + 1) : 0.0f;
        EXPECT_NEAR(out, expected, 1e-2f) << "n=" << n;
    }
    filter.destroy();
}

TEST(ConvolutionFilterTest, RecoversAfterNonFiniteInput) {
    constexpr int taps = 1000;
    ConvolutionFilter filter;
    filter.initialize(taps);
    for (int i = 0; i < taps; ++i) filter.getImpulseResponse()[i] = 0.001f;

    filter.f(std::nanf(""));
    // A finite-memory FIR must flush the bad sample after `taps` samples.
    float out = 0.0f;
    for (int n = 0; n < taps * 3; ++n) out = filter.f(0.0f);
    EXPECT_TRUE(std::isfinite(out));
    filter.destroy();
}
