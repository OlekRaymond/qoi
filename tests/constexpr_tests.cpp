#include <iostream>
#include <vector>
#include <qoi.hpp>

namespace {
    using namespace qoi;
    template<ErrorHandling eh>
    constexpr auto InvalidEncode()
    {
        std::vector<uint8_t> example_data{0,1,2,3,4,5,5,6,7,7,8,8,9};
        Description desc{};
        return qoi::Qoi<DefaultQoiConfig, DefaultQoiConfig{eh}>::encode(example_data, desc);
    }
    static_assert(InvalidEncode<ErrorHandling::TreatAsMonadic>().empty());
    static_assert(!InvalidEncode<ErrorHandling::Optional>().has_value());
    static_assert(!InvalidEncode<ErrorHandling::Optional>().has_value());

    template<ErrorHandling eh>
    constexpr auto ValidEncode()
    {
        std::vector<uint8_t> example_data{0,1,2,3,4,5,5,6,7,7,8,8};
        constexpr qoi::Description desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        return qoi::Qoi<DefaultQoiConfig, DefaultQoiConfig{eh}>::encode(example_data, desc);
    }
    static_assert(!ValidEncode<ErrorHandling::TreatAsMonadic>().empty());
    static_assert(ValidEncode<ErrorHandling::Optional>().has_value());
    static_assert(ValidEncode<ErrorHandling::Expected>().has_value());

    constexpr int ConstexprTest(const std::vector<uint8_t>& example_data)
    {
        using Q = qoi::Qoi<qoi::DefaultQoiConfig, qoi::DefaultQoiConfig{qoi::ErrorHandling::Expected}>;
        constexpr qoi::Description desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        auto encoded = Q::encode(example_data, desc);
        if (!encoded.has_value()) {
            return 1;
        }
        const auto [decoded, desc_out] = Q::decode(*encoded, 3);
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

    inline void RuntimeTest(const std::vector<uint8_t>& example_data)
    {
        using Q = qoi::Qoi<qoi::DefaultQoiConfig, qoi::DefaultQoiConfig{qoi::ErrorHandling::Expected}>;
        constexpr qoi::Description desc{
            .width=2,
            .height=2,
            .channels=3,
            .colorspace=0
        };
        auto encoded = Q::encode(example_data, desc);
        if (!encoded.has_value()) {
            std::cout << "encode did not create valid value: " << encoded.error() << "\n";
        }
        std::cout << "Encoded size: " << (*encoded).size() << "\n";
        const auto [decoded, desc_out] = Q::decode(*encoded, 3);
        if (!decoded.has_value())
        {
            std::cout << "Decode was unsuccessful: " << decoded.error() << "\n";
        }
        if (decoded != example_data)
        {
            std::cout << "round trip was not successful\n";
        }
    }
} // namespace

int main()
{
#define Both( ... ) RuntimeTest( __VA_ARGS__ ); static_assert(ConstexprTest( __VA_ARGS__ ) == 0)

    Both({0,1,2,3,4,5,5,6,7,7,8,8});
    Both({128,128,128,3,4,5,5,6,7,7,8,8});
    Both({128,128,128,255,255,255,5,6,7,7,8,8});

    static_assert(ConstexprTest({0x32,0x32,0x32,0xf0,0x41,0xf0}) == 1);
}
