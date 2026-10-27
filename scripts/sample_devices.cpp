// Writes devices as JSON Lines for scripts/validate_user_agents.py.
// Usage: sample_devices COUNT DATE... (dates as YYYY-MM-DD)
#include "dasmig/devicegen.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: sample_devices COUNT DATE...\n";
        return 2;
    }
    const auto count = std::stoull(argv[1]);
    auto& gen = dasmig::dvg::instance();
    for (int i = 2; i < argc; ++i)
    {
        const std::string date = argv[i];
        dasmig::device_request request;
        request.reference_date = std::chrono::year{std::stoi(date.substr(0, 4))} /
                                 std::chrono::month{static_cast<unsigned>(std::stoi(date.substr(5, 2)))} /
                                 std::chrono::day{static_cast<unsigned>(std::stoi(date.substr(8, 2)))};
        for (std::uint64_t s = 0; s < count; ++s)
        {
            std::cout << gen.get_device(request, dasmig::core::item_seed(static_cast<std::uint64_t>(i), s)).to_json()
                      << '\n';
        }
    }
    return 0;
}
