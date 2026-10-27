// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Diego Dasso Migotto

#ifndef DASMIG_DEVICEGEN_HPP
#define DASMIG_DEVICEGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <format>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef DASMIG_DEVICE_EMBED
#include "devicegen_data.hpp"
#endif

/// @file devicegen.hpp
/// @brief Device Generator library — devices, browsers, user agents and IP addresses that fit together for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 1);

namespace dasmig
{

/// @brief The form of a device.
enum class device_kind : std::uint8_t
{
    phone,   ///< A phone.
    tablet,  ///< A tablet.
    desktop, ///< A desktop or laptop computer.
    other    ///< Anything else (no browser is modelled for it).
};

/// @brief Which IP family a device gets.
enum class ip_version : std::uint8_t
{
    any, ///< IPv6 with the country's IPv6 share, otherwise IPv4.
    v4,  ///< Always IPv4.
    v6   ///< IPv6 where the country has prefixes, otherwise IPv4.
};

/// @brief What a device call asks for.
///
/// Every generation call takes one; `device_request{}` asks for the
/// defaults. Choices are never stored on the generator, so one call cannot
/// change the next.
struct device_request
{
    /// @brief The default reference date, 2023-07-01 (birth-generator's and
    ///        user-generator's); the library never reads the clock.
    static constexpr std::chrono::year_month_day default_reference_date{
        std::chrono::year{2023}, std::chrono::month{7}, std::chrono::day{1}};

    /// @brief The earliest reference date the data covers.
    static constexpr std::chrono::year_month_day earliest_reference_date{
        std::chrono::year{2021}, std::chrono::month{1}, std::chrono::day{1}};

    /// @brief ISO 3166-1 alpha-2 code (any case). Unset draws a country by
    ///        its number of internet users.
    std::optional<std::string> country{}; // NOLINT(readability-redundant-member-init)

    /// @brief A platform key (`android-phone`, `ios-phone`, `windows-desktop`,
    ///        ...; user-generator's keys). Fixes the kind and the OS; unset
    ///        draws one by the country's shares.
    std::optional<std::string> platform{}; // NOLINT(readability-redundant-member-init)

    /// @brief Narrow the platform draw to one kind. Ignored when it agrees
    ///        with a given platform; an error when it does not.
    std::optional<device_kind> kind{}; // NOLINT(readability-redundant-member-init)

    /// @brief Force a browser by key (`chrome`, `safari`, `edge`, `firefox`,
    ///        `samsung`). The platform, when drawn, is one that has it.
    std::optional<std::string> browser{}; // NOLINT(readability-redundant-member-init)

    /// @brief A BCP 47 locale (`pt-BR`) for the `Accept-Language` header;
    ///        empty leaves the header empty.
    std::string locale{}; // NOLINT(readability-redundant-member-init)

    /// @brief The date the device is seen at: models, OS and browser versions
    ///        are those of that date. Must not be before
    ///        earliest_reference_date; a date after the data's newest release
    ///        is seen as that release's date.
    std::chrono::year_month_day reference_date{default_reference_date};

    /// @brief The IP family.
    ip_version ip{ip_version::any};

    /// @brief Give an address from the documentation ranges (RFC 5737,
    ///        RFC 3849) instead of a registered one.
    bool safe{false};

    /// @brief Draw by the data's shares (true) or uniformly among what is
    ///        possible (false).
    bool weighted{true};
};

/// @brief A screen in CSS pixels.
struct device_screen
{
    int width{0};             ///< Width in CSS pixels (portrait for phones and tablets).
    int height{0};            ///< Height in CSS pixels.
    double pixel_ratio{0.0};  ///< Device pixels per CSS pixel.
};

/// @brief Return type for device generation.
///
/// Supports implicit conversion to std::string (a one-line summary),
/// streaming via operator<< and JSON output via to_json().
class device
{
  public:
    std::string country_code;    ///< ISO 3166-1 alpha-2 code.
    std::string platform;        ///< Platform key (`android-phone`, ...).
    std::string platform_label;  ///< Display label of the platform.
    std::string kind;            ///< `phone`, `tablet`, `desktop` or `other`.
    std::string maker;           ///< Maker (`Samsung`, `Apple`, `Lenovo`); may be empty.
    std::string model;           ///< Model name (`Galaxy A54`); empty for most desktops.
    std::string os;              ///< Operating system (`Android`, `iOS`, `Windows`, ...).
    std::string os_version;      ///< OS version (`14`, `17.5`, `10.15`); empty where none is modelled.
    std::string browser;         ///< Browser key (`chrome`, `safari`, ...); empty for `other`.
    std::string browser_version; ///< Browser version (`126`, `17.5`, `25.0`).
    std::string engine;          ///< `Blink`, `WebKit` or `Gecko`.
    std::string user_agent;      ///< The `User-Agent` header this device sends.
    std::string accept_language; ///< The `Accept-Language` header; empty without a locale.
    device_screen screen;        ///< The screen.
    std::string ip;              ///< IP address (dotted quad or RFC 5952 text).
    int ip_version{4};           ///< 4 or 6.
    bool reserved{false};        ///< The address is from a documentation range.

    /// @brief Retrieve the random seed used to generate this device.
    /// @return The call seed; pass it to get_device(request, seed) with
    ///         the same request and data to replay this device.
    [[nodiscard]] std::uint64_t seed() const { return _signature.seed; }

    /// @brief The seed and the data this device was generated from.
    /// @return The replay signature; `data` is dvg::data_hash() at the
    ///         time of generation.
    [[nodiscard]] const core::seed_signature& signature() const { return _signature; }

    /// @brief Human-readable summary string.
    /// @return For example `Chrome 126 on Android 14 (Samsung Galaxy A54), 203.0.113.7`.
    [[nodiscard]] std::string to_string() const
    {
        std::string out = browser.empty() ? platform_label : browser_name(browser) + " " + browser_version;
        if (!browser.empty())
        {
            out += " on " + os;
            if (!os_version.empty()) out += " " + os_version;
        }
        std::string who = maker;
        if (!model.empty()) who += who.empty() ? model : " " + model;
        if (!who.empty()) out += " (" + who + ")";
        if (!ip.empty()) out += ", " + ip;
        return out;
    }

    /// @brief Write this device as a JSON object.
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("country_code", country_code)
            .field("platform", platform)
            .field("platform_label", platform_label)
            .field("kind", kind)
            .field("maker", maker)
            .field("model", model)
            .field("os", os)
            .field("os_version", os_version)
            .field("browser", browser)
            .field("browser_version", browser_version)
            .field("engine", engine)
            .field("user_agent", user_agent)
            .field("accept_language", accept_language)
            .key("screen")
            .begin_object()
            .field("width", screen.width)
            .field("height", screen.height)
            .field("pixel_ratio", screen.pixel_ratio)
            .end_object()
            .field("ip", ip)
            .field("ip_version", ip_version)
            .field("reserved", reserved)
            .key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This device as a JSON document.
    /// @return Compact JSON.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return Summary string.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return to_string();
    }

    /// @brief Stream the summary to an output stream.
    /// @param os Output stream.
    /// @param v The device to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const device& v)
    {
        os << v.to_string();
        return os;
    }

    /// @brief The display name of a browser key (`chrome` gives `Chrome`).
    /// @param key A browser key.
    /// @return The name, or the key itself when it is not one of the shipped browsers.
    [[nodiscard]] static std::string browser_name(std::string_view key)
    {
        if (key == "chrome") return "Chrome";
        if (key == "safari") return "Safari";
        if (key == "edge") return "Edge";
        if (key == "firefox") return "Firefox";
        if (key == "samsung") return "Samsung Internet";
        return std::string{key};
    }

  private:
    core::seed_signature _signature;
    friend class dvg;
};

/// @brief The data tables of device-generator. Each is a TSV file in a tier
///        directory and can be extended with layers.
enum class device_table : std::uint8_t
{
    countries,       ///< `countries.tsv`: population, internet and IPv6 shares.
    platforms,       ///< `platforms.tsv`: the platforms and their global shares.
    platform_shares, ///< `platform_shares.tsv`: per-country shares, in percent.
    maker_shares,    ///< `maker_shares.tsv`: makers by country and platform.
    models,          ///< `models.tsv`: models, release dates, OS range, screen.
    releases,        ///< `releases.tsv`: OS and browser versions with dates.
    adoption,        ///< `adoption.tsv`: how fast versions and models spread.
    version_shares,  ///< `version_shares.tsv`: explicit OS version shares by date.
    requirements,    ///< `requirements.tsv`: the oldest OS a browser version runs on.
    browser_shares,  ///< `browser_shares.tsv`: browsers by OS and country.
    ua_templates,    ///< `ua_templates.tsv`: user-agent formats.
    screens,         ///< `screens.tsv`: screen sizes where the model has none.
    ipv4,            ///< `ipv4.tsv`: IPv4 blocks by country.
    ipv6,            ///< `ipv6.tsv`: IPv6 prefixes by country.
    reserved         ///< `reserved.tsv`: the documentation ranges.
};

/// @brief Device Generator: devices whose model, OS, browser, user agent,
///        screen and IP address fit together at a date.
///
/// A call draws, from streams split from the call seed: the country
/// (`"country"`), the platform (`"platform"`), the maker and model
/// (`"model"`), the OS version (`"os-version"`), the browser (`"browser"`)
/// and its version (`"browser-version"`), the screen (`"screen"`), the IP
/// family (`"ip-version"`) and the address (`"ip"`). Fixing one in the request
/// leaves the others' draws unchanged. Output is identical on every compiler
/// and platform (dasmig-core).
///
/// Can be used as a singleton via instance() or constructed independently.
///
/// @par Thread safety
/// Each instance is independent. Concurrent calls to get_device(request) on
/// the **same** instance require external synchronisation; the seeded
/// overload is const and safe to call concurrently.
class dvg
{
  public:
    /// @brief Default constructor — creates an empty generator with no data.
    dvg() = default;

    dvg(const dvg&) = delete;                 ///< Not copyable.
    dvg& operator=(const dvg&) = delete;      ///< Not copyable.
    dvg(dvg&&) noexcept = default;            ///< Movable.
    dvg& operator=(dvg&&) noexcept = default; ///< Movable.
    ~dvg() = default;                         ///< Destructor.

    /// @brief Access the global singleton instance.
    ///
    /// Loads the `lite` tier (or `full` if `lite` is missing) on first
    /// access, from compiled-in data or an auto-probed `resources/`.
    /// @return The singleton.
    static dvg& instance()
    {
        static dvg inst{auto_probe_tag{}};
        return inst;
    }

    // -- Generation -------------------------------------------------------

    /// @brief Generate a random device.
    ///        Use device_request{} for the defaults.
    /// @param request What to generate.
    /// @return The device.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the request is invalid (see the
    ///         seeded overload).
    [[nodiscard]] device get_device(const device_request& request) { return get_device(request, draw_seed_()); }

    /// @brief Generate a deterministic device.
    /// @param request What to generate.
    /// @param call_seed Seed; the same request, seed and data give the same
    ///        result. Does not touch the instance engine.
    /// @return The device.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the reference date is before
    ///         device_request::earliest_reference_date or not a valid date, the
    ///         country, platform or browser is unknown, the kind contradicts
    ///         the platform, the browser does not run on the platform, or
    ///         the country has no registered addresses (unless `safe`).
    [[nodiscard]] device get_device(const device_request& request, std::uint64_t call_seed) const
    {
        require_data_();
        // A date after the newest release in the data is seen as that date.
        const int day = std::min(check_date_(request.reference_date), _model.as_of);
        const core::rng call{call_seed};

        device d;
        d._signature = {.seed = call_seed, .data = data_hash()};

        const auto& country = pick_country_(request, call);
        d.country_code = country.code;
        const platform_row& p = pick_platform_(request, country.code, call);
        d.platform = p.key;
        d.platform_label = p.label;
        d.kind = p.device;
        d.os = p.os;

        auto model_stream = call.split("model");
        d.maker = pick_maker_(request, country.code, p, day, model_stream);
        const model_row* mdl = pick_model_(request, p, d.maker, day, model_stream);
        if (mdl != nullptr) d.model = mdl->model;

        const release* os_release = pick_os_version_(request, p, mdl, day, call);
        if (os_release != nullptr)
        {
            d.os_version = os_release->version;
            if (has_minor_versions_(p.os)) d.os_version = with_minor_(*os_release, day, call.split("os-minor"));
        }

        std::string ua_full;
        if (p.os != "other")
        {
            const auto browser = pick_browser_(request, country.code, p, d.maker, d.os_version, day, call);
            d.browser = browser.key;
            d.browser_version = browser.version;
            ua_full = browser.full;
            d.engine = engine_(d.browser, p.os);
            d.user_agent = render_ua_(d, p, mdl, os_release, ua_full);
        }
        d.accept_language = accept_language_(request.locale);
        d.screen = pick_screen_(request, country.code, p, mdl, call);
        pick_ip_(request, country, call, d);
        return d;
    }

    // -- Batch generation -------------------------------------------------

    /// @brief Generate a batch of devices from a seed drawn like
    ///        get_device() draws its own.
    ///
    /// Item `i` is `get_device(request, core::item_seed(batch_seed, options.first + i))`
    /// for the drawn batch seed; see the seeded overload.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @return The results, in index order.
    [[nodiscard]] std::vector<device> get_device_batch(const device_request& request,
                                                       const core::batch_options& options)
    {
        return get_device_batch(request, options, draw_seed_());
    }

    /// @brief Generate a batch of devices, deterministically.
    ///
    /// Item `i` is exactly `get_device(request, core::item_seed(batch_seed,
    /// options.first + i))`, so an item replays on its own, a range can be
    /// generated directly, two batches that follow each other equal one, and
    /// the output does not depend on `options.threads`.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness (see
    ///        core::unique_rule; the rule must be made for `device`, for
    ///        example keyed by the IP).
    /// @param batch_seed Seed of the whole batch.
    /// @return The results, in index order.
    /// @throws std::invalid_argument If the request or options are invalid.
    /// @throws std::runtime_error If no data has been loaded, or a unique
    ///         batch runs out of attempts.
    [[nodiscard]] std::vector<device> get_device_batch(const device_request& request,
                                                       const core::batch_options& options,
                                                       std::uint64_t batch_seed) const
    {
        return core::run_batch(options, batch_seed,
                               [&](std::uint64_t item_seed) { return get_device(request, item_seed); });
    }

    /// @brief Stream a batch of devices to a sink without holding the whole
    ///        batch.
    ///
    /// Generates the same items as get_device_batch() and calls
    /// `sink(index, item)` in index order on the calling thread; `index`
    /// includes `options.first`.
    ///
    /// @tparam Sink Callable taking `(std::uint64_t, device&)`; for example
    ///         core::jsonl_writer or core::csv_writer.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param batch_seed Seed of the whole batch.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_device(const device_request& request, const core::batch_options& options,
                         std::uint64_t batch_seed, Sink&& sink) const
    {
        core::for_each_batch(options, batch_seed,
                             [&](std::uint64_t item_seed) { return get_device(request, item_seed); },
                             std::forward<Sink>(sink));
    }

    /// @brief Stream a batch of devices with a batch seed drawn like
    ///        get_device() draws its own.
    /// @tparam Sink Callable taking `(std::uint64_t, device&)`.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_device(const device_request& request, const core::batch_options& options, Sink&& sink)
    {
        for_each_device(request, options, draw_seed_(), std::forward<Sink>(sink));
    }

    // -- Queries ----------------------------------------------------------

    /// @brief The countries a request can name: those with a row in
    ///        `countries.tsv`, sorted.
    /// @return ISO 3166-1 alpha-2 codes.
    [[nodiscard]] std::vector<std::string> countries() const
    {
        std::vector<std::string> out;
        out.reserve(_model.countries.size());
        for (const auto& c : _model.countries) out.push_back(c.code);
        return out;
    }

    /// @brief The platform keys a request can name, in table order.
    /// @return The keys.
    [[nodiscard]] std::vector<std::string> platforms() const
    {
        std::vector<std::string> out;
        out.reserve(_model.platforms.size());
        for (const auto& p : _model.platforms) out.push_back(p.key);
        return out;
    }

    /// @brief The number of countries loaded.
    /// @return The count.
    [[nodiscard]] std::size_t country_count() const { return _model.countries.size(); }

    /// @brief The number of device models loaded.
    /// @return The count.
    [[nodiscard]] std::size_t model_count() const { return _model.models.size(); }

    // -- Data management --------------------------------------------------

    /// @brief Check whether the data needed to generate is loaded.
    /// @return True when countries and platforms are loaded.
    [[nodiscard]] bool has_data() const { return !_model.countries.empty() && !_model.platforms.empty(); }

    /// @brief Load data from a resource directory, replacing all data and
    ///        layers loaded before.
    ///
    /// The directory must contain `countries.tsv` and `platforms.tsv`; the
    /// other tables are read when present. A path without them is ignored.
    ///
    /// @param dir Directory such as `resources/lite`.
    /// @throws core::tsv_error If a file is malformed; the data is then
    ///         unchanged.
    void load(const std::filesystem::path& dir)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(device_table::countries), ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(device_table::platforms), ec))
        {
            return;
        }
        apply_(read_directory_(dir), core::layer_mode::replace, true);
    }

    /// @brief Load a dataset tier, replacing all data and layers loaded
    ///        before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_DEVICE_EMBED`, otherwise searches `resources/`, `../resources`,
    /// `device-generator/resources` and `../device-generator/resources`.
    ///
    /// @param tier The tier.
    /// @return False when the tier was not found.
    /// @throws core::tsv_error If a file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string tier_dir{core::tier_name(tier)};
        const auto marker = tier_dir + "/" + std::string{file_name_(device_table::countries)};
        const auto root = core::find_resources(library_name_, marker);
        documents docs;
        for (const auto t : all_tables_)
        {
            docs.at(index_(t)) =
                core::load_resource(tier_dir + "/" + std::string{file_name_(t)}, embedded_files_(), root);
        }
        if (!docs.at(index_(device_table::countries)) || !docs.at(index_(device_table::platforms))) return false;
        apply_(std::move(docs), core::layer_mode::replace, true);
        return true;
    }

    /// @brief Add data layers from a directory or a file.
    ///
    /// A directory may hold any of the table files (see device_table); all
    /// are applied together or, if any fails, none. A file must have one of
    /// those names.
    ///
    /// @param path Directory or file.
    /// @param mode How the layers combine with the data loaded before.
    /// @throws core::tsv_error If a file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    /// @throws std::invalid_argument If @p path is a file with another name.
    void extend(const std::filesystem::path& path, core::layer_mode mode = core::layer_mode::override)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec))
        {
            apply_(read_directory_(path), mode, false);
            return;
        }
        for (const auto t : all_tables_)
        {
            if (path.filename() == file_name_(t))
            {
                (void)extend(t, core::tsv_document::load(path), mode);
                return;
            }
        }
        throw std::invalid_argument("extend: expected one of the device tables (countries.tsv, platforms.tsv, "
                                    "models.tsv, ...), got " +
                                    path.filename().string());
    }

    /// @brief Add a data layer to one table.
    ///
    /// Rows are keyed by the table's key columns (see doc/data.md). In the
    /// default override mode a row replaces the loaded row with the same key
    /// and new keys are added; a row with `disabled` set to 1 removes its key.
    ///
    /// @param which The table.
    /// @param doc TSV with that table's columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did (rows added, replaced, removed, warnings).
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend(device_table which, core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        documents docs;
        docs.at(index_(which)).emplace(std::move(doc));
        apply_(std::move(docs), mode, false);
        return _tables.at(index_(which)).layers().back();
    }

    /// @brief Add a data layer to one table from TSV text (for example from a
    ///        WebAssembly host).
    /// @param which The table.
    /// @param text TSV with that table's columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(device_table which, std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(which, core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief The layers in effect for one table, in order.
    /// @param which The table.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers(device_table which) const
    {
        return _tables.at(index_(which)).layers();
    }

    /// @brief Hash of the loaded data, recorded in each result's signature().
    /// @return A hash that changes when any table does; 0 when nothing is
    ///         loaded.
    [[nodiscard]] std::uint64_t data_hash() const
    {
        std::uint64_t h = 0;
        bool any = false;
        for (const auto& t : _tables)
        {
            any = any || !t.layers().empty();
            h = core::hash_combine(h, t.data_hash());
        }
        return any ? h : 0;
    }

  private:
    using table = device_table;

    // -- Tables -----------------------------------------------------------

    static constexpr std::string_view library_name_ = "device-generator";
    static constexpr std::size_t table_count_ = 15;
    static constexpr std::array<table, table_count_> all_tables_{
        table::countries,      table::platforms,    table::platform_shares, table::maker_shares,
        table::models,         table::releases,     table::adoption,        table::version_shares,
        table::requirements,   table::browser_shares, table::ua_templates,  table::screens,
        table::ipv4,           table::ipv6,         table::reserved};

    using documents = std::array<std::optional<core::tsv_document>, table_count_>;
    using tables = std::array<core::layered_table, table_count_>;

    static constexpr std::size_t index_(table t) { return static_cast<std::size_t>(t); }

    static constexpr std::string_view file_name_(table t)
    {
        switch (t)
        {
        case table::countries: return "countries.tsv";
        case table::platforms: return "platforms.tsv";
        case table::platform_shares: return "platform_shares.tsv";
        case table::maker_shares: return "maker_shares.tsv";
        case table::models: return "models.tsv";
        case table::releases: return "releases.tsv";
        case table::adoption: return "adoption.tsv";
        case table::version_shares: return "version_shares.tsv";
        case table::requirements: return "requirements.tsv";
        case table::browser_shares: return "browser_shares.tsv";
        case table::ua_templates: return "ua_templates.tsv";
        case table::screens: return "screens.tsv";
        case table::ipv4: return "ipv4.tsv";
        case table::ipv6: return "ipv6.tsv";
        case table::reserved: return "reserved.tsv";
        }
        return "";
    }

    static core::table_schema schema_(table t)
    {
        switch (t)
        {
        case table::countries:
            return {.key = {"country_code"}, .required = {"population", "internet_share", "ipv6_share"}, .optional = {}};
        case table::platforms:
            return {.key = {"key"}, .required = {"label", "device", "os", "weight"}, .optional = {}};
        case table::platform_shares: return {.key = {"country", "key"}, .required = {"weight"}, .optional = {}};
        case table::maker_shares:
            return {.key = {"country", "platform", "maker"}, .required = {"weight"}, .optional = {}};
        case table::models:
            return {.key = {"key"},
                    .required = {"maker", "platform", "model", "released", "os_first", "weight"},
                    .optional = {"os_last", "width", "height", "pixel_ratio", "ua_model"}};
        case table::releases: return {.key = {"product", "version"}, .required = {"released"}, .optional = {"full"}};
        case table::adoption:
            return {.key = {"product"}, .required = {"ramp_days", "half_life_days"}, .optional = {}};
        case table::version_shares:
            return {.key = {"product", "version", "date"}, .required = {"share"}, .optional = {}};
        case table::requirements:
            return {.key = {"browser", "os", "from_version"}, .required = {"min_os"}, .optional = {}};
        case table::browser_shares:
            return {.key = {"country", "os", "browser"}, .required = {"weight"}, .optional = {}};
        case table::ua_templates:
            return {.key = {"browser", "os", "min_browser", "min_os"}, .required = {"template"}, .optional = {}};
        case table::screens:
            return {.key = {"country", "os", "width", "height", "pixel_ratio"}, .required = {"weight"}, .optional = {}};
        case table::ipv4: return {.key = {"country_code", "start"}, .required = {"size"}, .optional = {}};
        case table::ipv6: return {.key = {"country_code", "prefix"}, .required = {"length"}, .optional = {}};
        case table::reserved: return {.key = {"version", "prefix"}, .required = {"length"}, .optional = {}};
        }
        return {};
    }

    static tables make_tables_()
    {
        return {core::layered_table{schema_(table::countries)},     core::layered_table{schema_(table::platforms)},
                core::layered_table{schema_(table::platform_shares)}, core::layered_table{schema_(table::maker_shares)},
                core::layered_table{schema_(table::models)},        core::layered_table{schema_(table::releases)},
                core::layered_table{schema_(table::adoption)},      core::layered_table{schema_(table::version_shares)},
                core::layered_table{schema_(table::requirements)},  core::layered_table{schema_(table::browser_shares)},
                core::layered_table{schema_(table::ua_templates)},  core::layered_table{schema_(table::screens)},
                core::layered_table{schema_(table::ipv4)},          core::layered_table{schema_(table::ipv6)},
                core::layered_table{schema_(table::reserved)}};
    }

    // -- Parsed rows ------------------------------------------------------

    using version_parts = std::vector<int>;

    struct country_row
    {
        std::string code;
        double users{0.0}; // population times internet share
        double ipv6_share{0.0};
    };

    struct platform_row
    {
        std::string key;
        std::string label;
        std::string device;
        std::string os;
        double weight{0.0};
    };

    struct model_row
    {
        std::string key;
        std::string maker;
        std::string platform;
        std::string model;
        int released{0};
        version_parts os_first;
        version_parts os_last; // empty: no upper bound
        device_screen screen;
        std::string ua_model;
        double weight{0.0};
    };

    struct release
    {
        std::string version;
        version_parts parts;
        int released{0};
        std::string full;
    };

    struct adoption_row
    {
        double ramp{0.0};
        double half_life{0.0};
    };

    struct requirement
    {
        int from{0};
        version_parts min_os;
    };

    struct ua_template
    {
        int min_browser{0};
        int min_os{0};
        std::string text;
    };

    struct screen_row
    {
        device_screen screen;
        double weight{0.0};
    };

    struct ipv4_set
    {
        std::vector<std::uint32_t> start;
        std::vector<std::uint64_t> size;
        std::optional<core::discrete> by_size;
    };

    struct ipv6_net
    {
        std::uint64_t hi{0};
        std::uint64_t lo{0};
        int length{0};
    };

    struct ipv6_set
    {
        std::vector<ipv6_net> nets;
        std::optional<core::discrete> by_size;
    };

    using weighted_keys = std::vector<std::pair<std::string, double>>;
    template <typename V> using string_map = std::map<std::string, V, std::less<>>;

    struct model
    {
        std::vector<country_row> countries;           // sorted by code
        string_map<std::size_t> country_index;        // code -> countries index
        std::vector<std::size_t> drawable;            // countries with IPv4 blocks
        std::optional<core::discrete> country_pick;   // over drawable, by users
        std::vector<platform_row> platforms;
        string_map<weighted_keys> platform_shares;    // country -> key, percent
        string_map<weighted_keys> maker_shares;       // country \t platform -> maker, weight
        std::vector<model_row> models;
        string_map<std::vector<std::size_t>> models_by; // platform \t maker -> model indices
        string_map<std::vector<release>> releases;    // product -> by date
        int as_of{std::numeric_limits<int>::max()};   // the newest release day
        string_map<adoption_row> adoption;
        string_map<string_map<std::vector<std::pair<int, double>>>> version_shares; // product -> version -> (day, share)
        string_map<std::vector<requirement>> requirements; // browser \t os
        string_map<weighted_keys> browser_shares;     // country \t os -> browser, weight
        string_map<std::vector<ua_template>> ua_templates; // browser \t os
        string_map<std::vector<screen_row>> screens;  // country \t os
        string_map<ipv4_set> ipv4;
        string_map<ipv6_set> ipv6;
        std::vector<std::pair<std::uint32_t, int>> reserved4; // start, length
        std::vector<ipv6_net> reserved6;
    };

    // -- Row parsing --------------------------------------------------------

    static double weight_(const core::tsv_row& row, std::string_view column = "weight")
    {
        const auto w = row.get<double>(column);
        if (!(w >= 0.0) || !std::isfinite(w))
            throw row.error(column, "must be a finite number, not negative");
        return w;
    }

    static double share_(const core::tsv_row& row, std::string_view column)
    {
        const auto s = row.get<double>(column);
        if (std::isnan(s) || s < 0.0 || s > 1.0) throw row.error(column, "must be between 0 and 1");
        return s;
    }

    static std::string country_or_any_(const core::tsv_row& row, std::string_view column = "country")
    {
        auto c = row.get<std::string>(column);
        if (c != "*" && !core::is_alpha2(c))
            throw row.error(column, "must be * or an upper case ISO 3166-1 alpha-2 code");
        return c;
    }

    static std::string country_(const core::tsv_row& row)
    {
        auto c = row.get<std::string>("country_code");
        if (!core::is_alpha2(c)) throw row.error("country_code", "must be an upper case ISO 3166-1 alpha-2 code");
        return c;
    }

    static std::string nonempty_(const core::tsv_row& row, std::string_view column)
    {
        auto text = row.get<std::string>(column);
        if (text.empty()) throw row.error(column, "must not be empty");
        return text;
    }

    // Days since 1970-01-01 of an ISO 8601 date, or nullopt.
    static std::optional<int> parse_day_(std::string_view text)
    {
        if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
        const auto num = [&](std::size_t at, std::size_t n) -> std::optional<int> {
            int v = 0;
            for (std::size_t i = at; i < at + n; ++i)
            {
                if (text[i] < '0' || text[i] > '9') return std::nullopt;
                v = (v * 10) + (text[i] - '0');
            }
            return v;
        };
        const auto y = num(0, 4);
        const auto mo = num(5, 2);
        const auto da = num(8, 2);
        if (!y || !mo || !da) return std::nullopt;
        const std::chrono::year_month_day ymd{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*mo)},
                                              std::chrono::day{static_cast<unsigned>(*da)}};
        if (!ymd.ok()) return std::nullopt;
        return static_cast<int>(std::chrono::sys_days{ymd}.time_since_epoch().count());
    }

    static int day_(const core::tsv_row& row, std::string_view column)
    {
        const auto d = parse_day_(row.get<std::string>(column));
        if (!d) throw row.error(column, "must be a date as YYYY-MM-DD");
        return *d;
    }

    // "10.15.7" -> {10, 15, 7}; nullopt unless dot-separated non-negative integers.
    static std::optional<version_parts> parse_version_(std::string_view text)
    {
        if (text.empty()) return std::nullopt;
        version_parts parts;
        int v = 0;
        bool digit = false;
        for (const char c : text)
        {
            if (c == '.')
            {
                if (!digit) return std::nullopt;
                parts.push_back(v);
                v = 0;
                digit = false;
            }
            else if (c >= '0' && c <= '9')
            {
                if (v > 100'000) return std::nullopt;
                v = (v * 10) + (c - '0');
                digit = true;
            }
            else
            {
                return std::nullopt;
            }
        }
        if (!digit) return std::nullopt;
        parts.push_back(v);
        return parts;
    }

    static version_parts version_(const core::tsv_row& row, std::string_view column)
    {
        const auto v = parse_version_(row.get<std::string>(column));
        if (!v) throw row.error(column, "must be a version such as 14 or 10.15");
        return *v;
    }

    // Compare versions part by part; a missing part counts as 0.
    static int compare_(const version_parts& a, const version_parts& b)
    {
        for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i)
        {
            const int x = i < a.size() ? a[i] : 0;
            const int y = i < b.size() ? b[i] : 0;
            if (x != y) return x < y ? -1 : 1;
        }
        return 0;
    }

    static std::optional<std::uint32_t> parse_ipv4_(std::string_view text)
    {
        std::uint32_t out = 0;
        int parts = 0;
        std::uint32_t v = 0;
        int digits = 0;
        for (std::size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == '.')
            {
                if (digits == 0 || v > 255) return std::nullopt;
                out = (out << 8U) | v;
                ++parts;
                v = 0;
                digits = 0;
            }
            else if (text[i] >= '0' && text[i] <= '9' && digits < 3)
            {
                v = (v * 10) + static_cast<std::uint32_t>(text[i] - '0');
                ++digits;
            }
            else
            {
                return std::nullopt;
            }
        }
        if (parts != 4) return std::nullopt;
        return out;
    }

    static std::optional<std::uint16_t> parse_hex16_(std::string_view g)
    {
        if (g.empty() || g.size() > 4) return std::nullopt;
        std::uint32_t v = 0;
        for (const char c : g)
        {
            v <<= 4U;
            if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return std::nullopt;
        }
        return static_cast<std::uint16_t>(v);
    }

    // Hex groups separated by ':' ("2a01:fb00"); false on anything else.
    static bool parse_ipv6_groups_(std::string_view part, std::vector<std::uint16_t>& out)
    {
        if (part.empty()) return true;
        std::size_t start = 0;
        for (;;)
        {
            const auto end = part.find(':', start);
            const auto v = parse_hex16_(part.substr(start, end == std::string_view::npos ? part.size() - start : end - start));
            if (!v) return false;
            out.push_back(*v);
            if (end == std::string_view::npos) return true;
            start = end + 1;
        }
    }

    static std::optional<ipv6_net> parse_ipv6_(std::string_view text)
    {
        std::vector<std::uint16_t> head;
        std::vector<std::uint16_t> tail;
        const auto gap = text.find("::");
        if (gap == std::string_view::npos)
        {
            if (!parse_ipv6_groups_(text, head) || head.size() != 8) return std::nullopt;
        }
        else if (text.find("::", gap + 1) != std::string_view::npos || !parse_ipv6_groups_(text.substr(0, gap), head) ||
                 !parse_ipv6_groups_(text.substr(gap + 2), tail) || head.size() + tail.size() > 7)
        {
            return std::nullopt;
        }
        std::array<std::uint16_t, 8> groups{};
        std::ranges::copy(head, groups.begin());
        std::ranges::copy(tail, groups.end() - static_cast<std::ptrdiff_t>(tail.size()));
        ipv6_net n;
        for (std::size_t i = 0; i < 4; ++i) n.hi = (n.hi << 16U) | groups.at(i);
        for (std::size_t i = 4; i < 8; ++i) n.lo = (n.lo << 16U) | groups.at(i);
        return n;
    }

    static void validate_countries_(const core::tsv_row& row)
    {
        (void)country_(row);
        if (!(row.get<double>("population") >= 0.0)) throw row.error("population", "must not be negative");
        (void)share_(row, "internet_share");
        (void)share_(row, "ipv6_share");
    }

    static void validate_platforms_(const core::tsv_row& row)
    {
        for (const char* column : {"key", "label", "device", "os"}) (void)nonempty_(row, column);
        if (!kind_of_(row.get<std::string>("device")))
            throw row.error("device", "must be phone, tablet, desktop or other");
        (void)weight_(row);
    }

    static void validate_models_(const core::tsv_row& row)
    {
        for (const char* column : {"maker", "platform", "model"}) (void)nonempty_(row, column);
        (void)day_(row, "released");
        (void)version_(row, "os_first");
        if (!row.get_or<std::string>("os_last", "").empty()) (void)version_(row, "os_last");
        if (row.get_or<int>("width", 0) < 0 || row.get_or<int>("height", 0) < 0 ||
            !(row.get_or<double>("pixel_ratio", 0.0) >= 0.0))
            throw row.error("width", "the screen must not be negative");
        (void)weight_(row);
    }

    static void validate_screens_(const core::tsv_row& row)
    {
        (void)country_or_any_(row);
        if (row.get<int>("width") <= 0 || row.get<int>("height") <= 0 || !(row.get<double>("pixel_ratio") > 0.0))
            throw row.error("width", "the screen must be positive");
        (void)weight_(row);
    }

    static void validate_ipv4_(const core::tsv_row& row)
    {
        (void)country_(row);
        if (!parse_ipv4_(row.get<std::string>("start"))) throw row.error("start", "must be a dotted quad");
        if (const auto s = row.get<std::uint64_t>("size"); s == 0 || s > (std::uint64_t{1} << 32U))
            throw row.error("size", "must be between 1 and 2^32");
    }

    static void validate_ipv6_(const core::tsv_row& row)
    {
        (void)country_(row);
        if (!parse_ipv6_(row.get<std::string>("prefix"))) throw row.error("prefix", "must be an IPv6 address");
        if (const auto n = row.get<int>("length"); n < 1 || n > 128)
            throw row.error("length", "must be between 1 and 128");
    }

    static void validate_reserved_(const core::tsv_row& row)
    {
        const auto v = row.get<int>("version");
        if (v != 4 && v != 6) throw row.error("version", "must be 4 or 6");
        const bool ok = v == 4 ? parse_ipv4_(row.get<std::string>("prefix")).has_value()
                               : parse_ipv6_(row.get<std::string>("prefix")).has_value();
        if (!ok) throw row.error("prefix", v == 4 ? "must be a dotted quad" : "must be an IPv6 address");
        const auto n = row.get<int>("length");
        if (n < 1 || n > (v == 4 ? 30 : 64))
            throw row.error("length", v == 4 ? "must be between 1 and 30 for IPv4" : "must be between 1 and 64 for IPv6");
    }

    static void validate_(table t, const core::tsv_row& row)
    {
        switch (t)
        {
        case table::countries: validate_countries_(row); break;
        case table::platforms: validate_platforms_(row); break;
        case table::platform_shares:
            if (!core::is_alpha2(row.get<std::string>("country")))
                throw row.error("country", "must be an upper case ISO 3166-1 alpha-2 code");
            (void)weight_(row);
            break;
        case table::maker_shares:
        case table::browser_shares:
            (void)country_or_any_(row);
            (void)weight_(row);
            break;
        case table::models: validate_models_(row); break;
        case table::releases:
            (void)version_(row, "version");
            (void)day_(row, "released");
            break;
        case table::adoption:
            if (!(row.get<double>("ramp_days") >= 0.0)) throw row.error("ramp_days", "must not be negative");
            if (!(row.get<double>("half_life_days") > 0.0)) throw row.error("half_life_days", "must be positive");
            break;
        case table::version_shares:
            (void)version_(row, "version");
            (void)day_(row, "date");
            (void)weight_(row, "share");
            break;
        case table::requirements:
            if (row.get<int>("from_version") < 0) throw row.error("from_version", "must not be negative");
            (void)version_(row, "min_os");
            break;
        case table::ua_templates: (void)nonempty_(row, "template"); break;
        case table::screens: validate_screens_(row); break;
        case table::ipv4: validate_ipv4_(row); break;
        case table::ipv6: validate_ipv6_(row); break;
        case table::reserved: validate_reserved_(row); break;
        }
    }

    // A lookup key: the parts joined by tabs.
    static std::string join_(std::initializer_list<std::string_view> parts)
    {
        std::string out;
        for (const auto part : parts)
        {
            if (!out.empty()) out += '\t';
            out += part;
        }
        return out;
    }

    static std::vector<core::tsv_row> rows_(const tables& tbl, table t) { return tbl.at(index_(t)).rows(); }

    // Countries, platforms, makers and models.
    static void build_devices_(model& m, const tables& tbl)
    {
        const auto rows = [&](table t) { return rows_(tbl, t); };
        for (const auto& row : rows(table::countries))
        {
            m.countries.push_back({.code = country_(row),
                                   .users = core::math::fenced(row.get<double>("population") *
                                                               share_(row, "internet_share")),
                                   .ipv6_share = share_(row, "ipv6_share")});
        }
        std::ranges::sort(m.countries, {}, &country_row::code);
        for (std::size_t i = 0; i < m.countries.size(); ++i) m.country_index[m.countries[i].code] = i;

        for (const auto& row : rows(table::platforms))
        {
            m.platforms.push_back({.key = row.get<std::string>("key"), .label = row.get<std::string>("label"),
                                   .device = row.get<std::string>("device"), .os = row.get<std::string>("os"),
                                   .weight = weight_(row)});
        }
        for (const auto& row : rows(table::platform_shares))
            m.platform_shares[row.get<std::string>("country")].emplace_back(row.get<std::string>("key"), weight_(row));
        for (const auto& row : rows(table::maker_shares))
        {
            m.maker_shares[join_({country_or_any_(row), row.get<std::string>("platform")})].emplace_back(
                row.get<std::string>("maker"), weight_(row));
        }
        for (const auto& row : rows(table::models))
        {
            model_row r{.key = row.get<std::string>("key"),
                        .maker = row.get<std::string>("maker"),
                        .platform = row.get<std::string>("platform"),
                        .model = row.get<std::string>("model"),
                        .released = day_(row, "released"),
                        .os_first = version_(row, "os_first"),
                        .os_last = {},
                        .screen = {.width = row.get_or<int>("width", 0),
                                   .height = row.get_or<int>("height", 0),
                                   .pixel_ratio = row.get_or<double>("pixel_ratio", 0.0)},
                        .ua_model = row.get_or<std::string>("ua_model", ""),
                        .weight = weight_(row)};
            if (!row.get_or<std::string>("os_last", "").empty()) r.os_last = version_(row, "os_last");
            m.models_by[join_({r.platform, r.maker})].push_back(m.models.size());
            m.models.push_back(std::move(r));
        }
    }

    // Releases, adoption curves, version shares and requirements.
    static void build_versions_(model& m, const tables& tbl)
    {
        const auto rows = [&](table t) { return rows_(tbl, t); };
        for (const auto& row : rows(table::releases))
        {
            m.releases[row.get<std::string>("product")].push_back({.version = row.get<std::string>("version"),
                                                                   .parts = version_(row, "version"),
                                                                   .released = day_(row, "released"),
                                                                   .full = row.get_or<std::string>("full", "")});
        }
        if (!m.releases.empty()) m.as_of = std::numeric_limits<int>::min();
        for (auto& [product, list] : m.releases)
        {
            for (const auto& rel : list) m.as_of = std::max(m.as_of, rel.released);
            std::ranges::sort(list, [](const release& a, const release& b) {
                if (a.released != b.released) return a.released < b.released;
                return compare_(a.parts, b.parts) < 0;
            });
        }
        for (const auto& row : rows(table::adoption))
        {
            m.adoption[row.get<std::string>("product")] = {.ramp = row.get<double>("ramp_days"),
                                                           .half_life = row.get<double>("half_life_days")};
        }
        for (const auto& row : rows(table::version_shares))
        {
            m.version_shares[row.get<std::string>("product")][row.get<std::string>("version")].emplace_back(
                day_(row, "date"), weight_(row, "share"));
        }
        for (auto& [product, versions] : m.version_shares)
            for (auto& [version, points] : versions) std::ranges::sort(points);
        for (const auto& row : rows(table::requirements))
        {
            m.requirements[join_({row.get<std::string>("browser"), row.get<std::string>("os")})].push_back(
                {.from = row.get<int>("from_version"), .min_os = version_(row, "min_os")});
        }
    }

    // Browser shares, user-agent templates and screens.
    static void build_browsers_(model& m, const tables& tbl)
    {
        const auto rows = [&](table t) { return rows_(tbl, t); };
        for (const auto& row : rows(table::browser_shares))
        {
            m.browser_shares[join_({country_or_any_(row), row.get<std::string>("os")})].emplace_back(
                row.get<std::string>("browser"), weight_(row));
        }
        for (const auto& row : rows(table::ua_templates))
        {
            m.ua_templates[join_({row.get<std::string>("browser"), row.get<std::string>("os")})].push_back(
                {.min_browser = row.get<int>("min_browser"),
                 .min_os = row.get<int>("min_os"),
                 .text = row.get<std::string>("template")});
        }
        for (const auto& row : rows(table::screens))
        {
            m.screens[join_({country_or_any_(row), row.get<std::string>("os")})].push_back(
                {.screen = {.width = row.get<int>("width"),
                            .height = row.get<int>("height"),
                            .pixel_ratio = row.get<double>("pixel_ratio")},
                 .weight = weight_(row)});
        }
    }

    // IP blocks and the documentation ranges.
    static void build_addresses_(model& m, const tables& tbl)
    {
        const auto rows = [&](table t) { return rows_(tbl, t); };
        for (const auto& row : rows(table::ipv4))
        {
            auto& set = m.ipv4[country_(row)];
            set.start.push_back(*parse_ipv4_(row.get<std::string>("start")));
            set.size.push_back(row.get<std::uint64_t>("size"));
        }
        for (auto& [code, set] : m.ipv4)
        {
            std::vector<double> w(set.size.begin(), set.size.end());
            set.by_size.emplace(w);
        }
        for (const auto& row : rows(table::ipv6))
        {
            auto net = *parse_ipv6_(row.get<std::string>("prefix"));
            net.length = row.get<int>("length");
            m.ipv6[country_(row)].nets.push_back(net);
        }
        for (auto& [code, set] : m.ipv6)
        {
            std::vector<double> w;
            w.reserve(set.nets.size());
            // Weight by the number of /64 networks (a longer prefix counts as one).
            for (const auto& n : set.nets) w.push_back(std::ldexp(1.0, std::max(0, 64 - n.length)));
            set.by_size.emplace(w);
        }
        for (const auto& row : rows(table::reserved))
        {
            if (row.get<int>("version") == 4)
            {
                m.reserved4.emplace_back(*parse_ipv4_(row.get<std::string>("prefix")), row.get<int>("length"));
            }
            else
            {
                auto net = *parse_ipv6_(row.get<std::string>("prefix"));
                net.length = row.get<int>("length");
                m.reserved6.push_back(net);
            }
        }
    }

    static model build_model_(const tables& tbl)
    {
        model m;
        build_devices_(m, tbl);
        build_versions_(m, tbl);
        build_browsers_(m, tbl);
        build_addresses_(m, tbl);

        std::vector<double> users;
        for (std::size_t i = 0; i < m.countries.size(); ++i)
        {
            if (!m.ipv4.contains(m.countries[i].code) || !(m.countries[i].users > 0.0)) continue;
            m.drawable.push_back(i);
            users.push_back(m.countries[i].users);
        }
        if (!users.empty()) m.country_pick.emplace(users);
        return m;
    }

    static documents read_directory_(const std::filesystem::path& dir)
    {
        documents docs;
        for (const auto t : all_tables_)
        {
            const auto file = dir / file_name_(t);
            std::error_code ec;
            if (std::filesystem::is_regular_file(file, ec)) docs.at(index_(t)) = core::tsv_document::load(file);
        }
        return docs;
    }

    void apply_(documents docs, core::layer_mode mode, bool reset)
    {
        tables next = _tables;
        for (const auto t : all_tables_)
        {
            auto& doc = docs.at(index_(t));
            auto& tbl = next.at(index_(t));
            if (!doc)
            {
                if (reset) tbl.clear();
                continue;
            }
            (void)tbl.extend(std::move(*doc), mode, [t](const core::tsv_row& row) { validate_(t, row); });
        }
        auto built = build_model_(next);
        _tables = std::move(next);
        _model = std::move(built);
    }

    // -- State ------------------------------------------------------------

    tables _tables{make_tables_()};
    model _model;
    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit dvg(auto_probe_tag /*tag*/)
    {
        if (!load(dataset::lite)) (void)load(dataset::full);
    }

    static std::span<const core::embedded_file> embedded_files_()
    {
#ifdef DASMIG_DEVICE_EMBED
        return embedded::devicegen::files;
#else
        return {};
#endif
    }

    std::uint64_t draw_seed_() { return _engine(); }

    void require_data_() const
    {
        if (!has_data()) throw std::runtime_error("device-generator: no data loaded. Call load() first.");
    }

    // -- Generation helpers -------------------------------------------------

    static std::optional<device_kind> kind_of_(std::string_view text)
    {
        if (text == "phone") return device_kind::phone;
        if (text == "tablet") return device_kind::tablet;
        if (text == "desktop") return device_kind::desktop;
        if (text == "other") return device_kind::other;
        return std::nullopt;
    }

    static std::string upper_(std::string_view text)
    {
        std::string out{text};
        for (auto& c : out)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        return out;
    }

    static int check_date_(const std::chrono::year_month_day& date)
    {
        if (!date.ok()) throw std::invalid_argument("device-generator: the reference date is not a valid date");
        if (date < device_request::earliest_reference_date)
            throw std::invalid_argument("device-generator: the reference date is before 2021-01-01, "
                                        "the earliest the data covers");
        return static_cast<int>(std::chrono::sys_days{date}.time_since_epoch().count());
    }

    // Index of a weighted pick; uniform over the positive weights when not weighted.
    static std::size_t pick_index_(std::span<const double> weights, bool weighted, core::rng& r)
    {
        if (weighted) return core::discrete{weights}(r);
        std::vector<double> flat;
        flat.reserve(weights.size());
        for (const double w : weights) flat.push_back(w > 0.0 ? 1.0 : 0.0);
        return core::discrete{flat}(r);
    }

    static bool any_positive_(std::span<const double> weights)
    {
        return std::ranges::any_of(weights, [](double w) { return w > 0.0; });
    }

    // A model's weight at a day: grows for `ramp` days, then halves every `half_life`.
    static double adoption_weight_(const adoption_row& a, int age)
    {
        if (age < 0) return 0.0;
        const double ramp = a.ramp > 0.0 ? std::min(1.0, (static_cast<double>(age) + 1.0) / a.ramp) : 1.0;
        const double decay = core::math::exp(-std::numbers::ln2 * static_cast<double>(age) / a.half_life);
        return core::math::fenced(ramp * decay);
    }

    // Weights of the allowed releases of one product (sorted by date) at a day: a
    // release grows for `ramp` days after it comes out and, once the next allowed
    // release is out, halves every `half_life` days from then. The newest allowed
    // release keeps its users, so a system stuck on its last version stays on it.
    static std::vector<double> version_weights_(const std::vector<release>& list, const std::vector<bool>& allowed,
                                                const adoption_row& a, int day)
    {
        std::vector<double> w(list.size(), 0.0);
        std::optional<int> successor;
        for (std::size_t i = list.size(); i-- > 0;)
        {
            if (!allowed[i]) continue;
            const int age = day - list[i].released;
            const double ramp = a.ramp > 0.0 ? std::min(1.0, (static_cast<double>(age) + 1.0) / a.ramp) : 1.0;
            const double decay =
                successor ? core::math::exp(-std::numbers::ln2 * static_cast<double>(day - *successor) / a.half_life)
                          : 1.0;
            w[i] = core::math::fenced(ramp * decay);
            successor = list[i].released;
        }
        return w;
    }

    [[nodiscard]] adoption_row adoption_of_(std::string_view product) const
    {
        if (const auto it = _model.adoption.find(product); it != _model.adoption.end()) return it->second;
        return {.ramp = 30.0, .half_life = 365.0};
    }

    // Linear interpolation of a version's explicit shares; held after the last date.
    static double interpolate_(const std::vector<std::pair<int, double>>& points, int day)
    {
        if (points.empty()) return 0.0;
        if (day <= points.front().first) return points.front().second;
        if (day >= points.back().first) return points.back().second;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            if (day <= points[i].first)
            {
                const auto [d0, s0] = points[i - 1];
                const auto [d1, s1] = points[i];
                const double t = static_cast<double>(day - d0) / static_cast<double>(d1 - d0);
                return s0 + core::math::fenced(t * (s1 - s0));
            }
        }
        return points.back().second;
    }

    [[nodiscard]] const country_row& pick_country_(const device_request& request, const core::rng& call) const
    {
        const auto& m = _model;
        if (request.country)
        {
            const auto code = upper_(*request.country);
            const auto it = m.country_index.find(code);
            if (it == m.country_index.end()) throw core::unknown_country_error(code);
            if (!request.safe && !m.ipv4.contains(code))
                throw std::invalid_argument("device-generator: no registered addresses for " + code +
                                            "; use safe for a documentation address");
            return m.countries[it->second];
        }
        if (!m.country_pick) throw std::runtime_error("device-generator: no country has addresses");
        auto r = call.split("country");
        std::vector<double> w;
        w.reserve(m.drawable.size());
        for (const auto i : m.drawable) w.push_back(m.countries[i].users);
        return m.countries[m.drawable[request.weighted ? (*m.country_pick)(r) : pick_index_(w, false, r)]];
    }

    // The browsers of an OS in a country, with their weights (the country's rows, else the default).
    [[nodiscard]] const weighted_keys* browsers_of_(std::string_view country, std::string_view os) const
    {
        if (const auto it = _model.browser_shares.find(join_({country, os})); it != _model.browser_shares.end())
            return &it->second;
        if (const auto it = _model.browser_shares.find(join_({"*", os})); it != _model.browser_shares.end())
            return &it->second;
        return nullptr;
    }

    // The makers of a platform in a country (the country's rows, else the default).
    [[nodiscard]] const weighted_keys* makers_of_(std::string_view country, std::string_view platform) const
    {
        if (const auto it = _model.maker_shares.find(join_({country, platform})); it != _model.maker_shares.end())
            return &it->second;
        if (const auto it = _model.maker_shares.find(join_({"*", platform})); it != _model.maker_shares.end())
            return &it->second;
        return nullptr;
    }

    // Whether a platform can have a browser in a country: the OS lists it, and
    // Samsung Internet needs a Samsung maker.
    [[nodiscard]] bool platform_has_browser_(std::string_view country, const platform_row& p,
                                             std::string_view browser) const
    {
        const auto* list = browsers_of_(country, p.os);
        if (list == nullptr) return false;
        if (!std::ranges::any_of(*list, [&](const auto& b) { return b.first == browser && b.second > 0.0; }))
            return false;
        if (browser != "samsung") return true;
        const auto* makers = makers_of_(country, p.key);
        return makers != nullptr &&
               std::ranges::any_of(*makers, [](const auto& m) { return m.first == "Samsung" && m.second > 0.0; });
    }

    // Platform weights in a country: its percentages for the platforms it lists; the others
    // share the rest by global weight. Zero for platforms the kind or browser rules out.
    // A platform's percentage in a country's shares, if the country lists it.
    static const double* share_of_(const weighted_keys* shares, std::string_view key)
    {
        if (shares == nullptr) return nullptr;
        const auto it = std::ranges::find(*shares, key, &std::pair<std::string, double>::first);
        return it == shares->end() ? nullptr : &it->second;
    }

    // Platform weights in a country: its percentages for the platforms it lists; the
    // others share the rest by global weight.
    [[nodiscard]] std::vector<double> base_platform_weights_(std::string_view country) const
    {
        const auto& m = _model;
        const auto it = m.platform_shares.find(country);
        const weighted_keys* shares = it == m.platform_shares.end() ? nullptr : &it->second;
        double listed = 0.0;
        double rest = 0.0;
        for (const auto& p : m.platforms)
        {
            if (const auto* w = share_of_(shares, p.key)) listed += *w;
            else rest += p.weight;
        }
        const double remaining = std::max(0.0, 100.0 - listed);
        std::vector<double> weights;
        weights.reserve(m.platforms.size());
        for (const auto& p : m.platforms)
        {
            if (const auto* o = share_of_(shares, p.key)) weights.push_back(*o);
            else if (shares == nullptr) weights.push_back(p.weight);
            else weights.push_back(rest > 0.0 ? core::math::fenced(remaining * p.weight) / rest : 0.0);
        }
        return weights;
    }

    // The country's platform weights, zero for platforms the kind or browser rules out.
    [[nodiscard]] std::vector<double> platform_weights_(const device_request& request, std::string_view country) const
    {
        auto weights = base_platform_weights_(country);
        for (std::size_t i = 0; i < weights.size(); ++i)
        {
            const auto& p = _model.platforms[i];
            if ((request.kind && kind_of_(p.device) != request.kind) ||
                (request.browser && !platform_has_browser_(country, p, *request.browser)))
                weights[i] = 0.0;
        }
        return weights;
    }

    [[nodiscard]] const platform_row& pick_platform_(const device_request& request, std::string_view country,
                                       const core::rng& call) const
    {
        const auto& m = _model;
        if (request.platform)
        {
            const auto it = std::ranges::find(m.platforms, *request.platform, &platform_row::key);
            if (it == m.platforms.end())
                throw std::invalid_argument("device-generator: unknown platform " + *request.platform);
            if (request.kind && kind_of_(it->device) != request.kind)
                throw std::invalid_argument("device-generator: platform " + it->key + " is a " + it->device);
            if (request.browser && !platform_has_browser_(country, *it, *request.browser))
                throw std::invalid_argument("device-generator: " + *request.browser + " does not run on " +
                                            it->key);
            return *it;
        }

        const auto weights = platform_weights_(request, country);
        if (!any_positive_(weights))
            throw std::invalid_argument("device-generator: no platform matches the kind and browser asked for");
        auto r = call.split("platform");
        return m.platforms[pick_index_(weights, request.weighted, r)];
    }

    // Whether a maker has a model of the platform out by the day; true when the
    // data lists no models for that maker and platform (most desktops).
    [[nodiscard]] bool maker_has_model_(std::string_view platform, std::string_view maker, int day) const
    {
        const auto it = _model.models_by.find(join_({platform, maker}));
        if (it == _model.models_by.end()) return true;
        return std::ranges::any_of(it->second, [&](std::size_t i) { return _model.models[i].released <= day; });
    }

    std::string pick_maker_(const device_request& request, std::string_view country, const platform_row& p, int day,
                            core::rng& r) const
    {
        const auto* list = makers_of_(country, p.key);
        if (list == nullptr) return {};
        std::vector<double> w;
        w.reserve(list->size());
        for (const auto& [maker, weight] : *list)
        {
            const bool wanted = request.browser != "samsung" || maker == "Samsung";
            w.push_back(wanted && maker_has_model_(p.key, maker, day) ? weight : 0.0);
        }
        if (!any_positive_(w)) return {};
        return (*list)[pick_index_(w, request.weighted, r)].first;
    }

    const model_row* pick_model_(const device_request& request, const platform_row& p, std::string_view maker,
                                 int day, core::rng& r) const
    {
        const auto& m = _model;
        const auto it = m.models_by.find(join_({p.key, maker}));
        if (it == m.models_by.end()) return nullptr;
        const auto a = adoption_of_("model:" + p.key);
        std::vector<double> w;
        w.reserve(it->second.size());
        for (const auto i : it->second)
        {
            const auto& mdl = m.models[i];
            w.push_back(core::math::fenced(mdl.weight * adoption_weight_(a, day - mdl.released)));
        }
        if (!any_positive_(w)) return nullptr;
        return &m.models[it->second[pick_index_(w, request.weighted, r)]];
    }

    static std::string product_of_os_(std::string_view os)
    {
        if (os == "iOS") return "ios";
        if (os == "iPadOS") return "ipados";
        if (os == "Android") return "android";
        if (os == "Windows") return "windows";
        if (os == "macOS") return "macos";
        return {};
    }

    static bool has_minor_versions_(std::string_view os) { return os == "iOS" || os == "iPadOS"; }

    // OS version weights at a day, within the model's range unless `respect_model` is false.
    [[nodiscard]] std::vector<double> os_weights_(const std::vector<release>& list, std::string_view product,
                                                  const model_row* mdl, int day, bool respect_model) const
    {
        std::vector<bool> allowed;
        allowed.reserve(list.size());
        for (const auto& rel : list)
        {
            const bool in_range = !respect_model || mdl == nullptr ||
                                  (compare_(rel.parts, mdl->os_first) >= 0 &&
                                   (mdl->os_last.empty() || compare_(rel.parts, mdl->os_last) <= 0));
            allowed.push_back(rel.released <= day && in_range);
        }
        const auto shares = _model.version_shares.find(product);
        if (shares == _model.version_shares.end()) return version_weights_(list, allowed, adoption_of_(product), day);
        std::vector<double> w(list.size(), 0.0);
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (!allowed[i]) continue;
            const auto v = shares->second.find(list[i].version);
            w[i] = v == shares->second.end() ? 0.0 : interpolate_(v->second, day);
        }
        return w;
    }

    [[nodiscard]] const release* pick_os_version_(const device_request& request, const platform_row& p,
                                                  const model_row* mdl, int day, const core::rng& call) const
    {
        const auto product = product_of_os_(p.os);
        const auto it = _model.releases.find(product);
        if (product.empty() || it == _model.releases.end()) return nullptr;
        const auto& list = it->second;
        auto w = os_weights_(list, product, mdl, day, true);
        if (!any_positive_(w))
        {
            // The model's range has no released version with a share: its first version, if released.
            if (mdl != nullptr)
            {
                for (const auto& rel : list)
                    if (rel.released <= day && compare_(rel.parts, mdl->os_first) >= 0) return &rel;
            }
            w = os_weights_(list, product, mdl, day, false);
            if (!any_positive_(w)) return nullptr;
        }
        auto r = call.split("os-version");
        return &list[pick_index_(w, request.weighted, r)];
    }

    // "17" -> "17.5": the newest minor out by the day (about one every 45 days, up
    // to the last one the data knows), then a few minors older with falling chance.
    static std::string with_minor_(const release& rel, int day, core::rng r)
    {
        const auto last = parse_version_(rel.full);
        const int known = last && last->size() > 1 ? (*last)[1] : 0;
        const int out_by_day = std::max(0, (day - rel.released) / 45);
        int minor = std::min(known, out_by_day);
        while (minor > 0 && core::bernoulli(r, 0.4)) --minor;
        return rel.version + "." + std::to_string(minor);
    }

    struct browser_pick
    {
        std::string key;
        std::string version;
        std::string full;
    };

    static std::string browser_product_(std::string_view browser, std::string_view os)
    {
        if (browser == "chrome" && (os == "iOS" || os == "iPadOS")) return "chrome-ios";
        return std::string{browser};
    }

    // Whether a browser version runs on an OS version (requirements.tsv).
    [[nodiscard]] bool runs_on_(std::string_view browser, std::string_view os, int browser_major,
                                const version_parts& os_version) const
    {
        const auto it = _model.requirements.find(join_({browser, os}));
        if (it == _model.requirements.end() || os_version.empty()) return true;
        return std::ranges::none_of(it->second, [&](const requirement& req) {
            return browser_major >= req.from && compare_(os_version, req.min_os) < 0;
        });
    }

    // Whether any version of a browser runs on the OS by the day (browser_versions_
    // gives one a positive weight exactly then).
    [[nodiscard]] bool has_browser_version_(std::string_view browser, std::string_view os,
                                            const version_parts& os_version, int day) const
    {
        const auto product = browser_product_(browser, os);
        const auto it = _model.releases.find(product);
        if (it == _model.releases.end()) return false;
        const std::string req_browser = browser == "chrome" ? product : std::string{browser};
        return std::ranges::any_of(it->second, [&](const release& rel) {
            const int major = rel.parts.empty() ? 0 : rel.parts.front();
            return rel.released <= day && runs_on_(req_browser, os, major, os_version);
        });
    }

    // The versions of a browser that run on the OS by the day, with their weights.
    [[nodiscard]] std::pair<const std::vector<release>*, std::vector<double>>
    browser_versions_(std::string_view browser, std::string_view os, const version_parts& os_version, int day) const
    {
        const auto product = browser_product_(browser, os);
        const auto it = _model.releases.find(product);
        if (it == _model.releases.end()) return {nullptr, {}};
        const std::string req_browser = browser == "chrome" ? std::string{product} : std::string{browser};
        std::vector<bool> allowed;
        allowed.reserve(it->second.size());
        for (const auto& rel : it->second)
        {
            const int major = rel.parts.empty() ? 0 : rel.parts.front();
            allowed.push_back(rel.released <= day && runs_on_(req_browser, os, major, os_version));
        }
        return {&it->second, version_weights_(it->second, allowed, adoption_of_(product), day)};
    }

    [[nodiscard]] browser_pick pick_browser_(const device_request& request, std::string_view country, const platform_row& p,
                               std::string_view maker, const std::string& os_version, int day,
                               const core::rng& call) const
    {
        const auto os_parts = parse_version_(os_version).value_or(version_parts{});
        const bool apple_mobile = p.os == "iOS" || p.os == "iPadOS";
        const auto* list = browsers_of_(country, p.os);
        if (list == nullptr) return {};

        const auto available = [&](std::string_view b) {
            if (b == "samsung" && maker != "Samsung") return false;
            if (b == "safari" && apple_mobile) return true; // the OS's own version
            return has_browser_version_(b, p.os, os_parts, day);
        };
        std::vector<double> w;
        w.reserve(list->size());
        for (const auto& [b, weight] : *list)
        {
            const bool wanted = !request.browser || *request.browser == b;
            w.push_back(wanted && weight > 0.0 && available(b) ? weight : 0.0);
        }
        if (!any_positive_(w))
        {
            if (request.browser)
                throw std::invalid_argument("device-generator: no version of " + *request.browser + " runs on " +
                                            p.os + " " + os_version + " by the reference date");
            return {};
        }
        auto r = call.split("browser");
        const auto& key = (*list)[pick_index_(w, request.weighted, r)].first;

        browser_pick out{.key = key, .version = {}, .full = {}};
        if (key == "safari" && apple_mobile)
        {
            out.version = os_version;
            return out;
        }
        auto [releases, vw] = browser_versions_(key, p.os, os_parts, day);
        auto rv = call.split("browser-version");
        const auto& rel = (*releases)[pick_index_(vw, request.weighted, rv)];
        out.version = key == "safari" ? with_minor_(rel, day, call.split("browser-minor")) : rel.version;
        out.full = rel.full;
        return out;
    }

    static std::string engine_(std::string_view browser, std::string_view os)
    {
        if (browser.empty()) return {};
        if (browser == "safari" || os == "iOS" || os == "iPadOS") return "WebKit";
        if (browser == "firefox") return "Gecko";
        return "Blink";
    }

    static int major_(std::string_view version)
    {
        const auto v = parse_version_(version);
        return v && !v->empty() ? v->front() : 0;
    }

    // The template with the highest (min_browser, min_os) the device reaches.
    [[nodiscard]] const ua_template* select_template_(std::string_view browser, std::string_view os, int bmaj,
                                                      int omaj) const
    {
        const auto it = _model.ua_templates.find(join_({browser, os}));
        if (it == _model.ua_templates.end()) return nullptr;
        const ua_template* best = nullptr;
        for (const auto& t : it->second)
        {
            if (t.min_browser > bmaj || t.min_os > omaj) continue;
            if (best == nullptr || std::pair{t.min_browser, t.min_os} > std::pair{best->min_browser, best->min_os})
                best = &t;
        }
        return best;
    }

    // Replace each {name} the lookup knows; leave anything else as written.
    template <typename Lookup> static std::string fill_template_(const std::string& text, const Lookup& value)
    {
        std::string out;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const auto close = text[i] == '{' ? text.find('}', i) : std::string::npos;
            const auto v = close == std::string::npos ? std::nullopt
                                                      : value(std::string_view{text}.substr(i + 1, close - i - 1));
            if (v)
            {
                out += *v;
                i = close;
                continue;
            }
            out += text[i];
        }
        return out;
    }

    // What fills a user-agent template.
    struct ua_context
    {
        const device* d{nullptr};
        std::string os_us;          // the OS version with underscores
        std::string os_full;        // the OS release's full token (Windows NT version)
        std::string model_token;
        std::string full;           // the browser release's full version
        int browser_major{0};
    };

    static std::optional<std::string> ua_value_(std::string_view name, const ua_context& c)
    {
        if (name == "os") return c.d->os_version;
        if (name == "os_") return c.os_us;
        if (name == "os_full") return c.os_full;
        if (name == "model") return c.model_token;
        if (name == "v") return c.d->browser_version;
        if (name == "full") return c.full.empty() ? std::to_string(c.browser_major) + ".0.0.0" : c.full;
        if (name == "mobile") return c.d->kind == "phone" ? "Mobile " : "";
        if (name == "form") return c.d->kind == "phone" ? "Mobile" : "Tablet";
        return std::nullopt;
    }

    [[nodiscard]] std::string render_ua_(const device& d, const platform_row& p, const model_row* mdl,
                                         const release* os_release, const std::string& full) const
    {
        const int bmaj = major_(d.browser_version);
        const auto* best = select_template_(d.browser, p.os, bmaj, major_(d.os_version));
        if (best == nullptr) return {};
        ua_context c{.d = &d,
                     .os_us = d.os_version,
                     .os_full = os_release != nullptr ? os_release->full : std::string{},
                     .model_token = mdl != nullptr && !mdl->ua_model.empty() ? mdl->ua_model : "K",
                     .full = full,
                     .browser_major = bmaj};
        std::ranges::replace(c.os_us, '.', '_');
        return fill_template_(best->text, [&](std::string_view name) { return ua_value_(name, c); });
    }

    // "pt-BR" -> "pt-BR,pt;q=0.9,en;q=0.8"; "en-US" -> "en-US,en;q=0.9".
    static std::string accept_language_(std::string_view locale)
    {
        if (locale.empty()) return {};
        std::string tag{locale};
        std::ranges::replace(tag, '_', '-');
        const auto dash = tag.find('-');
        const std::string lang = tag.substr(0, dash);
        std::string out = tag;
        double q = 0.9;
        if (dash != std::string::npos)
        {
            out += std::format(",{};q={:.1f}", lang, q);
            q -= 0.1;
        }
        if (lang != "en") out += std::format(",en;q={:.1f}", q);
        return out;
    }

    device_screen pick_screen_(const device_request& request, std::string_view country, const platform_row& p,
                               const model_row* mdl, const core::rng& call) const
    {
        if (mdl != nullptr && mdl->screen.width > 0) return mdl->screen;
        const auto& m = _model;
        auto it = m.screens.find(join_({country, p.os}));
        if (it == m.screens.end()) it = m.screens.find(join_({"*", p.os}));
        if (it == m.screens.end() || it->second.empty()) return {};
        std::vector<double> w;
        w.reserve(it->second.size());
        for (const auto& s : it->second) w.push_back(s.weight);
        if (!any_positive_(w)) return {};
        auto r = call.split("screen");
        return it->second[pick_index_(w, request.weighted, r)].screen;
    }

    static std::string format_ipv4_(std::uint32_t a)
    {
        return std::format("{}.{}.{}.{}", a >> 24U, (a >> 16U) & 0xFFU, (a >> 8U) & 0xFFU, a & 0xFFU);
    }

    // RFC 5952 text: lower case, no leading zeros, the longest run of two or more zero groups as "::".
    static std::string format_ipv6_(const ipv6_net& a)
    {
        std::array<std::uint16_t, 8> g{};
        for (std::size_t i = 0; i < 4; ++i)
        {
            g.at(i) = static_cast<std::uint16_t>(a.hi >> (48U - (16U * i)));
            g.at(i + 4) = static_cast<std::uint16_t>(a.lo >> (48U - (16U * i)));
        }
        std::size_t best = 8;
        std::size_t best_len = 1;
        for (std::size_t i = 0; i < 8;)
        {
            if (g.at(i) != 0)
            {
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j < 8 && g.at(j) == 0) ++j;
            if (j - i > best_len)
            {
                best = i;
                best_len = j - i;
            }
            i = j;
        }
        std::string out;
        for (std::size_t i = 0; i < 8; ++i)
        {
            if (i == best)
            {
                out += "::";
                i += best_len - 1;
                continue;
            }
            if (!out.empty() && out.back() != ':') out += ':';
            out += std::format("{:x}", g.at(i));
        }
        return out;
    }

    // An address inside an IPv4 block, avoiding .0 and .255 where the block allows it.
    static std::uint32_t address_in_v4_(std::uint32_t start, std::uint64_t size, core::rng& r)
    {
        std::uint32_t a = start;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            a = start + static_cast<std::uint32_t>(core::bounded(r, size));
            const auto last = a & 0xFFU;
            if (last != 0 && last != 255) break;
        }
        return a;
    }

    // An address inside an IPv6 prefix: the prefix bits kept, the rest random.
    static ipv6_net address_in_v6_(const ipv6_net& n, core::rng& r)
    {
        const auto mask_of = [](int bits) -> std::uint64_t {
            if (bits <= 0) return 0;
            if (bits >= 64) return ~std::uint64_t{0};
            return ~std::uint64_t{0} << (64U - static_cast<unsigned>(bits));
        };
        const std::uint64_t mask_hi = mask_of(n.length);
        const std::uint64_t mask_lo = mask_of(n.length - 64);
        ipv6_net a{.hi = (n.hi & mask_hi) | (r() & ~mask_hi), .lo = 0, .length = 128};
        a.lo = (n.lo & mask_lo) | (r() & ~mask_lo);
        if (a.lo == 0) a.lo = 1;
        return a;
    }

    void pick_ip_(const device_request& request, const country_row& country, const core::rng& call, device& d) const
    {
        const auto& m = _model;
        const auto v6 = m.ipv6.find(country.code);
        const bool has_v6 = request.safe ? !m.reserved6.empty() : v6 != m.ipv6.end();
        bool use_v6 = false;
        if (request.ip == ip_version::v6) use_v6 = has_v6;
        else if (request.ip == ip_version::any && has_v6 && country.ipv6_share > 0.0)
        {
            auto rv = call.split("ip-version");
            use_v6 = core::bernoulli(rv, country.ipv6_share);
        }
        auto r = call.split("ip");
        d.reserved = request.safe;
        if (request.safe)
        {
            if (use_v6)
            {
                const auto& n = m.reserved6[core::bounded(r, m.reserved6.size())];
                d.ip = format_ipv6_(address_in_v6_(n, r));
                d.ip_version = 6;
                return;
            }
            if (m.reserved4.empty()) throw std::runtime_error("device-generator: reserved.tsv has no IPv4 range");
            const auto& [start, length] = m.reserved4[core::bounded(r, m.reserved4.size())];
            d.ip = format_ipv4_(address_in_v4_(start, std::uint64_t{1} << (32U - static_cast<unsigned>(length)), r));
            d.ip_version = 4;
            return;
        }
        if (use_v6)
        {
            const auto& set = v6->second;
            if (!set.by_size) throw std::runtime_error("device-generator: no IPv6 prefixes for " + country.code);
            const auto i = request.weighted ? (*set.by_size)(r) : core::bounded(r, set.nets.size());
            d.ip = format_ipv6_(address_in_v6_(set.nets[i], r));
            d.ip_version = 6;
            return;
        }
        const auto& set = m.ipv4.at(country.code);
        if (!set.by_size) throw std::runtime_error("device-generator: no IPv4 blocks for " + country.code);
        const auto i = request.weighted ? (*set.by_size)(r) : core::bounded(r, set.start.size());
        d.ip = format_ipv4_(address_in_v4_(set.start[i], set.size[i], r));
        d.ip_version = 4;
    }
};

} // namespace dasmig

#endif // DASMIG_DEVICEGEN_HPP
