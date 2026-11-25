/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "FFTFrame.h"

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

extern "C" {
#include <libavutil/mem.h>
#include <libavutil/tx.h>
}

namespace WebCore {

namespace {

constexpr int minFFTPow2Size = 2;
constexpr int maxFFTPow2Size = 24;

size_t unpackedFFTDataSize(unsigned fftSize)
{
    return fftSize / 2 + 1;
}

struct FFTTransform {
    AVTXContext* forward { nullptr };
    av_tx_fn forwardFn { nullptr };
    AVTXContext* inverse { nullptr };
    av_tx_fn inverseFn { nullptr };
    float* timeBuffer { nullptr };
    float* complexBuffer { nullptr };
};

FFTTransform* createTransform(unsigned fftSize)
{
    auto* transform = new FFTTransform;
    float forwardScale = 1.0f;
    float inverseScale = 1.0f / fftSize;

    if (av_tx_init(&transform->forward, &transform->forwardFn, AV_TX_FLOAT_RDFT, 0, fftSize, &forwardScale, 0) < 0) {
        transform->forward = nullptr;
    }
    if (av_tx_init(&transform->inverse, &transform->inverseFn, AV_TX_FLOAT_RDFT, 1, fftSize, &inverseScale, 0) < 0) {
        transform->inverse = nullptr;
    }

    transform->timeBuffer = static_cast<float*>(av_malloc(sizeof(float) * (fftSize + 2)));
    transform->complexBuffer = static_cast<float*>(av_malloc(sizeof(float) * 2 * (unpackedFFTDataSize(fftSize) + 1)));

    if (!transform->forward || !transform->inverse || !transform->timeBuffer || !transform->complexBuffer) {
        av_tx_uninit(&transform->forward);
        av_tx_uninit(&transform->inverse);
        av_freep(&transform->timeBuffer);
        av_freep(&transform->complexBuffer);
        delete transform;
        return nullptr;
    }

    return transform;
}

class FFTTransformPool {
public:
    static FFTTransformPool& singleton()
    {
        static NeverDestroyed<FFTTransformPool> pool;
        return pool;
    }

    FFTTransform* take(unsigned fftSize)
    {
        {
            Locker locker { m_lock };
            auto it = m_free.find(fftSize);
            if (it != m_free.end() && !it->value.isEmpty()) {
                return it->value.takeLast();
            }
        }
        return createTransform(fftSize);
    }

    void give(unsigned fftSize, FFTTransform* transform)
    {
        Locker locker { m_lock };
        m_free.ensure(fftSize, [] {
            return Vector<FFTTransform*> { };
        }).iterator->value.append(transform);
    }

private:
    Lock m_lock;
    HashMap<unsigned, Vector<FFTTransform*>> m_free WTF_GUARDED_BY_LOCK(m_lock);
};

class ScopedFFTTransform {
public:
    explicit ScopedFFTTransform(unsigned fftSize)
        : m_fftSize(fftSize)
        , m_transform(fftSize ? FFTTransformPool::singleton().take(fftSize) : nullptr)
    {
    }

    ~ScopedFFTTransform()
    {
        if (m_transform) {
            FFTTransformPool::singleton().give(m_fftSize, m_transform);
        }
    }

    FFTTransform* get() const { return m_transform; }

private:
    unsigned m_fftSize;
    FFTTransform* m_transform;
};

}

FFTFrame::FFTFrame(unsigned fftSize)
    : m_FFTSize(fftSize)
    , m_log2FFTSize(static_cast<unsigned>(log2(fftSize)))
    , m_realData(unpackedFFTDataSize(fftSize))
    , m_imagData(unpackedFFTDataSize(fftSize))
{
}

FFTFrame::FFTFrame()
    : m_FFTSize(0)
    , m_log2FFTSize(0)
{
}

FFTFrame::FFTFrame(const FFTFrame& frame)
    : m_FFTSize(frame.m_FFTSize)
    , m_log2FFTSize(frame.m_log2FFTSize)
    , m_realData(unpackedFFTDataSize(frame.m_FFTSize))
    , m_imagData(unpackedFFTDataSize(frame.m_FFTSize))
{
    m_realData.copyToRange(frame.m_realData.span(), 0, m_realData.size());
    m_imagData.copyToRange(frame.m_imagData.span(), 0, m_imagData.size());
}

FFTFrame::~FFTFrame() = default;

void FFTFrame::initialize()
{
}

void FFTFrame::doFFT(std::span<const float> data)
{
    ScopedFFTTransform scoped(m_FFTSize);
    auto* transform = scoped.get();
    if (!transform) {
        return;
    }

    memcpySpan(std::span { transform->timeBuffer, m_FFTSize }, data.first(m_FFTSize));
    transform->forwardFn(transform->forward, transform->complexBuffer, transform->timeBuffer, sizeof(float));

    auto realData = m_realData.span();
    auto imagData = m_imagData.span();
    for (size_t i = 0; i < unpackedFFTDataSize(m_FFTSize); ++i) {
        realData[i] = transform->complexBuffer[2 * i];
        imagData[i] = transform->complexBuffer[2 * i + 1];
    }
}

void FFTFrame::doInverseFFT(std::span<float> data)
{
    ScopedFFTTransform scoped(m_FFTSize);
    auto* transform = scoped.get();
    if (!transform) {
        return;
    }

    auto realData = m_realData.span();
    auto imagData = m_imagData.span();
    for (size_t i = 0; i < unpackedFFTDataSize(m_FFTSize); ++i) {
        transform->complexBuffer[2 * i] = realData[i];
        transform->complexBuffer[2 * i + 1] = imagData[i];
    }

    transform->inverseFn(transform->inverse, transform->timeBuffer, transform->complexBuffer, 2 * sizeof(float));
    memcpySpan(data.first(m_FFTSize), std::span<const float> { transform->timeBuffer, m_FFTSize });
}

int FFTFrame::minFFTSize()
{
    return 1 << minFFTPow2Size;
}

int FFTFrame::maxFFTSize()
{
    return 1 << maxFFTPow2Size;
}

}

#endif
