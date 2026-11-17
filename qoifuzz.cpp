#include "qoi.h"
#include <fuzzer/FuzzedDataProvider.h>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

using Q = qoi::Qoi<
    qoi::DefaultQoiConfig,
    qoi::DefaultQoiConfig{qoi::ErrorHandling::TreatAsMonadic}
>;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzedDataProvider provider(data, size);

    // --- 1. Fuzz decode: feed raw fuzzer bytes directly ---
    {
        const auto [decoded, desc] = Q::decode({data, size}, 0);
        // If decode succeeded, verify output size matches description
        if (!decoded.empty()) {
            const size_t expected = static_cast<size_t>(desc.width) *
                                    desc.height * desc.channels;
            if (decoded.size() != expected) {
                __builtin_trap();
            }
        }
    }

    // --- 2. Fuzz encode: structured pixel data + description ---
    {
        const auto width    = provider.ConsumeIntegralInRange<unsigned int>(1, 64);
        const auto height   = provider.ConsumeIntegralInRange<unsigned int>(1, 64);
        const auto channels = provider.ConsumeIntegralInRange<unsigned char>(3, 4);
        const auto cs       = provider.ConsumeIntegralInRange<unsigned char>(0, 1);

        qoi::Description desc{width, height, channels, cs};

        const size_t px_len = static_cast<size_t>(width) * height * channels;
        auto pixels = provider.ConsumeBytes<uint8_t>(px_len);

        auto encoded = Q::encode(pixels, desc);
        if (encoded.empty()) {
            return 0; // valid rejection
        }

        // --- 3. Round-trip: decode the encoded output ---
        const auto [decoded, desc_out] = Q::decode(encoded, channels);
        if (decoded.empty()) {
            __builtin_trap(); // encode succeeded but decode failed
        }
        if (decoded.size() != px_len) {
            __builtin_trap();
        }
        // Verify pixel data matches
        for (size_t i = 0; i < px_len; ++i) {
            if (decoded[i] != pixels[i]) {
                __builtin_trap();
            }
        }
    }

    // --- 4. Fuzz decode with a specific channels hint (3 or 4) ---
    {
        const auto ch = provider.ConsumeIntegralInRange<uint8_t>(0, 4);
        const auto [decoded, desc] = Q::decode({data, size}, ch);
        if (!decoded.empty()) {
            const size_t expected = static_cast<size_t>(desc.width) *
                                    desc.height * desc.channels;
            if (decoded.size() != expected) {
                __builtin_trap();
            }
        }
    }

    return 0;
}
