#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>
#include <array>
#include <bit>
#include <algorithm>
#include <span>
#include <version>

#ifndef QOI_HAS_EXCEPTIONS
// Standard
#   if defined(__cpp_exceptions) && __cpp_exceptions
#       define QOI_HAS_EXCEPTIONS 1
// gcc
#   elif defined(__EXCEPTIONS) && __EXCEPTIONS
#       define QOI_HAS_EXCEPTIONS 1
// msvc
#   elif defined(_HAS_EXCEPTIONS) && _HAS_EXCEPTIONS
#       define QOI_HAS_EXCEPTIONS 1
// unknown or doesn't have any
#   else
#       define QOI_HAS_EXCEPTIONS 0
#   endif
#endif

#if QOI_HAS_EXCEPTIONS
#   define QOI_IF_HAS_EXCEPTIONS(...) __VA_ARGS__
#   include <stdexcept>
#else
#   define QOI_IF_HAS_EXCEPTIONS(...)
#endif

#ifndef QOI_HAS_EXPECTED
#   if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#       define QOI_HAS_EXPECTED 1
#   else
#       define QOI_HAS_EXPECTED 0
#   endif
#endif

#if QOI_HAS_EXPECTED
#   define QOI_IF_HAS_EXPECTED(...) __VA_ARGS__
#   include <expected>
#else
#   define QOI_IF_HAS_EXPECTED(...)
#endif


#if !defined(QOI_HAS_OPTIONAL)
#   if __cpp_lib_optional >= 201606L
#       define QOI_HAS_OPTIONAL 1
#   else
#       define QOI_HAS_OPTIONAL 0
#   endif
#endif

#if QOI_HAS_OPTIONAL
#   define QOI_IF_HAS_OPTIONAL(...) __VA_ARGS__
#   include <optional>
#else
#   define QOI_IF_HAS_OPTIONAL(...)
#endif

#if defined(QOI_CUSTOM_TYPE)
#   define QOI_IF_CUSTOM_TYPE(...) __VA_ARGS__
#else
#   define QOI_IF_CUSTOM_TYPE(...)
#endif

namespace qoi
{
    /* A qoi_desc struct has to be supplied to all of qoi's functions.
It describes either the input format (for qoi_write and qoi_encode), or is
filled with the description read from the file header (for qoi_read and
qoi_decode).

The colorspace in this qoi_desc is an enum where
    0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
    1 = all channels are linear
You may use the constants QOI_SRGB or QOI_LINEAR. The colorspace is purely
informative. It will be saved to the file header, but does not affect
how chunks are en-/decoded. */

    enum struct ColorSpace {
        QOI_SRGB = 0,
        QOI_LINEAR = 1
    };

    struct Description {
        unsigned int width;
        unsigned int height;
        unsigned char channels;
        unsigned char colorspace;
        constexpr auto operator<=>(const Description&) const noexcept = default;
    };

    constexpr uint8_t QOI_OP_INDEX = 0x00; /* 00xxxxxx */
    constexpr uint8_t QOI_OP_DIFF  = 0x40; /* 01xxxxxx */
    constexpr uint8_t QOI_OP_LUMA  = 0x80; /* 10xxxxxx */
    constexpr uint8_t QOI_OP_RUN   = 0xc0; /* 11xxxxxx */
    constexpr uint8_t QOI_OP_RGB   = 0xfe; /* 11111110 */
    constexpr uint8_t QOI_OP_RGBA  = 0xff; /* 11111111 */
    constexpr uint8_t QOI_MASK_2   = 0xc0; /* 11000000 */

    constexpr unsigned int QOI_MAGIC = []() -> unsigned int {
        const auto ui = [](const char c) -> unsigned int { return static_cast<unsigned int>(c); };
        return
            ui('q') << 24 |
            ui('o') << 16 |
            ui('i') <<  8 |
            ui('f');
    }();
    constexpr size_t QOI_HEADER_SIZE = 14;

    /* 2GB is the max file size that this implementation can safely handle. We guard
    against anything larger than that, assuming the worst case with 5 bytes per
    pixel, rounded down to a nice clean value. 400 million pixels ought to be
    enough for anybody. */
    constexpr size_t QOI_PIXELS_MAX = 400000000;

    struct qoi_rgba_t {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
        constexpr auto operator<=>(const qoi_rgba_t&) const noexcept = default;
    };

    constexpr int64_t QOI_COLOR_HASH(const qoi_rgba_t& color) {
        return color.r * 3 + color.g * 5 + color.b * 7 + color.a * 11;
    }

    constexpr unsigned char qoi_padding[8] = {0,0,0,0,0,0,0,1};

    template<typename T>
    concept ConstByteBuffer = requires(T& t)
    {
        { t[0] } -> std::convertible_to<uint8_t>;
    };
    template<typename T>
    concept ByteBuffer = ConstByteBuffer<T> && requires(T& t)
    {
        { t[0] = 0 } -> std::convertible_to<uint8_t>;
    };

    constexpr void qoi_write_32(ByteBuffer auto& bytes, size_t& p, const uint32_t v) {
        bytes[p++] = (0xff000000 & v) >> 24;
        bytes[p++] = (0x00ff0000 & v) >> 16;
        bytes[p++] = (0x0000ff00 & v) >> 8;
        bytes[p++] = (0x000000ff & v);
    }

    constexpr uint32_t qoi_read_32(const ConstByteBuffer auto& bytes, size_t& p) {
        const uint8_t b0 = bytes[p++];
        const uint8_t b1 = bytes[p++];
        const uint8_t b2 = bytes[p++];
        const uint8_t b3 = bytes[p++];
        const auto val = b0 << 24 | b1 << 16 | b2 << 8 | b3;
        return std::bit_cast<uint32_t>(val);
    }

enum struct ErrorHandling {
    QOI_IF_HAS_EXCEPTIONS(Exceptions,)
    QOI_IF_HAS_EXPECTED(Expected,)
    QOI_IF_HAS_OPTIONAL(Optional,)
    QOI_IF_CUSTOM_TYPE(CustomType,)
    TreatAsMonadic // e.g. returning a 0 length vector
};

QOI_IF_HAS_EXCEPTIONS(struct QoiException : std::runtime_error {}; )

struct DefaultQoiConfig {
    using vector_type = std::vector<uint8_t>;
    ErrorHandling error_handling;
};

template<typename Config, Config config_t = DefaultQoiConfig{}>
class Qoi {
    template<typename Type, bool error_t = false>
    static constexpr auto MakeResultType(auto... args) {
        constexpr auto errorHandling_t = config_t.error_handling;
        if constexpr (errorHandling_t == ErrorHandling::TreatAsMonadic) {
            if constexpr (error_t) { return Type{}; } else return Type{args...};
        }
        QOI_IF_HAS_EXCEPTIONS(if constexpr (errorHandling_t == ErrorHandling::Exceptions) {
            if constexpr (error_t) { throw QoiException(args...); } else
            return Type(args...);
        })
        QOI_IF_HAS_OPTIONAL(if constexpr (errorHandling_t == ErrorHandling::Optional) {
            if constexpr (error_t) { return std::nullopt; } else
            return std::optional<Type>(args...);
        })
        QOI_IF_HAS_EXPECTED(if constexpr (errorHandling_t == ErrorHandling::Expected) {
            using E = std::expected<Type, std::string_view>;
            if constexpr (error_t) { return E(std::unexpect_t{}, std::string_view{args...}); } else
            return E(args...);
        })
        QOI_IF_CUSTOM_TYPE(if constexpr (errorHandling_t == ErrorHandling::CustomType) {
            if constexpr (error_t) { return QOI_CUSTOM_TYPE::Error(args..); } else
            return QOI_CUSTOM_TYPE(args...);
        })
    }
    // using vector_type = typename ;
    using Result_t = decltype(MakeResultType<typename Config::vector_type, false>());
public:
    /* Encode raw RGB or RGBA pixels into a QOI image in memory.
       - Error handling is dependent on class template parameter
       - If custom allocation is required user can provide any vector type
    */
    constexpr static Result_t encode(const std::span<const uint8_t> data, const Description& desc) {
        constexpr auto MakeError = [](auto sv) {
            return MakeResultType<typename Config::vector_type, true>(sv);
        };

        std::array<qoi_rgba_t, 64> index{};
        qoi_rgba_t px_prev{};

        if (
            data.size() <= 3 ||
            desc.width == 0 || desc.height == 0 ||
            desc.channels < 3 || desc.channels > 4 ||
            desc.colorspace > 1 ||
            desc.height >= QOI_PIXELS_MAX / desc.width
        ) {
            return MakeError("Invalid input, qoi has 3|4 channels and a reasonable amount of pixels");
        }

        const auto expected_size = desc.width * desc.height * desc.channels;
        if (data.size() != expected_size)
        {
            return MakeError("Invalid input, buffer did not contain enough pixels");
        }

        const size_t max_size = desc.width * desc.height * (desc.channels + 1) +
                    QOI_HEADER_SIZE + sizeof(qoi_padding);

        size_t p = 0;
        typename Config::vector_type result {};
        result.resize(max_size);

        qoi_write_32(result, p, QOI_MAGIC);
        qoi_write_32(result, p, desc.width);
        qoi_write_32(result, p, desc.height);
        result[p++] = desc.channels;
        result[p++] = desc.colorspace;

        const auto& pixels = data;

        uint8_t run = 0;
        px_prev.r = 0;
        px_prev.g = 0;
        px_prev.b = 0;
        px_prev.a = 255;
        qoi_rgba_t px = px_prev;

        const unsigned int px_len = desc.width * desc.height * desc.channels;
        const unsigned int px_end = px_len - desc.channels;
        const unsigned int channels = desc.channels;

        for (size_t px_pos = 0; px_pos < px_len; px_pos += channels) {
            px.r = pixels[px_pos + 0];
            px.g = pixels[px_pos + 1];
            px.b = pixels[px_pos + 2];

            if (channels == 4) {
                px.a = pixels[px_pos + 3];
            }

            if (px == px_prev) {
                run++;
                if (run == 62 || px_pos == px_end) {
                    result[p++] = QOI_OP_RUN | (run - 1);
                    run = 0;
                }
            }
            else {
                if (run > 0) {
                    result[p++] = QOI_OP_RUN | (run - 1);
                    run = 0;
                }

                const size_t index_pos = QOI_COLOR_HASH(px) & (64 - 1);

                if (index[index_pos] == px) {
                    result[p++] = QOI_OP_INDEX | static_cast<uint8_t>(index_pos);
                }
                else {
                    index[index_pos] = px;

                    if (px.a == px_prev.a) {
                        const int vr = px.r - px_prev.r;
                        const int vg = px.g - px_prev.g;
                        const int vb = px.b - px_prev.b;

                        const int vg_r = vr - vg;
                        const int vg_b = vb - vg;

                        if (
                            vr > -3 && vr < 2 &&
                            vg > -3 && vg < 2 &&
                            vb > -3 && vb < 2
                        ) {
                            result[p++] = static_cast<uint8_t>(QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2));
                        }
                        else if (
                            vg_r >  -9 && vg_r <  8 &&
                            vg   > -33 && vg   < 32 &&
                            vg_b >  -9 && vg_b <  8
                        ) {
                            result[p++] = static_cast<uint8_t>(QOI_OP_LUMA | (vg   + 32));
                            result[p++] = static_cast<uint8_t>((vg_r + 8) << 4 | (vg_b +  8));
                        }
                        else {
                            result[p++] = QOI_OP_RGB;
                            result[p++] = px.r;
                            result[p++] = px.g;
                            result[p++] = px.b;
                        }
                    }
                    else {
                        result[p++] = QOI_OP_RGBA;
                        result[p++] = px.r;
                        result[p++] = px.g;
                        result[p++] = px.b;
                        result[p++] = px.a;
                    }
                }
            }
            px_prev = px;
        }

        for (const unsigned char i: qoi_padding) {
            result[p++] = i;
        }

        return result;
    }

    /* Decode a QOI image from memory.
        qoi_desc struct is filled with the description from the file header.
    */
    static constexpr std::tuple<Result_t, Description> decode(std::span<const uint8_t> data, uint8_t channels) {

        Description desc{};
        constexpr auto MakeError = [](auto sv) {
            return MakeResultType<typename Config::vector_type, true>(sv);
        };
        using VectorType = typename Config::vector_type;
        using namespace std::string_view_literals;
        std::array<qoi_rgba_t, 64> index{};
        qoi_rgba_t px{};
        size_t p = 0;
        int run = 0;

        if (
            (channels != 0 && channels != 3 && channels != 4) ||
            data.size() < QOI_HEADER_SIZE + static_cast<int>(sizeof(qoi_padding))
        ) {
            return {MakeError("Input data was not valid, must have positive size"sv), desc};
        }

        const auto& bytes = data;

        const uint32_t header_magic = qoi_read_32(bytes, p);
        desc.width = qoi_read_32(bytes, p);
        desc.height = qoi_read_32(bytes, p);
        desc.channels = bytes[p++];
        desc.colorspace = bytes[p++];

        if (desc.width == 0 || desc.height == 0)
        {
            return {MakeError("Input data contained zero sized axis"sv), desc};
        }

        if (
            desc.channels < 3 || desc.channels > 4 ||
            desc.colorspace > 1 ||
            header_magic != QOI_MAGIC ||
            desc.height >= QOI_PIXELS_MAX / desc.width
        ) {
            return {MakeError("Header data was not valid, size must be positive"sv), desc};
        }

        if (channels == 0) {
            channels = desc.channels;
        }

        const size_t px_len = desc.width * desc.height * channels;
        VectorType pixels{};
        pixels.resize(px_len);

        px.r = 0;
        px.g = 0;
        px.b = 0;
        px.a = 255;

        const size_t chunks_len = bytes.size() - sizeof(qoi_padding);
        for (size_t px_pos = 0; px_pos < px_len; px_pos += channels) {
            if (run > 0) {
                run--;
            }
            else if (p < chunks_len) {
                const size_t b1 = bytes[p++];

                if (b1 == QOI_OP_RGB) {
                    px.r = bytes[p++];
                    px.g = bytes[p++];
                    px.b = bytes[p++];
                }
                else if (b1 == QOI_OP_RGBA) {
                    px.r = bytes[p++];
                    px.g = bytes[p++];
                    px.b = bytes[p++];
                    px.a = bytes[p++];
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                    px = index[b1];
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                    px.r += ((b1 >> 4) & 0x03) - 2;
                    px.g += ((b1 >> 2) & 0x03) - 2;
                    px.b += ( b1       & 0x03) - 2;
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                    const int b2 = bytes[p++];
                    const int vg = (b1 & 0x3f) - 32;
                    px.r += vg - 8 + ((b2 >> 4) & 0x0f);
                    px.g += vg;
                    px.b += vg - 8 +  (b2       & 0x0f);
                }
                else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                    run = (b1 & 0x3f);
                }

                index[QOI_COLOR_HASH(px) & (64 - 1)] = px;
            }

            pixels[px_pos + 0] = px.r;
            pixels[px_pos + 1] = px.g;
            pixels[px_pos + 2] = px.b;

            if (channels == 4) {
                pixels[px_pos + 3] = px.a;
            }
        }

        return {pixels, desc};
    }
};
}
