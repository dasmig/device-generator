// Benchmarks for device-generator; see bench.hpp for the options.

#include "../dasmig/devicegen.hpp"
#include "bench.hpp"

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"device-generator", argc, argv};

    run.load("load", [&] {
        dasmig::dvg gen;
        return gen.load(run.tier());
    });

    dasmig::dvg gen;
    if (!gen.load(run.tier())) return run.missing_tier();

    run.calls("get_device", [&](std::uint64_t seed) { return gen.get_device({}, seed); });
    run.calls("get_device to_json", [&](std::uint64_t seed) {
        return gen.get_device({}, seed).to_json();
    });

    run.batches("get_device_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_device_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
