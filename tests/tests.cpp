#include "catch_amalgamated.hpp"
#include "../dasmig/devicegen.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

using namespace std::chrono;

std::filesystem::path lite_dir()
{
    for (const auto* candidate : {"resources/lite", "../resources/lite"})
    {
        if (std::filesystem::is_directory(candidate)) return candidate;
    }
    return {};
}

dasmig::dvg make_loaded_generator()
{
    dasmig::dvg gen;
    if (!gen.load(dasmig::dataset::lite)) gen.load(lite_dir());
    REQUIRE(gen.has_data());
    return gen;
}

dasmig::device_request at(year_month_day date)
{
    dasmig::device_request r;
    r.reference_date = date;
    return r;
}

int day_of(std::string_view iso)
{
    const int y = std::stoi(std::string{iso.substr(0, 4)});
    const unsigned m = std::stoul(std::string{iso.substr(5, 2)});
    const unsigned d = std::stoul(std::string{iso.substr(8, 2)});
    return static_cast<int>(sys_days{year{y} / month{m} / day{d}}.time_since_epoch().count());
}

int day_of(year_month_day date) { return static_cast<int>(sys_days{date}.time_since_epoch().count()); }

int major_of(std::string_view version) { return std::stoi(std::string{version.substr(0, version.find('.'))}); }

// The shipped tables, read directly, to check results against.
struct reference_data
{
    std::map<std::string, std::map<std::string, int>> releases; // product -> version -> day
    std::map<std::string, std::pair<int, std::string>> models;   // model name -> released, platform
    std::map<std::string, std::vector<std::pair<std::uint32_t, std::uint64_t>>> ipv4;
    std::map<std::string, std::vector<std::pair<std::array<std::uint16_t, 8>, int>>> ipv6;

    reference_data()
    {
        const auto dir = lite_dir();
        const auto releases_doc = dasmig::core::tsv_document::load(dir / "releases.tsv");
        for (const auto& row : releases_doc.rows())
            releases[row.get<std::string>("product")][row.get<std::string>("version")] =
                day_of(row.get<std::string>("released"));
        const auto models_doc = dasmig::core::tsv_document::load(dir / "models.tsv");
        for (const auto& row : models_doc.rows())
            models[row.get<std::string>("model")] = {day_of(row.get<std::string>("released")),
                                                     row.get<std::string>("platform")};
        const auto ipv4_doc = dasmig::core::tsv_document::load(dir / "ipv4.tsv");
        for (const auto& row : ipv4_doc.rows())
            ipv4[row.get<std::string>("country_code")].emplace_back(parse4(row.get<std::string>("start")),
                                                                    row.get<std::uint64_t>("size"));
        const auto ipv6_doc = dasmig::core::tsv_document::load(dir / "ipv6.tsv");
        for (const auto& row : ipv6_doc.rows())
            ipv6[row.get<std::string>("country_code")].emplace_back(parse6(row.get<std::string>("prefix")),
                                                                    row.get<int>("length"));
    }

    static std::uint32_t parse4(const std::string& text)
    {
        std::uint32_t out = 0;
        std::istringstream in{text};
        std::string part;
        while (std::getline(in, part, '.')) out = (out << 8U) | static_cast<std::uint32_t>(std::stoul(part));
        return out;
    }

    static std::array<std::uint16_t, 8> parse6(const std::string& text)
    {
        std::array<std::uint16_t, 8> g{};
        const auto gap = text.find("::");
        const auto groups = [](const std::string& part) {
            std::vector<std::uint16_t> out;
            std::istringstream in{part};
            std::string h;
            while (std::getline(in, h, ':'))
                if (!h.empty()) out.push_back(static_cast<std::uint16_t>(std::stoul(h, nullptr, 16)));
            return out;
        };
        const auto head = groups(text.substr(0, gap));
        const auto tail = gap == std::string::npos ? std::vector<std::uint16_t>{} : groups(text.substr(gap + 2));
        std::ranges::copy(head, g.begin());
        std::ranges::copy(tail, g.end() - static_cast<std::ptrdiff_t>(tail.size()));
        return g;
    }

    [[nodiscard]] bool in_country(const std::string& country, const std::string& ip) const
    {
        if (ip.find(':') == std::string::npos)
        {
            const auto a = parse4(ip);
            const auto it = ipv4.find(country);
            if (it == ipv4.end()) return false;
            return std::ranges::any_of(it->second, [&](const auto& b) { return a >= b.first && a - b.first < b.second; });
        }
        const auto a = parse6(ip);
        const auto it = ipv6.find(country);
        if (it == ipv6.end()) return false;
        return std::ranges::any_of(it->second, [&](const auto& n) { return same_prefix(a, n.first, n.second); });
    }

    static bool same_prefix(const std::array<std::uint16_t, 8>& a, const std::array<std::uint16_t, 8>& b, int bits)
    {
        for (std::size_t i = 0; i < 8 && bits > 0; ++i, bits -= 16)
        {
            const unsigned keep = bits >= 16 ? 16U : static_cast<unsigned>(bits);
            const auto mask = static_cast<std::uint16_t>(0xFFFFU << (16U - keep));
            if ((a.at(i) & mask) != (b.at(i) & mask)) return false;
        }
        return true;
    }
};

const reference_data& ref()
{
    static const reference_data data;
    return data;
}

bool in_documentation_range(const std::string& ip)
{
    return ip.starts_with("192.0.2.") || ip.starts_with("198.51.100.") || ip.starts_with("203.0.113.") ||
           ip.starts_with("2001:db8:");
}

std::string os_product(const std::string& os)
{
    if (os == "iOS") return "ios";
    if (os == "iPadOS") return "ipados";
    if (os == "Android") return "android";
    if (os == "Windows") return "windows";
    if (os == "macOS") return "macos";
    return {};
}

std::string browser_product(const dasmig::device& d)
{
    if (d.browser == "chrome" && (d.os == "iOS" || d.os == "iPadOS")) return "chrome-ios";
    return d.browser;
}

} // namespace

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads every table", "[loading]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.country_count() == 195);
    REQUIRE(gen.model_count() > 100);
    REQUIRE(gen.platforms().size() == 9);
    for (const auto t : {dasmig::device_table::countries, dasmig::device_table::models,
                         dasmig::device_table::releases, dasmig::device_table::ipv4,
                         dasmig::device_table::ua_templates, dasmig::device_table::reserved})
    {
        REQUIRE(gen.layers(t).size() == 1);
    }
    const auto codes = gen.countries();
    REQUIRE(std::ranges::is_sorted(codes));
    REQUIRE(std::ranges::find(codes, "BR") != codes.end());
}

TEST_CASE("load - non-existent path is a no-op", "[loading]")
{
    dasmig::dvg gen;
    gen.load(std::filesystem::path{"no/such/directory"});
    REQUIRE_FALSE(gen.has_data());
    REQUIRE(gen.data_hash() == 0);
}

#ifndef DASMIG_DEVICE_EMBED // compiled-in data is never missing
TEST_CASE("load(dataset) returns false for a missing tier", "[loading]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "dvg_empty_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::dvg gen;
    const bool loaded = gen.load(dasmig::dataset::full);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(gen.has_data());
}
#endif

TEST_CASE("the singleton loads data on first use", "[loading]")
{
    REQUIRE(dasmig::dvg::instance().has_data());
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

TEST_CASE("get_device throws when no data is loaded", "[errors]")
{
    dasmig::dvg gen;
    REQUIRE_THROWS_AS(gen.get_device(dasmig::device_request{}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_device(dasmig::device_request{}, std::uint64_t{1}), std::runtime_error);
}

TEST_CASE("invalid requests are rejected", "[errors]")
{
    const auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_device(at(year{2020} / 12 / 31), 1), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_device(at(year{2023} / 2 / 30), 1), std::invalid_argument);
    REQUIRE_NOTHROW(gen.get_device(at(year{2021} / 1 / 1), 1));

    dasmig::device_request r;
    r.country = "QQ";
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);

    r = {};
    r.platform = "toaster";
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);

    r = {};
    r.platform = "ios-phone";
    r.kind = dasmig::device_kind::desktop;
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);

    r = {};
    r.platform = "android-phone";
    r.browser = "safari";
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);

    r = {};
    r.browser = "netscape";
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);
}

TEST_CASE("a locale that is not a language tag is rejected", "[errors]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    for (const auto* bad : {"pt-BR\r\nX-Injected: 1", "en US", "-en", "pt;q=1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                            "pt-", "pt--BR", "pt_", "123", "e", "en-toolongsub", "1en-US"})
    {
        r.locale = bad;
        REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);
    }
    r.locale = "zh-Hant-TW";
    REQUIRE(gen.get_device(r, 1).accept_language == "zh-Hant-TW,zh;q=0.9,en;q=0.8");
}

TEST_CASE("a country without registered addresses needs safe", "[errors][ip]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.country = "xk"; // Kosovo: in the country list, no registry block
    REQUIRE_THROWS_AS(gen.get_device(r, 1), std::invalid_argument);
    r.safe = true;
    const auto d = gen.get_device(r, 1);
    REQUIRE(d.country_code == "XK");
    REQUIRE(d.reserved);
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

TEST_CASE("get_device with a seed is deterministic", "[seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 100; ++s)
    {
        REQUIRE(gen.get_device({}, s).to_json() == gen.get_device({}, s).to_json());
    }
}

TEST_CASE("a result replays from its seed", "[seed]")
{
    auto gen = make_loaded_generator();
    const auto v = gen.get_device({});
    REQUIRE(gen.get_device({}, v.seed()).to_json() == v.to_json());
}

TEST_CASE("explicit per-call seeds replay across generator instances", "[seed]")
{
    const auto first = make_loaded_generator();
    const auto second = make_loaded_generator();
    REQUIRE(first.get_device({}, 42).to_json() == second.get_device({}, 42).to_json());
}

// ---------------------------------------------------------------------------
// Coherence: every part of a device fits the others and the date
// ---------------------------------------------------------------------------

TEST_CASE("devices are coherent at every date", "[coherence]")
{
    const auto gen = make_loaded_generator();
    const auto& data = ref();
    for (const auto date : {year{2021} / 1 / 1, year{2022} / 6 / 15, year{2023} / 7 / 1, year{2025} / 3 / 1,
                            year{2026} / 10 / 1, year{2031} / 1 / 1})
    {
        const int today = day_of(date);
        for (std::uint64_t s = 0; s < 4000; ++s)
        {
            const auto d = gen.get_device(at(date), s);
            INFO(d.to_json());

            REQUIRE(data.in_country(d.country_code, d.ip));
            REQUIRE_FALSE(d.reserved);
            REQUIRE((d.ip_version == 6) == (d.ip.find(':') != std::string::npos));

            if (!d.model.empty())
            {
                const auto& [released, platform] = data.models.at(d.model);
                REQUIRE(released <= today);
                REQUIRE(platform == d.platform);
            }
            if (const auto product = os_product(d.os); !product.empty())
            {
                REQUIRE_FALSE(d.os_version.empty());
                const auto major = d.os == "macOS" || d.os == "Windows" || d.os == "Android"
                                       ? d.os_version
                                       : std::to_string(major_of(d.os_version));
                REQUIRE(data.releases.at(product).contains(major));
                REQUIRE(data.releases.at(product).at(major) <= today);
            }
            if (d.kind == "other")
            {
                REQUIRE(d.browser.empty());
                REQUIRE(d.user_agent.empty());
                continue;
            }

            REQUIRE_FALSE(d.browser.empty());
            REQUIRE_FALSE(d.user_agent.empty());
            REQUIRE(d.user_agent.starts_with("Mozilla/5.0 ("));
            REQUIRE(d.user_agent.find('{') == std::string::npos);
            REQUIRE(d.screen.width > 0);
            REQUIRE(d.screen.height > 0);
            REQUIRE(d.screen.pixel_ratio > 0.0);

            const bool apple = d.os == "iOS" || d.os == "iPadOS" || d.os == "macOS";
            if (d.browser == "safari") REQUIRE(apple);
            if (d.browser == "samsung") REQUIRE(d.maker == "Samsung");
            if (d.os == "iOS" || d.os == "iPadOS")
            {
                REQUIRE(d.engine == "WebKit");
                if (d.browser == "safari") REQUIRE(d.browser_version == d.os_version);
            }
            else
            {
                const auto product = browser_product(d);
                const auto version = d.browser == "safari" ? std::to_string(major_of(d.browser_version))
                                                           : d.browser_version;
                REQUIRE(data.releases.at(product).at(version) <= today);
            }

            const auto& ua = d.user_agent;
            if (d.browser == "chrome")
                REQUIRE((ua.find("Chrome/") != std::string::npos || ua.find("CriOS/") != std::string::npos));
            if (d.browser == "firefox")
                REQUIRE((ua.find("Firefox/") != std::string::npos || ua.find("FxiOS/") != std::string::npos));
            if (d.browser == "edge") REQUIRE(ua.find("Edg") != std::string::npos);
            if (d.browser == "samsung") REQUIRE(ua.find("SamsungBrowser/") != std::string::npos);
            if (d.browser == "opera") REQUIRE(ua.find(" OPR/") != std::string::npos);
            if (d.browser == "yandex") REQUIRE(ua.find(" YaBrowser/") != std::string::npos);
            if (d.browser == "safari") REQUIRE(ua.find("Version/" + d.browser_version) != std::string::npos);
            if (d.os == "Android" && (d.browser == "chrome" || d.browser == "edge"))
            {
                const bool reduced = major_of(d.browser_version) >= 110;
                REQUIRE((ua.find("Android 10; K)") != std::string::npos) == reduced);
            }
            if (d.os == "iOS" && major_of(d.os_version) >= 26) REQUIRE(ua.find("OS 18_6 like") != std::string::npos);
            if (d.os == "Windows")
                REQUIRE(ua.find(d.os_version == "7" ? "Windows NT 6.1" : (d.os_version == "8.1" ? "Windows NT 6.3"
                                                                                                 : "Windows NT 10.0")) !=
                        std::string::npos);
        }
    }
}

TEST_CASE("browser versions respect the OS they run on", "[coherence]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2025} / 6 / 1);
    r.platform = "windows-desktop";
    r.browser = "chrome";
    int seven = 0;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto d = gen.get_device(r, s);
        if (d.os_version == "7" || d.os_version == "8.1")
        {
            ++seven;
            REQUIRE(major_of(d.browser_version) <= 109); // Chrome 110 needs Windows 10
        }
    }
    REQUIRE(seven > 0);
}

TEST_CASE("a date after the data is seen as the data's newest date", "[coherence]")
{
    const auto gen = make_loaded_generator();
    const auto newest = year{2026} / 10 / 27; // Firefox 159, the newest release in the shipped data
    for (std::uint64_t s = 0; s < 200; ++s)
    {
        auto late = gen.get_device(at(year{2060} / 1 / 1), s).to_json();
        REQUIRE(late == gen.get_device(at(newest), s).to_json());
    }
}

TEST_CASE("old systems keep the last browser versions they run", "[coherence]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2026} / 10 / 1);
    r.platform = "windows-desktop";
    int seven = 0;
    for (std::uint64_t s = 0; s < 4000; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE_FALSE(d.browser.empty());
        if (d.os_version != "7") continue;
        ++seven;
        if (d.browser == "chrome" || d.browser == "edge") REQUIRE(d.browser_version == "109");
        if (d.browser == "firefox") REQUIRE(major_of(d.browser_version) <= 115);
    }
    REQUIRE(seven > 0);
}

TEST_CASE("Safari on a Mac is at least the version macOS ships with", "[coherence]")
{
    const auto gen = make_loaded_generator();
    const std::map<std::string, int> shipped{{"10.15", 13}, {"11", 14}, {"12", 15}, {"13", 16},
                                             {"14", 17},    {"15", 18}, {"26", 26}, {"27", 27}};
    dasmig::device_request r = at(year{2026} / 10 / 1);
    r.platform = "macos-desktop";
    r.browser = "safari";
    int checked = 0;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto d = gen.get_device(r, s);
        const auto it = shipped.find(d.os_version);
        if (it == shipped.end()) continue;
        ++checked;
        REQUIRE(major_of(d.browser_version) >= it->second);
    }
    REQUIRE(checked > 2500);
}

TEST_CASE("iPads ask for the desktop site from iPadOS 13", "[coherence]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2021} / 1 / 1);
    r.platform = "ipad";
    r.browser = "safari";
    int before_13 = 0;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto d = gen.get_device(r, s);
        if (major_of(d.os_version) < 13)
        {
            ++before_13;
            REQUIRE(d.user_agent.starts_with("Mozilla/5.0 (iPad; CPU OS 12_"));
        }
        else
        {
            REQUIRE(d.user_agent.starts_with("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"));
        }
    }
    REQUIRE(before_13 > 0);
}

TEST_CASE("client hints match what Chrome sends", "[coherence][hints]")
{
    // Sec-CH-UA as real Chrome releases on Windows send it.
    const std::map<int, std::string> expected{
        {103, R"(".Not/A)Brand";v="99", "Google Chrome";v="103", "Chromium";v="103")"},
        {104, R"("Chromium";v="104", " Not A;Brand";v="99", "Google Chrome";v="104")"},
        {105, R"("Google Chrome";v="105", "Not)A;Brand";v="8", "Chromium";v="105")"},
        {106, R"("Chromium";v="106", "Google Chrome";v="106", "Not;A=Brand";v="99")"},
        {108, R"("Not?A_Brand";v="8", "Chromium";v="108", "Google Chrome";v="108")"},
        {110, R"("Chromium";v="110", "Not A(Brand";v="24", "Google Chrome";v="110")"},
        {120, R"("Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120")"},
        {124, R"("Chromium";v="124", "Google Chrome";v="124", "Not-A.Brand";v="99")"},
    };
    const auto gen = make_loaded_generator();
    std::set<int> seen;
    for (const auto date : {year{2022} / 7 / 15, year{2022} / 8 / 20, year{2022} / 9 / 20, year{2022} / 10 / 20,
                            year{2022} / 12 / 20, year{2023} / 2 / 20, year{2024} / 1 / 5, year{2024} / 5 / 5})
    {
        dasmig::device_request r = at(date);
        r.platform = "windows-desktop";
        r.browser = "chrome";
        for (std::uint64_t s = 0; s < 400; ++s)
        {
            const auto d = gen.get_device(r, s);
            const auto it = expected.find(major_of(d.browser_version));
            if (it == expected.end()) continue;
            seen.insert(it->first);
            REQUIRE(d.sec_ch_ua == it->second);
            REQUIRE(d.sec_ch_ua_mobile == "?0");
            REQUIRE(d.sec_ch_ua_platform == R"("Windows")");
        }
    }
    REQUIRE(seen.size() == expected.size());
}

TEST_CASE("client hints are sent by Chromium browsers only", "[coherence][hints]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 4000; ++s)
    {
        const auto d = gen.get_device(at(year{2025} / 3 / 1), s);
        const bool sends = (d.browser == "chrome" || d.browser == "edge") && d.os != "iOS" && d.os != "iPadOS";
        INFO(d.to_json());
        REQUIRE(d.sec_ch_ua.empty() != sends);
        if (!sends) continue;
        REQUIRE(d.sec_ch_ua_mobile == (d.kind == "phone" ? "?1" : "?0"));
        REQUIRE(d.sec_ch_ua_platform == "\"" + (d.os == "ChromeOS" ? std::string{"Chrome OS"} : d.os) + "\"");
        REQUIRE(d.sec_ch_ua.find(d.browser == "edge" ? "\"Microsoft Edge\"" : "\"Google Chrome\"") != std::string::npos);
    }
}

TEST_CASE("shares follow their trends over the years", "[stats]")
{
    const auto gen = make_loaded_generator();
    const auto huawei_share = [&](year_month_day date) {
        dasmig::device_request r = at(date);
        r.country = "DE";
        r.platform = "android-phone";
        int n = 0;
        for (std::uint64_t s = 0; s < 4000; ++s) n += gen.get_device(r, s).maker == "Huawei" ? 1 : 0;
        return n;
    };
    REQUIRE(huawei_share(year{2021} / 6 / 1) > 2 * huawei_share(year{2025} / 6 / 1));

    const auto edge_share = [&](year_month_day date) {
        dasmig::device_request r = at(date);
        r.platform = "windows-desktop";
        int n = 0;
        for (std::uint64_t s = 0; s < 4000; ++s) n += gen.get_device(r, s).browser == "edge" ? 1 : 0;
        return n;
    };
    REQUIRE(edge_share(year{2026} / 6 / 1) > edge_share(year{2021} / 6 / 1));
}

TEST_CASE("Opera and Yandex Browser", "[coherence]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2025} / 3 / 1);
    r.browser = "yandex";
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE((d.country_code == "RU" || d.country_code == "BY" || d.country_code == "KZ"));
        REQUIRE(d.user_agent.find("YaBrowser/" + d.browser_version + ".0") != std::string::npos);
    }
    r = at(year{2026} / 1 / 1);
    r.browser = "opera";
    r.platform = "windows-desktop";
    int seven = 0;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE(d.user_agent.ends_with(" OPR/" + d.browser_version + ".0.0.0"));
        // Opera N is built on Chromium N + 14.
        REQUIRE(d.user_agent.find("Chrome/" + std::to_string(major_of(d.browser_version) + 14) + ".0.0.0") !=
                std::string::npos);
        if (d.os_version == "7")
        {
            ++seven;
            REQUIRE(major_of(d.browser_version) <= 95); // Opera 96 needs Windows 10, like Chromium 110
        }
    }
    REQUIRE(seven > 0);
}

TEST_CASE("models only run OS versions they support", "[coherence]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2026} / 10 / 1);
    r.platform = "ios-phone";
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto d = gen.get_device(r, s);
        if (d.model == "iPhone 8" || d.model == "iPhone X") REQUIRE(major_of(d.os_version) <= 16);
        if (d.model == "iPhone 17") REQUIRE(major_of(d.os_version) >= 26);
    }
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

TEST_CASE("given fields are honoured and shift no other draw", "[request]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 300; ++s)
    {
        const auto free = gen.get_device({}, s);

        dasmig::device_request r;
        r.country = free.country_code;
        REQUIRE(gen.get_device(r, s).to_json() == free.to_json());

        r = {};
        r.platform = free.platform;
        REQUIRE(gen.get_device(r, s).to_json() == free.to_json());

        r = {};
        r.locale = "de-DE";
        const auto with_locale = gen.get_device(r, s);
        REQUIRE(with_locale.accept_language == "de-DE,de;q=0.9,en;q=0.8");
        REQUIRE(with_locale.user_agent == free.user_agent);
        REQUIRE(with_locale.ip == free.ip);

        r = {};
        r.ip = dasmig::ip_version::v4;
        const auto v4 = gen.get_device(r, s);
        REQUIRE(v4.ip_version == 4);
        REQUIRE(v4.user_agent == free.user_agent);
        REQUIRE(v4.model == free.model);
    }
}

TEST_CASE("kind, browser and platform narrow the draw", "[request]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.kind = dasmig::device_kind::tablet;
    for (std::uint64_t s = 0; s < 500; ++s) REQUIRE(gen.get_device(r, s).kind == "tablet");

    r = {};
    r.browser = "safari";
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE(d.browser == "safari");
        REQUIRE((d.os == "iOS" || d.os == "iPadOS" || d.os == "macOS"));
    }

    r = {};
    r.browser = "samsung";
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE(d.browser == "samsung");
        REQUIRE(d.maker == "Samsung");
    }

    r = {};
    r.platform = "other";
    const auto other = gen.get_device(r, 3);
    REQUIRE(other.kind == "other");
    REQUIRE(other.user_agent.empty());
    REQUIRE_FALSE(other.ip.empty());
}

TEST_CASE("safe devices get documentation addresses only", "[request][ip]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.safe = true;
    std::set<int> versions;
    for (std::uint64_t s = 0; s < 2000; ++s)
    {
        const auto d = gen.get_device(r, s);
        REQUIRE(d.reserved);
        REQUIRE(in_documentation_range(d.ip));
        versions.insert(d.ip_version);
    }
    REQUIRE(versions == std::set<int>{4, 6});

    r.ip = dasmig::ip_version::v6;
    REQUIRE(gen.get_device(r, 5).ip.starts_with("2001:db8:"));
}

TEST_CASE("old organisation blocks are not in the address data", "[ip]")
{
    // Defense, Apple, Ford, Daimler, UK Ministry of Defence, AMPRNet and other pre-2000 /8s.
    const std::set<int> organisation{6, 7, 11, 17, 19, 21, 22, 25, 26, 28, 29, 30, 33, 38, 53, 55, 214, 215};
    for (const auto& [country, blocks] : ref().ipv4)
    {
        for (const auto& [start, size] : blocks)
        {
            INFO(country << " " << (start >> 24U));
            REQUIRE_FALSE(organisation.contains(static_cast<int>(start >> 24U)));
            REQUIRE(start >> 24U != 10U);  // private
            REQUIRE(start >> 28U < 14U);   // below multicast
        }
    }
}

TEST_CASE("addresses are well formed and avoid .0 and .255", "[ip]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.ip = dasmig::ip_version::v4;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto ip = gen.get_device(r, s).ip;
        const auto last = std::stoi(ip.substr(ip.rfind('.') + 1));
        REQUIRE(last > 0);
        REQUIRE(last < 255);
    }
    r.ip = dasmig::ip_version::v6;
    for (std::uint64_t s = 0; s < 3000; ++s)
    {
        const auto ip = gen.get_device(r, s).ip;
        REQUIRE(ip.find(":::") == std::string::npos);
        REQUIRE(ip == [&] {
            std::string lower = ip;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return lower;
        }());
    }
}

TEST_CASE("accept-language from the locale", "[request]")
{
    const auto gen = make_loaded_generator();
    const auto lang = [&](std::string locale) {
        dasmig::device_request r;
        r.locale = std::move(locale);
        return gen.get_device(r, 1).accept_language;
    };
    REQUIRE(lang("") == "");
    REQUIRE(lang("en-US") == "en-US,en;q=0.9");
    REQUIRE(lang("pt_BR") == "pt-BR,pt;q=0.9,en;q=0.8");
    REQUIRE(lang("ja") == "ja,en;q=0.9");
    // The fallback entries are lower case, so English is not listed twice.
    REQUIRE(lang("EN-us") == "EN-us,en;q=0.9");
    REQUIRE(lang("PT-br") == "PT-br,pt;q=0.9,en;q=0.8");
    REQUIRE(lang("es-419") == "es-419,es;q=0.9,en;q=0.8");
}

TEST_CASE("unweighted draws are uniform over what is possible", "[request]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.weighted = false;
    std::map<std::string, int> platforms;
    for (std::uint64_t s = 0; s < 9000; ++s) ++platforms[gen.get_device(r, s).platform];
    REQUIRE(platforms.size() == 9);
    for (const auto& [key, n] : platforms) REQUIRE(n == Catch::Approx(1000).margin(150));
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

TEST_CASE("platform and browser shares follow the data", "[stats]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r;
    r.country = "US";
    std::map<std::string, int> platforms;
    std::map<std::string, int> windows_browsers;
    constexpr int draws = 20000;
    for (std::uint64_t s = 0; s < draws; ++s)
    {
        const auto d = gen.get_device(r, s);
        ++platforms[d.platform];
        if (d.os == "Windows") ++windows_browsers[d.browser];
    }
    REQUIRE(platforms["ios-phone"] == Catch::Approx(draws * 0.50).margin(draws * 0.02));
    REQUIRE(platforms["android-phone"] == Catch::Approx(draws * 0.30).margin(draws * 0.02));
    const double windows = platforms["windows-desktop"];
    REQUIRE(windows_browsers["chrome"] / windows == Catch::Approx(0.72 / 0.96).margin(0.04));
}

TEST_CASE("countries follow their internet users", "[stats]")
{
    const auto gen = make_loaded_generator();
    std::map<std::string, int> counts;
    for (std::uint64_t s = 0; s < 20000; ++s) ++counts[gen.get_device({}, s).country_code];
    REQUIRE(counts["CN"] > counts["US"]);
    REQUIRE(counts["IN"] > counts["US"]);
    REQUIRE(counts["US"] > counts["DE"]);
    REQUIRE(counts["DE"] > counts["PT"]);
}

TEST_CASE("newer versions dominate soon after release", "[stats]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2024} / 3 / 1);
    r.platform = "ios-phone";
    std::map<int, int> majors;
    for (std::uint64_t s = 0; s < 5000; ++s) ++majors[major_of(gen.get_device(r, s).os_version)];
    REQUIRE(majors[17] > majors[16]);
    REQUIRE(majors[16] > majors[15]);
    REQUIRE(majors[18] == 0); // iOS 18 came out in September 2024
}

// ---------------------------------------------------------------------------
// Conversion and JSON
// ---------------------------------------------------------------------------

TEST_CASE("string conversion and streaming give the summary", "[conversion]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_device({}, std::uint64_t{5});
    const std::string s = v;
    std::ostringstream os;
    os << v;
    REQUIRE(s == v.to_string());
    REQUIRE(os.str() == v.to_string());
    REQUIRE(s.find(v.ip) != std::string::npos);
}

TEST_CASE("the summary names the maker once", "[conversion]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request r = at(year{2025} / 6 / 1);
    int named_after_maker = 0;
    for (const auto* platform : {"android-phone", "android-tablet"})
    {
        r.platform = platform;
        for (std::uint64_t s = 0; s < 4000; ++s)
        {
            const auto d = gen.get_device(r, s);
            const auto summary = d.to_string();
            REQUIRE(summary.find("(" + d.maker + " " + d.maker + " ") == std::string::npos);
            if (d.model.starts_with(d.maker + " "))
            {
                ++named_after_maker;
                REQUIRE(summary.find("(" + d.model + ")") != std::string::npos); // "(OnePlus 12)"
            }
            else if (!d.model.empty())
            {
                REQUIRE(summary.find("(" + d.maker + " " + d.model + ")") != std::string::npos); // "(Samsung Galaxy A54)"
            }
        }
    }
    REQUIRE(named_after_maker > 0); // OnePlus, Honor and the Xiaomi Pad 5 are drawn
}

TEST_CASE("to_json and signature", "[json]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_device({}, std::uint64_t{99});
    REQUIRE(v.signature().seed == 99);
    REQUIRE(v.seed() == 99);
    REQUIRE(v.signature().data == gen.data_hash());
    const auto json = v.to_json();
    REQUIRE(json.starts_with(R"({"country_code":")"));
    REQUIRE(json.find(R"("user_agent":")") != std::string::npos);
    REQUIRE(json.find(R"("screen":{"width":)") != std::string::npos);
    REQUIRE(json.find(R"("signature":{"seed":"0000000000000063")") != std::string::npos);

    dasmig::core::json_writer w;
    w.begin_array();
    v.to_json(w);
    w.end_array();
    REQUIRE(w.str() == "[" + json + "]");
}

// ---------------------------------------------------------------------------
// Known answers (rule 2): the same seed and data give these exact results on
// every compiler and platform. A change here is a change of output and needs
// a major version.
// ---------------------------------------------------------------------------

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    dasmig::dvg gen;
    gen.load(lite_dir());
    REQUIRE(gen.has_data());
    REQUIRE(gen.data_hash() == 0xc9d3e7931dda7213ULL);

    REQUIRE(gen.get_device({}, std::uint64_t{1}).to_json() ==
            R"({"country_code":"DE","platform":"ios-phone","platform_label":"iPhone","kind":"phone","maker":"Apple","model":"iPhone 11","os":"iOS","os_version":"16.5","browser":"safari","browser_version":"16.5","engine":"WebKit","user_agent":"Mozilla/5.0 (iPhone; CPU iPhone OS 16_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.5 Mobile/15E148 Safari/604.1","accept_language":"","client_hints":{"sec_ch_ua":"","sec_ch_ua_mobile":"","sec_ch_ua_platform":""},"screen":{"width":414,"height":896,"pixel_ratio":2},"ip":"77.7.36.34","ip_version":4,"reserved":false,"signature":{"seed":"0000000000000001","data":"c9d3e7931dda7213"}})");
    dasmig::device_request r = at(year{2026} / 10 / 1);
    r.platform = "ios-phone";
    r.locale = "pt-BR";
    REQUIRE(gen.get_device(r, std::uint64_t{7}).to_json() ==
            R"({"country_code":"PK","platform":"ios-phone","platform_label":"iPhone","kind":"phone","maker":"Apple","model":"iPhone 15 Plus","os":"iOS","os_version":"26.7","browser":"safari","browser_version":"26.7","engine":"WebKit","user_agent":"Mozilla/5.0 (iPhone; CPU iPhone OS 18_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.7 Mobile/15E148 Safari/604.1","accept_language":"pt-BR,pt;q=0.9,en;q=0.8","client_hints":{"sec_ch_ua":"","sec_ch_ua_mobile":"","sec_ch_ua_platform":""},"screen":{"width":430,"height":932,"pixel_ratio":3},"ip":"125.209.109.2","ip_version":4,"reserved":false,"signature":{"seed":"0000000000000007","data":"c9d3e7931dda7213"}})");
    r = at(year{2021} / 6 / 1);
    r.platform = "windows-desktop";
    r.safe = true;
    REQUIRE(gen.get_device(r, std::uint64_t{3}).to_json() ==
            R"({"country_code":"IN","platform":"windows-desktop","platform_label":"Windows PC","kind":"desktop","maker":"HP","model":"","os":"Windows","os_version":"10","browser":"chrome","browser_version":"91","engine":"Blink","user_agent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.77 Safari/537.36","accept_language":"","client_hints":{"sec_ch_ua":"","sec_ch_ua_mobile":"","sec_ch_ua_platform":""},"screen":{"width":1707,"height":960,"pixel_ratio":1.5},"ip":"2001:db8:179:a0f2:ada2:1e3d:d4bf:77a","ip_version":6,"reserved":true,"signature":{"seed":"0000000000000003","data":"c9d3e7931dda7213"}})");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 10000; ++s)
    {
        for (const char c : gen.get_device({}, s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0xba9c1bdff4039135ULL);
}

// ---------------------------------------------------------------------------
// Extending data (rule 8)
// ---------------------------------------------------------------------------

TEST_CASE("extend adds models and changes shares", "[extend]")
{
    auto gen = make_loaded_generator();
    const auto base_hash = gen.data_hash();
    const auto before = gen.get_device({}, std::uint64_t{7});

    // A new phone that is the only Samsung model.
    const auto& info = gen.extend_from_string(
        dasmig::device_table::models,
        "key\tmaker\tplatform\tmodel\treleased\tos_first\tos_last\twidth\theight\tpixel_ratio\tua_model\tweight\n"
        "test-phone\tSamsung\tandroid-phone\tTest Phone\t2022-01-01\t12\t\t400\t900\t2.5\tSM-TEST\t1000000\n",
        "models-layer.tsv");
    REQUIRE(info.added == 1);
    REQUIRE(gen.data_hash() != base_hash);
    REQUIRE(gen.layers(dasmig::device_table::models).size() == 2);

    dasmig::device_request r = at(year{2022} / 6 / 1);
    r.country = "KR";
    r.platform = "android-phone";
    int test_phones = 0;
    for (std::uint64_t s = 0; s < 200; ++s)
    {
        const auto d = gen.get_device(r, s);
        if (d.model == "Test Phone")
        {
            ++test_phones;
            REQUIRE(d.screen.width == 400);
            REQUIRE(major_of(d.os_version) >= 12);
        }
    }
    REQUIRE(test_phones > 150);

    // A country that only uses Linux.
    gen.extend_from_string(dasmig::device_table::platform_shares, "country\tkey\tweight\nPT\tlinux-desktop\t100\n", "shares.tsv");
    for (const auto& p : gen.platforms())
    {
        if (p == "linux-desktop") continue;
        gen.extend_from_string(dasmig::device_table::platform_shares, "country\tkey\tweight\nPT\t" + p + "\t0\n", "zero.tsv");
    }
    r = {};
    r.country = "PT";
    for (std::uint64_t s = 0; s < 100; ++s) REQUIRE(gen.get_device(r, s).platform == "linux-desktop");

    // Reloading the base drops every layer.
    REQUIRE(gen.load(dasmig::dataset::lite));
    REQUIRE(gen.data_hash() == base_hash);
    REQUIRE(gen.get_device({}, std::uint64_t{7}).to_json() == before.to_json());
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    using dasmig::device_table;

    REQUIRE_THROWS_AS(gen.extend_from_string(device_table::ipv4, "country_code\tstart\tsize\nPT\t1.2.3\t256\n", "a"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(device_table::ipv6, "country_code\tprefix\tlength\nPT\t2001:zz::\t32\n",
                                             "b"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(device_table::releases, "product\tversion\treleased\nios\t99\t2026-13-01\n",
                                             "c"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(device_table::countries,
                                             "country_code\tpopulation\tinternet_share\tipv6_share\nPT\t1\t1.5\t0\n", "d"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(device_table::platforms,
                                             "key\tlabel\tdevice\tos\tweight\nx\tX\twatch\tWearOS\t1\n", "e"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend(std::filesystem::path{"not-a-table.tsv"}), std::invalid_argument);

    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.layers(device_table::ipv4).size() == 1);
}

#ifdef DASMIG_DEVICE_EMBED
TEST_CASE("compiled-in data needs no files and matches them", "[embed]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto files = make_loaded_generator();
    const auto tmp = std::filesystem::temp_directory_path() / "dvg_embed_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::dvg gen;
    const bool loaded = gen.load(dasmig::dataset::lite);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);

    REQUIRE(loaded);
    REQUIRE(gen.layers(dasmig::device_table::countries).front().source == "embedded:lite/countries.tsv");
    REQUIRE(gen.data_hash() == files.data_hash());
    REQUIRE(gen.get_device({}, std::uint64_t{1}).to_json() == files.get_device({}, std::uint64_t{1}).to_json());
}
#endif

// ---------------------------------------------------------------------------
// Batch generation
// ---------------------------------------------------------------------------

TEST_CASE("device batch: items equal the single calls for any thread count", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = dasmig::device_request{};
    const auto batch = gen.get_device_batch(request, {.count = 200, .threads = 1}, 77);
    REQUIRE(batch.size() == 200);
    for (std::uint64_t i = 0; i < batch.size(); ++i)
    {
        REQUIRE(batch[i].to_json() == gen.get_device(request, dasmig::core::item_seed(77, i)).to_json());
    }

    const auto tail = gen.get_device_batch(request, {.count = 100, .first = 100, .threads = 4}, 77);
    REQUIRE(tail.size() == 100);
    for (std::size_t i = 0; i < tail.size(); ++i) REQUIRE(tail[i].to_json() == batch[100 + i].to_json());
    for (const std::size_t threads : {2U, 7U, 0U})
    {
        const auto other = gen.get_device_batch(request, {.count = 200, .threads = threads}, 77);
        REQUIRE(other.size() == batch.size());
        for (std::size_t i = 0; i < other.size(); ++i) REQUIRE(other[i].to_json() == batch[i].to_json());
    }
    REQUIRE(gen.get_device_batch(request, {.count = 5}).size() == 5);
}

TEST_CASE("device batch: streaming gives the same items in order", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = dasmig::device_request{};
    const auto batch = gen.get_device_batch(request, {.count = 300, .first = 10, .threads = 3}, 5);
    std::uint64_t next = 10;
    std::size_t seen = 0;
    gen.for_each_device(request, {.count = 300, .first = 10, .threads = 3}, 5,
                        [&](std::uint64_t index, const dasmig::device& item) {
                            REQUIRE(index == next);
                            REQUIRE(item.to_json() == batch[seen].to_json());
                            ++next;
                            ++seen;
                        });
    REQUIRE(seen == batch.size());
}

TEST_CASE("device batch: unique by IP", "[batch]")
{
    const auto gen = make_loaded_generator();
    dasmig::device_request request;
    request.safe = true;
    request.ip = dasmig::ip_version::v4; // 762 usable documentation addresses
    const auto batch = gen.get_device_batch(
        request,
        {.count = 300, .threads = 2,
         .unique = dasmig::core::unique_rule::by<dasmig::device>([](const dasmig::device& d) { return d.ip; })},
        9);
    std::set<std::string> ips;
    for (const auto& d : batch) ips.insert(d.ip);
    REQUIRE(ips.size() == batch.size());
}

TEST_CASE("device batch: known answer of 10,000 items", "[batch][kat]")
{
    auto gen = make_loaded_generator();
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    gen.for_each_device({}, {.count = 10000, .threads = 4}, 1, [&](std::uint64_t, const dasmig::device& item) {
        hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(item.to_json()));
    });
    REQUIRE(hash == 0x55ce7e25df3f1bc2ULL);
}
