#include "../dasmig/devicegen.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>

int main()
{
    // The singleton loads the lite tier (compiled in, or from resources/).
    auto& gen = dasmig::dvg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the device data.\n";
        return 1;
    }
    std::cout << "Loaded " << gen.country_count() << " countries and " << gen.model_count() << " models.\n\n";

    std::cout << "Random, at the default date (2023-07-01):\n";
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "  " << gen.get_device(dasmig::device_request{}) << '\n';
    }

    std::cout << "\nAn iPhone in Brazil today, with the user agent it sends:\n";
    dasmig::device_request phone;
    phone.country = "BR";
    phone.platform = "ios-phone";
    phone.locale = "pt-BR";
    phone.reference_date = std::chrono::year{2026} / 10 / 1;
    const auto d = gen.get_device(phone, std::uint64_t{42});
    std::cout << "  " << d << '\n'
              << "  User-Agent: " << d.user_agent << '\n'
              << "  Accept-Language: " << d.accept_language << '\n';

    std::cout << "\nThe same seed replays it exactly:\n  "
              << (gen.get_device(phone, d.seed()).to_json() == d.to_json() ? "yes" : "no") << '\n';

    std::cout << "\nSafe test data: documentation addresses only:\n";
    dasmig::device_request safe;
    safe.safe = true;
    for (int i = 0; i < 3; ++i)
    {
        std::cout << "  " << gen.get_device(safe) << '\n';
    }

    std::cout << "\nAs JSON:\n  " << gen.get_device(dasmig::device_request{}, std::uint64_t{7}).to_json() << '\n';
    return 0;
}
