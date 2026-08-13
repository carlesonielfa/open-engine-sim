#ifndef OPEN_ENGINE_SIM_SPSC_AUDIO_RING_H
#define OPEN_ENGINE_SIM_SPSC_AUDIO_RING_H

#include <atomic>
#include <cstddef>
#include <memory>

// A bounded, preallocated single-producer/single-consumer queue for the
// simulation-to-device hand-off. The producer is the simulation thread; the
// consumer is the platform's real-time audio callback. Neither side waits.
template <typename T>
class SpscAudioRing {
public:
    void initialize(std::size_t capacity) {
        m_data = std::make_unique<T[]>(capacity + 1);
        m_capacity = capacity + 1;
        clear();
    }

    void destroy() {
        m_data.reset();
        m_capacity = 0;
        clear();
    }

    bool push(T value) {
        const std::size_t write = m_write.load(std::memory_order_relaxed);
        const std::size_t next = increment(write);
        if (next == m_read.load(std::memory_order_acquire)) return false;
        m_data[write] = value;
        m_write.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T &value) {
        const std::size_t read = m_read.load(std::memory_order_relaxed);
        if (read == m_write.load(std::memory_order_acquire)) return false;
        value = m_data[read];
        m_read.store(increment(read), std::memory_order_release);
        return true;
    }

    // Consumer-only fast-forward. This keeps an interactive output close to
    // the newest producer state without asking the producer to mutate read.
    std::size_t discard(std::size_t count) {
        const std::size_t read = m_read.load(std::memory_order_relaxed);
        const std::size_t write = m_write.load(std::memory_order_acquire);
        const std::size_t available = write >= read ? write - read : m_capacity - read + write;
        const std::size_t discarded = count < available ? count : available;
        m_read.store((read + discarded) % m_capacity, std::memory_order_release);
        return discarded;
    }

    std::size_t size() const {
        const std::size_t write = m_write.load(std::memory_order_acquire);
        const std::size_t read = m_read.load(std::memory_order_acquire);
        return write >= read ? write - read : m_capacity - read + write;
    }

    void clear() {
        m_read.store(0, std::memory_order_relaxed);
        m_write.store(0, std::memory_order_relaxed);
    }

private:
    std::size_t increment(std::size_t index) const {
        return index + 1 == m_capacity ? 0 : index + 1;
    }

    std::unique_ptr<T[]> m_data;
    std::size_t m_capacity = 0;
    std::atomic<std::size_t> m_read{0};
    std::atomic<std::size_t> m_write{0};
};

#endif
