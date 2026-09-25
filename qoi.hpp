#pragma once
#include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <string_view>
    #include <vector>
    #include <array>
    #include <bit>
    #include <algorithm>

// TODO: Some form of malloc indirection

    #define QOI_CPP_STD_EXCEPTIONS (defined(__cpp_exceptions) && __cpp_exceptions)
    #define QOI_GCC_EXCEPTIONS (defined(__EXCEPTIONS) && __EXCEPTIONS)
    #define QOI_MSVC_EXCEPTIONS (defined(_HAS_EXCEPTIONS) && _HAS_EXCEPTIONS)

    #if !defined(QOI_HAS_EXCEPTIONS) && (QOI_CPP_STD_EXCEPTIONS || QOI_GCC_EXCEPTIONS || QOI_MSVC_EXCEPTIONS )
    #define QOI_HAS_EXCEPTIONS 1
    #define QOI_IF_HAS_EXCEPTIONS(...) __VA_ARGS__
    #include <stdexcept>
    #else
    #define QOI_HAS_EXCEPTIONS 0
    #define QOI_IF_HAS_EXCEPTIONS(...)
    #endif

    #include <version>

    #if !defined(QOI_HAS_EXPECTED) && defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    #define QOI_HAS_EXPECTED 1
    #define QOI_IF_HAS_EXPECTED(...) __VA_ARGS__
    #include <expected>
    #else
    #define QOI_HAS_EXPECTED 0
    #define QOI_IF_HAS_EXPECTED(...)
    #endif

    #if !defined(QOI_HAS_OPTIONAL)
    #if __cpp_lib_optional >= 201606L
    #define QOI_HAS_OPTIONAL 1
    #define QOI_IF_HAS_OPTIONAL(...) __VA_ARGS__
    #include <optional>
    #else
    #define QOI_IF_HAS_OPTIONAL(...)
    #define QOI_HAS_OPTIONAL 0
    #endif
    #endif

    #if defined(QOI_CUSTOM_TYPE)
    #define QOI_IF_CUSTOM_TYPE(...) __VA_ARGS__
    #else
    #define QOI_IF_CUSTOM_TYPE(...)
    #endif

    namespace qoi
    {
        /* A pointer to a qoi_desc struct has to be supplied to all of qoi's functions.
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

        struct qoi_desc {
            unsigned int width;
            unsigned int height;
            unsigned char channels;
            unsigned char colorspace;
            constexpr auto operator<=>(const qoi_desc&) const noexcept = default;
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
            return ui('q') << 24 |
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
            uint8_t g;uint8_t b;uint8_t a;
            constexpr auto operator<=>(const qoi_rgba_t&) const noexcept = default;
        };

        constexpr uint32_t QOI_COLOR_HASH(const qoi_rgba_t& color) {
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
            return b0 << 24 | b1 << 16 | b2 << 8 | b3;
        }

        [[deprecated("for backwards compat until change")]]
        constexpr uint8_t qoi_read_32(const ConstByteBuffer auto& bytes, auto* p) {
            return qoi_read_32(bytes, static_cast<size_t&>(*p));
        }

    enum struct ErrorHandling {
        QOI_IF_HAS_EXCEPTIONS(Exceptions,)
        QOI_IF_HAS_EXPECTED(Expected,)
        QOI_IF_HAS_OPTIONAL(Optional,)
        QOI_IF_CUSTOM_TYPE(CustomType,)
        TreatAsMonadic // e.g. returning a 0 length vector
    };

    template<ErrorHandling errorHandling_t>
    class Qoi {
        QOI_IF_HAS_EXCEPTIONS(struct QoiException : std::runtime_error {}; )

        template<typename Type, bool error_t = false>
        static constexpr auto MakeResultType(auto... args) {
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
        template<typename T>
        using Result_t = decltype(MakeResultType<T, false>());
    public:
        constexpr static Result_t<std::vector<uint8_t>> encode(const void *data, const qoi_desc *desc) {
            using T = std::vector<uint8_t>;

            std::array<qoi_rgba_t, 64> index{};
            qoi_rgba_t px_prev{};

            if (
                data == nullptr || desc == nullptr ||
                desc->width == 0 || desc->height == 0 ||
                desc->channels < 3 || desc->channels > 4 ||
                desc->colorspace > 1 ||
                desc->height >= QOI_PIXELS_MAX / desc->width
            ) {
                return MakeResultType<T, true>("Invalid input, qoi has 3|4 channels and a resonable amount of pixesls");
            }

            const size_t max_size = desc->width * desc->height * (desc->channels + 1) +
                        QOI_HEADER_SIZE + sizeof(qoi_padding);

            size_t p = 0;
            std::vector<uint8_t> result {};
            result.resize(max_size);

            qoi_write_32(result, p, QOI_MAGIC);
            qoi_write_32(result, p, desc->width);
            qoi_write_32(result, p, desc->height);
            result[p++] = desc->channels;
            result[p++] = desc->colorspace;

            const auto *pixels = static_cast<const unsigned char *>(data);

            uint8_t run = 0;
            px_prev.r = 0;
            px_prev.g = 0;
            px_prev.b = 0;
            px_prev.a = 255;
            qoi_rgba_t px = px_prev;

            const unsigned int px_len = desc->width * desc->height * desc->channels;
            const unsigned int px_end = px_len - desc->channels;
            const unsigned int channels = desc->channels;

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
                        result[p++] = QOI_OP_INDEX | index_pos;
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
                                result[p++] = QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
                            }
                            else if (
                                vg_r >  -9 && vg_r <  8 &&
                                vg   > -33 && vg   < 32 &&
                                vg_b >  -9 && vg_b <  8
                            ) {
                                result[p++] = QOI_OP_LUMA     | (vg   + 32);
                                result[p++] = (vg_r + 8) << 4 | (vg_b +  8);
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
        /* Encode raw RGB or RGBA pixels into a QOI image in memory.

            The function either returns NULL on failure (invalid parameters or malloc
            failed) or a pointer to the encoded data on success. On success the out_len
            is set to the size in bytes of the encoded data.

            The returned qoi data should be free()d after use. */


        /* Decode a QOI image from memory.

        The function either returns NULL on failure (invalid parameters or malloc
        failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
        is filled with the description from the file header.

        The returned pixel data should be free()d after use. */
        static constexpr Result_t<std::vector<uint8_t>> qoi_decode(const void *data, const size_t size, qoi_desc *desc, int channels) {
            using ResultValue = std::vector<uint8_t>;
            constexpr auto MakeError = [](auto sv) {
                return MakeResultType<ResultValue, true>(sv);
            };
            using namespace std::string_view_literals;
            std::array<qoi_rgba_t, 64> index{};
            qoi_rgba_t px{};
            size_t p = 0;
            int run = 0;

            if (data == nullptr || desc == nullptr)
            {
                return MakeError("input data was null"sv);
            }

            if (
                (channels != 0 && channels != 3 && channels != 4) ||
                size < QOI_HEADER_SIZE + static_cast<int>(sizeof(qoi_padding))
            ) {
                return MakeError("Input data was not valid, must have positive size"sv);
            }

            const auto *bytes = static_cast<const unsigned char *>(data);

            const uint32_t header_magic = qoi_read_32(bytes, p);
            desc->width = qoi_read_32(bytes, p);
            desc->height = qoi_read_32(bytes, p);
            desc->channels = bytes[p++];
            desc->colorspace = bytes[p++];

            if (desc->width == 0 || desc->height == 0)
            {
                return MakeError("Input data contained zero sized axis"sv);
            }

            if (
                desc->channels < 3 || desc->channels > 4 ||
                desc->colorspace > 1 ||
                header_magic != QOI_MAGIC ||
                desc->height >= QOI_PIXELS_MAX / desc->width
            ) {
                return MakeError("Header data was not valid, size must be positive"sv);
            }

            if (channels == 0) {
                channels = desc->channels;
            }

            const size_t px_len = desc->width * desc->height * channels;
            std::vector<unsigned char> pixels{};
            pixels.resize(px_len);

            px.r = 0;
            px.g = 0;
            px.b = 0;
            px.a = 255;

            const size_t chunks_len = size - sizeof(qoi_padding);
            for (size_t px_pos = 0; px_pos < px_len; px_pos += channels) {
                if (run > 0) {
                    run--;
                }
                else if (p < chunks_len) {
                    const int b1 = bytes[p++];

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

            return pixels;
        }
    };

    template<ErrorHandling eh>
    constexpr auto InvalidEncode()
    {
        std::vector<uint8_t> example_data{0,1,2,3,4,5,5,6,7,7,8,8,9};
        qoi_desc desc{};
        return qoi::Qoi<eh>::encode(example_data.data(), &desc);
    }
    static_assert(InvalidEncode<ErrorHandling::TreatAsMonadic>().empty());
    static_assert(!InvalidEncode<ErrorHandling::Optional>().has_value());
    static_assert(!InvalidEncode<ErrorHandling::Optional>().has_value());

    template<ErrorHandling eh>
    constexpr auto ValidEncode()
    {
        std::vector<uint8_t> example_data{0,1,2,3,4,5,5,6,7,7,8,8};
        constexpr qoi::qoi_desc desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        return qoi::Qoi<eh>::encode(example_data.data(), &desc);
    }
    static_assert(!ValidEncode<ErrorHandling::TreatAsMonadic>().empty());
    static_assert(ValidEncode<ErrorHandling::Optional>().has_value());
    static_assert(ValidEncode<ErrorHandling::Expected>().has_value());

    #define QOI_NO_STDIO
    #ifndef QOI_NO_STDIO
    #include <cstdio>

        /* Encode raw RGB or RGBA pixels into a QOI image and write it to the file
        system. The qoi_desc struct must be filled with the image width, height,
        number of channels (3 = RGB, 4 = RGBA) and the colorspace.

        The function returns 0 on failure (invalid parameters, or fopen or malloc
        failed) or the number of bytes written on success. */
        inline int qoi_write(const char *filename, const void *data, const qoi_desc *desc) {
            FILE *f = fopen(filename, "wb");
            int size;

            if (!f) {
                return 0;
            }

            void *encoded = qoi_encode(data, desc, &size);
            if (!encoded) {
                fclose(f);
                return 0;
            }

            fwrite(encoded, 1, size, f);
            fflush(f);
            const int err = ferror(f);
            fclose(f);

            QOI_FREE(encoded);
            return err ? 0 : size;
        }

        /* Read and decode a QOI image from the file system. If channels is 0, the
        number of channels from the file header is used. If channels is 3 or 4 the
        output format will be forced into this number of channels.

        The function either returns NULL on failure (invalid data, or malloc or fopen
        failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
        will be filled with the description from the file header.

        The returned pixel data should be free()d after use. */
        inline void *qoi_read(const char *filename, qoi_desc *desc, int channels) {
            FILE *f = fopen(filename, "rb");

            if (!f) {
                return nullptr;
            }

            fseek(f, 0, SEEK_END);
            const int size = ftell(f);
            if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) {
                fclose(f);
                return nullptr;
            }

            void *data = QOI_MALLOC(size);
            if (!data) {
                fclose(f);
                return nullptr;
            }

            int bytes_read = fread(data, 1, size, f);
            fclose(f);
            void *pixels = (bytes_read != size) ? nullptr : qoi_decode(data, bytes_read, desc, channels);
            QOI_FREE(data);
            return pixels;
        }

    #endif /* QOI_NO_STDIO */

    } // namespace

    constexpr int ConstexprTest(const std::vector<uint8_t>& example_data)
    {
        using Q = qoi::Qoi<qoi::ErrorHandling::Expected>;
        constexpr qoi::qoi_desc desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        auto encoded = Q::encode(example_data.data(), &desc);
        if (!encoded.has_value()) {
            return 1;
        }
        qoi::qoi_desc desc_out{};
        const auto decoded = Q::qoi_decode((*encoded).data(), (*encoded).size(), &desc_out, 3);
        if (!decoded.has_value())
        {
            return 2;
        }
        if (decoded != example_data)
        {
            return 3;
        }
        return 0;
    }

    #include <iostream>

    void RuntimeTest(const std::vector<uint8_t>& example_data)
    {
        using Q = qoi::Qoi<qoi::ErrorHandling::Expected>;
        constexpr qoi::qoi_desc desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        auto encoded = Q::encode(example_data.data(), &desc);
        if (!encoded.has_value()) {
            std::cout << "encode did not create valid value: " << encoded.error() << "\n";
        }
        std::cout << "Encoded size: " << (*encoded).size() << "\n";
        uint8_t* in = (*encoded).data();
        qoi::qoi_desc desc_out{};
        const auto decoded = Q::qoi_decode((*encoded).data(), (*encoded).size(), &desc_out, 3);
        if (!decoded.has_value())
        {
            std::cout << "Decode was unsuccessful: " << decoded.error() << "\n";
        }
        if (decoded != example_data)
        {
            std::cout << "round trip was not successful\n";
        }
    }

    int main()
    {
        #define Both( ... ) RuntimeTest( __VA_ARGS__ ); static_assert(ConstexprTest( __VA_ARGS__ ) == 0);

        Both({0,1,2,3,4,5,5,6,7,7,8,8});
        Both({128,128,128,3,4,5,5,6,7,7,8,8});
        Both({128,128,128,255,255,255,5,6,7,7,8,8});



    }
