// Threads/RingBuffer.h - Lock-free single-producer / single-consumer ring buffer
//   Producer: acquisition thread
//   Consumer: GUI render thread + HTTP thread
//
// Uses power-of-2 capacity and std::atomic for head/tail.
// DecodedWaveform is stored by value (move-enabled).
#pragma once
#include "pch.h"
#include "WaveformSample.h"

template<typename T, size_t Capacity>
class SPSCRingBuffer
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t kMask = Capacity - 1;

public:
    SPSCRingBuffer() : m_head(0), m_tail(0) {}

    // Producer: push (returns false if full)
    bool Push(T&& item)
    {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;
        if (next == m_tail.load(std::memory_order_acquire))
            return false;  // full
        m_buf[head] = std::move(item);
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool Push(const T& item)
    {
        T copy(item);
        return Push(std::move(copy));
    }

    // Consumer: pop (returns false if empty)
    bool Pop(T& out)
    {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return false;  // empty
        out = std::move(m_buf[tail]);
        m_tail.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    // Peek at latest item without removing (returns false if empty)
    bool PeekLatest(T& out) const
    {
        const size_t head = m_head.load(std::memory_order_acquire);
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (head == tail) return false;
        size_t latest = (head == 0) ? kMask : (head - 1);
        out = m_buf[latest];
        return true;
    }

    bool IsEmpty() const
    {
        return m_head.load(std::memory_order_acquire) ==
               m_tail.load(std::memory_order_acquire);
    }

    size_t Size() const
    {
        size_t h = m_head.load(std::memory_order_acquire);
        size_t t = m_tail.load(std::memory_order_acquire);
        return (h - t + Capacity) & kMask;
    }

private:
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
    std::array<T, Capacity> m_buf;
};

// Concrete ring buffer for decoded waveforms
// 8 slots: enough to decouple acquisition from rendering at any reasonable FPS
using WaveformRingBuffer = SPSCRingBuffer<DecodedWaveform, 8>;
