#ifndef OPEN_ENGINE_SIM_AUDIO_DSP_UTILS_H
#define OPEN_ENGINE_SIM_AUDIO_DSP_UTILS_H

#include <cmath>
#include <cstdint>

#if (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)) \
    && !defined(__EMSCRIPTEN__)
#include <xmmintrin.h>
#endif

namespace audio_dsp {

// Flush denormal floats to zero on the calling thread.
//
// Decaying filter tails (exhaust convolution, IIR state after a throttle lift)
// drift into the denormal range. Handling those in hardware or microcode is
// many times slower than normal arithmetic on common phone cores, so a quiet
// passage can suddenly make the audio thread miss its deadline and crackle.
// The floating-point control register is per thread, so every thread that
// does DSP or physics work calls this once.
inline void enableFlushToZero() {
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t fpcr = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ull << 24); // FZ
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)) \
    && !defined(__EMSCRIPTEN__)
    _mm_setcsr(_mm_getcsr() | 0x8040u); // FTZ | DAZ
#endif
}

// A non-finite or absurdly large sample would poison every recursive filter it
// touches, and an IIR never forgets a NaN, so the noise would never stop.
inline double sanitizeSample(double value, double limit) {
    if (!(value == value)) return 0.0;      // NaN
    if (value > limit) return limit;        // also catches +inf
    if (value < -limit) return -limit;
    return value;
}

} // namespace audio_dsp

#endif /* OPEN_ENGINE_SIM_AUDIO_DSP_UTILS_H */
