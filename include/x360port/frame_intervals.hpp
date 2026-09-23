#ifndef X360PORT_FRAME_INTERVALS_HPP
#define X360PORT_FRAME_INTERVALS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace x360port
{

// The quantile of a set of frame intervals, as the upper edge of the bucket
// holding it. An open-ended quantile fell in the last bucket, which holds every
// interval at least as long as its lower edge, so the value is a lower bound.
struct FrameIntervalQuantile
{
    std::uint32_t microseconds = 0;
    bool open_ended = false;
};

// Host time between consecutive guest swaps, counted in fixed-width buckets.
// A snapshot is a value: subtracting an earlier snapshot gives the intervals
// recorded in between, so a caller can report any window without resetting
// the running count.
class FrameIntervalHistogram final
{
  public:
    static constexpr std::uint32_t kBucketMicroseconds = 100;
    // The last bucket holds every interval of 100 ms or more.
    static constexpr std::size_t kBucketCount = 1001;
    using Buckets = std::array<std::uint64_t, kBucketCount>;

    FrameIntervalHistogram() = default;
    explicit FrameIntervalHistogram(const Buckets& buckets) noexcept : buckets_(buckets) {}

    [[nodiscard]] std::uint64_t Count() const noexcept;

    // The interval below which a `fraction` in (0, 1] of intervals fall.
    // Empty when no interval was recorded.
    [[nodiscard]] std::optional<FrameIntervalQuantile> Quantile(double fraction) const noexcept;

    // The intervals recorded after `earlier`, which must be an earlier
    // snapshot of the same running count.
    [[nodiscard]] FrameIntervalHistogram
    Since(const FrameIntervalHistogram& earlier) const noexcept;

  private:
    Buckets buckets_{};
};

} // namespace x360port

#endif
