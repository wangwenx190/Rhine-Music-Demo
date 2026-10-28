// Keeps bounded diagnostic samples and computes their distributions. Measurements describe
// observed submissions and never imply unknown GPU completion times.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace yalahoo::rhi {
namespace diagnosticSampling {
// Quantiles describe only observed samples; batch time bounds GUI diagnostic publication.
inline constexpr auto MedianQuantile = .5;
inline constexpr auto HighQuantile = .95;
inline constexpr auto TailQuantile = .99;
inline constexpr auto BatchMilliseconds = 500;
}

// Fixed-size nearest-rank distributions. No allocations or JSON on the render hot path.
class DiagnosticSeries final
{
public:
    void add(double value)
    {
        if (!std::isfinite(value) || value < 0)
            return;
        m_values[m_seen++ % capacity] = value;
    }
    QVariantMap distribution() const
    {
        const auto count = std::min(m_seen, capacity);
        if (!count)
            return { };
        auto sorted = m_values;
        std::sort(sorted.begin(), sorted.begin() + count);
        const auto percentile = [&](double p) {
            return sorted[std::max(0, int(std::ceil(count * p)) - 1)];
        };
        return { { u"count"_s, count },
                 { u"discarded"_s, m_seen - count },
                 { u"p50"_s, percentile(diagnosticSampling::MedianQuantile) },
                 { u"p95"_s, percentile(diagnosticSampling::HighQuantile) },
                 { u"p99"_s, percentile(diagnosticSampling::TailQuantile) } };
    }
    int seen() const
    {
        return m_seen;
    }
    static constexpr int capacity = 256;

private:
    std::array<double, capacity> m_values{ };
    int m_seen = 0;
};
struct DiagnosticSample final
{
    double submittedMs = 0, cpuMs = 0, syncMs = 0, gpuMs = -1;
    qint64 atlasBytes = 0, selectedBytes = 0, uniformBytes = 0;
};
struct DiagnosticBatch final
{
    DiagnosticSeries intervals, cpu, sync, gpu;
    int submissions = 0;
    double elapsedMs = 0;
    qint64 atlasBytes = 0, selectedBytes = 0, uniformBytes = 0;
};
class DiagnosticSamples final
{
public:
    std::optional<DiagnosticBatch> add(const DiagnosticSample &sample)
    {
        if (!m_started) {
            m_started = true;
            m_start = sample.submittedMs;
        } else {
            m_batch.intervals.add(sample.submittedMs - m_previous);
        }
        m_previous = sample.submittedMs;
        m_batch.cpu.add(sample.cpuMs);
        m_batch.sync.add(sample.syncMs);
        m_batch.gpu.add(sample.gpuMs);
        ++m_batch.submissions;
        m_batch.atlasBytes += sample.atlasBytes;
        m_batch.selectedBytes += sample.selectedBytes;
        m_batch.uniformBytes += sample.uniformBytes;
        m_batch.elapsedMs = sample.submittedMs - m_start;
        if (m_batch.elapsedMs < diagnosticSampling::BatchMilliseconds)
            return std::nullopt;
        const auto result = m_batch;
        m_batch = { };
        m_start = sample.submittedMs;
        return result;
    }

private:
    DiagnosticBatch m_batch;
    double m_start = 0, m_previous = 0;
    bool m_started = false;
};
}
