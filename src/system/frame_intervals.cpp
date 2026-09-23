#include "x360port/frame_intervals.hpp"

#include <cmath>
#include <numeric>

namespace x360port
{

std::uint64_t FrameIntervalHistogram::Count() const noexcept
{
    return std::accumulate(buckets_.begin(), buckets_.end(), std::uint64_t{0});
}

std::optional<FrameIntervalQuantile>
FrameIntervalHistogram::Quantile(double fraction) const noexcept
{
    std::uint64_t count = Count();
    if (count == 0 || !(fraction > 0.0) || fraction > 1.0)
    {
        return std::nullopt;
    }
    // The rank of the quantile interval, counting from one.
    auto rank = static_cast<std::uint64_t>(std::ceil(fraction * static_cast<double>(count)));
    std::uint64_t seen = 0;
    for (std::size_t bucket = 0; bucket < kBucketCount; ++bucket)
    {
        seen += buckets_[bucket];
        if (seen >= rank)
        {
            bool last = bucket + 1 == kBucketCount;
            auto edge = static_cast<std::uint32_t>(last ? bucket : bucket + 1);
            return FrameIntervalQuantile{.microseconds = edge * kBucketMicroseconds,
                                         .open_ended = last};
        }
    }
    return std::nullopt;
}

FrameIntervalHistogram
FrameIntervalHistogram::Since(const FrameIntervalHistogram& earlier) const noexcept
{
    Buckets difference{};
    for (std::size_t bucket = 0; bucket < kBucketCount; ++bucket)
    {
        difference[bucket] = buckets_[bucket] - earlier.buckets_[bucket];
    }
    return FrameIntervalHistogram(difference);
}

} // namespace x360port
