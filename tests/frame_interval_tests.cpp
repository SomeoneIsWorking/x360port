#include "guarded_main.hpp"
#include "x360port/frame_intervals.hpp"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace
{

using namespace x360port;

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "frame_interval_tests: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

FrameIntervalHistogram::Buckets
Record(std::initializer_list<std::pair<std::uint32_t, std::uint64_t>> intervals)
{
    FrameIntervalHistogram::Buckets buckets{};
    for (auto [microseconds, count] : intervals)
    {
        std::size_t bucket = microseconds / FrameIntervalHistogram::kBucketMicroseconds;
        if (bucket >= FrameIntervalHistogram::kBucketCount)
        {
            bucket = FrameIntervalHistogram::kBucketCount - 1;
        }
        buckets[bucket] += count;
    }
    return buckets;
}

bool Is(std::optional<FrameIntervalQuantile> quantile, std::uint32_t microseconds, bool open_ended)
{
    return quantile && quantile->microseconds == microseconds && quantile->open_ended == open_ended;
}

void NothingRecordedHasNoQuantile()
{
    FrameIntervalHistogram empty;
    Require(empty.Count() == 0, "an empty histogram counted intervals");
    Require(!empty.Quantile(0.5), "an empty histogram reported a median");
}

void QuantilesNameTheBucketHoldingTheirRank()
{
    // 90 frames of 8.3 ms, 9 of 16.6 ms, and one 150 ms stall.
    FrameIntervalHistogram histogram(Record({{8300, 90}, {16600, 9}, {150000, 1}}));
    Require(histogram.Count() == 100, "the histogram lost intervals");
    Require(Is(histogram.Quantile(0.5), 8400, false), "the median was not the 8.3 ms bucket");
    Require(Is(histogram.Quantile(0.9), 8400, false), "the 90th percentile left the 8.3 ms bucket");
    Require(Is(histogram.Quantile(0.95), 16700, false),
            "the 95th percentile was not the 16.6 ms bucket");
    Require(Is(histogram.Quantile(0.99), 16700, false),
            "the 99th percentile was not the 16.6 ms bucket");
    Require(Is(histogram.Quantile(1.0), 100000, true),
            "the stall was not reported as at least 100 ms");
}

void FractionsOutsideTheUnitIntervalAreRefused()
{
    FrameIntervalHistogram histogram(Record({{8300, 10}}));
    Require(!histogram.Quantile(0.0), "a zero fraction was answered");
    Require(!histogram.Quantile(1.5), "a fraction above one was answered");
}

void SinceCountsOnlyTheWindow()
{
    FrameIntervalHistogram earlier(Record({{8300, 50}}));
    FrameIntervalHistogram later(Record({{8300, 50}, {33300, 10}}));
    FrameIntervalHistogram window = later.Since(earlier);
    Require(window.Count() == 10, "the window kept intervals recorded before it");
    Require(Is(window.Quantile(0.5), 33400, false), "the window's median was not its own");
}

[[nodiscard]] int RunTests()
{
    NothingRecordedHasNoQuantile();
    QuantilesNameTheBucketHoldingTheirRank();
    FractionsOutsideTheUnitIntervalAreRefused();
    SinceCountsOnlyTheWindow();
    std::cout << "frame_interval_tests: 4 cases passed\n";
    return EXIT_SUCCESS;
}

} // namespace

int main() { return x360port::tests::GuardedMain("frame_interval_tests", RunTests); }
