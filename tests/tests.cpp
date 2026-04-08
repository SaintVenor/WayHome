#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <optional>
#include <vector>
#include <string>
#include <memory>
#include <system_error>
#include <iostream>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include "utf8.h"
#include "time_manager.h"
#include "city.h"
#include "cache.h"
#include "trip.h"
#include "printer.h"

using json = nlohmann::json;

// Auxiliary: temporary directory for cache tests

static std::filesystem::path MakeTempDir(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("yandex_test_" + name);
    std::filesystem::create_directories(path);
    return path;
}

static void RemoveTempDir(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

// UTF8

TEST(UTF8Test, ToCodepointsAscii) {
    auto cps = UTF8::ToCodepoints("abc");
    ASSERT_EQ(cps.size(), 3u);
    EXPECT_TRUE(cps[0].IsAscii());
    EXPECT_EQ(cps[0].AsciiChar(), 'a');
}

TEST(UTF8Test, ToCodepointsCyrillic) {
    auto cps = UTF8::ToCodepoints("а");
    ASSERT_EQ(cps.size(), 1u);
    EXPECT_FALSE(cps[0].IsAscii());
    EXPECT_EQ(cps[0].lead,  0xD0u);
    EXPECT_EQ(cps[0].trail, 0xB0u);
}

TEST(UTF8Test, ToCodepointsEmpty) {
    EXPECT_TRUE(UTF8::ToCodepoints("").empty());
}

TEST(UTF8Test, FromCodepointsRoundtrip) {
    const std::string s = "Привет, World!";
    EXPECT_EQ(UTF8::FromCodepoints(UTF8::ToCodepoints(s)), s);
}

TEST(UTF8Test, ToLowerAscii) {
    EXPECT_EQ(UTF8::ToLower("Hello"), "hello");
    EXPECT_EQ(UTF8::ToLower("ABC"), "abc");
    EXPECT_EQ(UTF8::ToLower("abc"), "abc");
}

TEST(UTF8Test, ToLowerCyrillicAP) {
    EXPECT_EQ(UTF8::ToLower("Абвгд"), "абвгд");
    EXPECT_EQ(UTF8::ToLower("МОСКВА"), "москва");
}

TEST(UTF8Test, ToLowerCyrillicRY) {
    EXPECT_EQ(UTF8::ToLower("Россия"), "россия");
    EXPECT_EQ(UTF8::ToLower("РЯ"), "ря");
}

TEST(UTF8Test, ToLowerYo) {
    EXPECT_EQ(UTF8::ToLower("Ёж"), "ёж");
    EXPECT_EQ(UTF8::ToLower("ЁЛА"), "ёла");
}

TEST(UTF8Test, ToLowerAlreadyLower) {
    EXPECT_EQ(UTF8::ToLower("москва"), "москва");
    EXPECT_EQ(UTF8::ToLower(""), "");
}

TEST(UTF8Test, ToUpperAscii) {
    EXPECT_EQ(UTF8::ToUpper("hello"), "HELLO");
    EXPECT_EQ(UTF8::ToUpper("abc"), "ABC");
}

TEST(UTF8Test, ToUpperCyrillicAP) {
    EXPECT_EQ(UTF8::ToUpper("абвгд"), "АБВГД");
}

TEST(UTF8Test, ToUpperCyrillicRY) {
    EXPECT_EQ(UTF8::ToUpper("ря"), "РЯ");
}

TEST(UTF8Test, ToUpperYo) {
    EXPECT_EQ(UTF8::ToUpper("ёж"), "ЁЖ");
}

TEST(UTF8Test, UpperFirstAscii) {
    EXPECT_EQ(UTF8::UpperFirst("hello"), "Hello");
    EXPECT_EQ(UTF8::UpperFirst("abc"), "Abc");
}

TEST(UTF8Test, UpperFirstCyrillic) {
    EXPECT_EQ(UTF8::UpperFirst("москва"), "Москва");
    EXPECT_EQ(UTF8::UpperFirst("россия"), "Россия");
}

TEST(UTF8Test, UpperFirstEmpty) {
    EXPECT_EQ(UTF8::UpperFirst(""), "");
}

TEST(UTF8Test, UpperFirstAlreadyUpper) {
    EXPECT_EQ(UTF8::UpperFirst("Москва"), "Москва");
}

TEST(UTF8Test, ToLowerToUpperRoundtrip) {
    const std::string original = "МОСКВА";
    EXPECT_EQ(UTF8::ToUpper(UTF8::ToLower(original)), original);
}

TEST(UTF8Test, MixedAsciiCyrillic) {
    EXPECT_EQ(UTF8::ToLower("Привет World"), "привет world");
    EXPECT_EQ(UTF8::ToUpper("привет world"), "ПРИВЕТ WORLD");
}

TEST(UTF8Test, CodepointEqualityOperator) {
    UTF8::Codepoint a{0xD0, 0xB0};
    UTF8::Codepoint b{0xD0, 0xB0};
    UTF8::Codepoint c{0xD0, 0xB1};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// TimeManager

TEST(TimeManagerTest, FormatDurationZero) {
    EXPECT_EQ(TimeManager::FormatDuration(0), "0мин");
    EXPECT_EQ(TimeManager::FormatDuration(-1), "0мин");
}

TEST(TimeManagerTest, FormatDurationMinutesOnly) {
    EXPECT_EQ(TimeManager::FormatDuration(30 * 60), "30мин");
    EXPECT_EQ(TimeManager::FormatDuration(59 * 60), "59мин");
}

TEST(TimeManagerTest, FormatDurationHoursAndMinutes) {
    EXPECT_EQ(TimeManager::FormatDuration(1 * 3600 + 30 * 60), "1ч 30мин");
    EXPECT_EQ(TimeManager::FormatDuration(2 * 3600), "2ч 0мин");
}

TEST(TimeManagerTest, FormatDurationDaysHoursMinutes) {
    EXPECT_EQ(TimeManager::FormatDuration(1 * 86400 + 2 * 3600 + 10 * 60), "1д 2ч 10мин");
}

TEST(TimeManagerTest, FormatDurationDaysZeroHours) {
    EXPECT_EQ(TimeManager::FormatDuration(1 * 86400 + 5 * 60), "1д 0ч 5мин");
}

TEST(TimeManagerTest, FormatTimeNormal) {
    EXPECT_EQ(TimeManager::FormatTime("2026-03-28T14:30:00+03:00"), "28.03 14:30");
}

TEST(TimeManagerTest, FormatTimeTooShort) {
    const std::string s = "short";
    EXPECT_EQ(TimeManager::FormatTime(s), s);
}

TEST(TimeManagerTest, IsoToSecondsBasic) {
    int s1 = TimeManager::IsoToSeconds("2026-03-28T14:30:00+03:00");
    int s2 = TimeManager::IsoToSeconds("2026-03-28T14:30:00+03:00");
    EXPECT_EQ(s1, s2);
}

TEST(TimeManagerTest, IsoToSecondsTooShort) {
    EXPECT_EQ(TimeManager::IsoToSeconds("short"), 0);
    EXPECT_EQ(TimeManager::IsoToSeconds(""), 0);
}

TEST(TimeManagerTest, IsoToSecondsTimezoneEffect) {
    int utc_plus3 = TimeManager::IsoToSeconds("2026-03-28T14:00:00+03:00");
    int utc_plus5 = TimeManager::IsoToSeconds("2026-03-28T16:00:00+05:00");
    EXPECT_EQ(utc_plus3, utc_plus5);
}

TEST(TimeManagerTest, IsoToSecondsNegativeTimezone) {
    int s = TimeManager::IsoToSeconds("2026-03-28T14:00:00-03:00");
    EXPECT_GT(s, 0);
}

TEST(TimeManagerTest, DurationSecondsNormal) {
    int d = TimeManager::DurationSeconds(
        "2026-03-28T10:00:00+03:00",
        "2026-03-28T12:00:00+03:00");
    EXPECT_EQ(d, 7200);
}

TEST(TimeManagerTest, DurationSecondsEmpty) {
    EXPECT_EQ(TimeManager::DurationSeconds("", "2026-03-28T12:00:00+03:00"), 0);
    EXPECT_EQ(TimeManager::DurationSeconds("2026-03-28T10:00:00+03:00", ""), 0);
    EXPECT_EQ(TimeManager::DurationSeconds("", ""), 0);
}

TEST(TimeManagerTest, DurationSecondsCrossTimezone) {
    int d = TimeManager::DurationSeconds(
        "2026-03-28T10:00:00+03:00",
        "2026-03-28T14:00:00+05:00");
    EXPECT_EQ(d, 7200);
}

TEST(TimeManagerTest, GetTodayReturnsValidDate) {
    auto today = TimeManager::GetToday();
    EXPECT_TRUE(today.ok());
}

TEST(TimeManagerTest, AddDaysPositive) {
    auto today = TimeManager::GetToday();
    auto tomorrow = TimeManager::AddDays(today, 1);
    auto day_after = TimeManager::AddDays(today, 2);
    EXPECT_LT(std::chrono::sys_days{today}, std::chrono::sys_days{tomorrow});
    EXPECT_LT(std::chrono::sys_days{tomorrow}, std::chrono::sys_days{day_after});
}

TEST(TimeManagerTest, AddDaysNegative) {
    auto today = TimeManager::GetToday();
    auto yesterday = TimeManager::AddDays(today, -1);
    EXPECT_LT(std::chrono::sys_days{yesterday}, std::chrono::sys_days{today});
}

TEST(TimeManagerTest, FormatYearMonth) {
    using namespace std::chrono;
    TimeManager::Date d{year{2026}, month{3u}, day{28u}};
    EXPECT_EQ(TimeManager::FormatYearMonth(d), "2026-03");
}

TEST(TimeManagerTest, FormatYearMonthDay) {
    using namespace std::chrono;
    TimeManager::Date d{year{2026}, month{3u}, day{28u}};
    EXPECT_EQ(TimeManager::FormatYearMonthDay(d), "2026-03-28");
}

TEST(TimeManagerTest, IsValidFutureDateToday) {
    EXPECT_TRUE(TimeManager::IsValidFutureDate(TimeManager::GetToday()));
}

TEST(TimeManagerTest, IsValidFutureDatePast) {
    using namespace std::chrono;
    TimeManager::Date past{year{2020}, month{1u}, day{1u}};
    EXPECT_FALSE(TimeManager::IsValidFutureDate(past));
}

TEST(TimeManagerTest, IsValidFutureDateFuture) {
    EXPECT_TRUE(TimeManager::IsValidFutureDate(TimeManager::AddDays(TimeManager::GetToday(), 30)));
}

TEST(TimeManagerTest, ParseDateSегодня) {
    auto result = TimeManager::ParseDate("сегодня");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), TimeManager::GetToday());
}

TEST(TimeManagerTest, ParseDateЗавтра) {
    auto result = TimeManager::ParseDate("завтра");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), TimeManager::AddDays(TimeManager::GetToday(), 1));
}

TEST(TimeManagerTest, ParseDateПослезавтра) {
    auto result = TimeManager::ParseDate("послезавтра");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), TimeManager::AddDays(TimeManager::GetToday(), 2));
}

TEST(TimeManagerTest, ParseDateDotFormat) {
    auto result = TimeManager::ParseDate("28.04.2026");
    ASSERT_TRUE(result.has_value());
    using namespace std::chrono;
    EXPECT_EQ(result.value(), TimeManager::Date(year{2026}, month{4u}, day{28u}));
}

TEST(TimeManagerTest, ParseDateDashFormat) {
    auto result = TimeManager::ParseDate("2026-04-28");
    ASSERT_TRUE(result.has_value());
    using namespace std::chrono;
    EXPECT_EQ(result.value(), TimeManager::Date(year{2026}, month{4u}, day{28u}));
}

TEST(TimeManagerTest, ParseDateSlashFormat) {
    auto result = TimeManager::ParseDate("28/04/2026");
    ASSERT_TRUE(result.has_value());
    using namespace std::chrono;
    EXPECT_EQ(result.value(), TimeManager::Date(year{2026}, month{4u}, day{28u}));
}

TEST(TimeManagerTest, ParseDateInvalidReturnsNullopt) {
    EXPECT_FALSE(TimeManager::ParseDate("").has_value());
    EXPECT_FALSE(TimeManager::ParseDate("notadate").has_value());
    EXPECT_FALSE(TimeManager::ParseDate("99.99.9999").has_value());
}

TEST(TimeManagerTest, ParseDatePastReturnsNullopt) {
    EXPECT_FALSE(TimeManager::ParseDate("01.01.2020").has_value());
}

TEST(TimeManagerTest, ParseDateTrailingSpaces) {
    auto result = TimeManager::ParseDate("28.04.2026   ");
    ASSERT_TRUE(result.has_value());
    using namespace std::chrono;
    EXPECT_EQ(result.value(), TimeManager::Date(year{2026}, month{4u}, day{28u}));
}

TEST(TimeManagerTest, ParseDateCaseInsensitive) {
    auto r1 = TimeManager::ParseDate("Завтра");
    auto r2 = TimeManager::ParseDate("ЗАВТРА");
    auto r3 = TimeManager::ParseDate("завтра");
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r1.value(), r2.value());
    EXPECT_EQ(r2.value(), r3.value());
}

// City

TEST(CityTest, GetNameStoredLower) {
    City c("Москва");
    EXPECT_EQ(c.GetName(), "москва");
}

TEST(CityTest, GetNameAlreadyLower) {
    City c("воронеж");
    EXPECT_EQ(c.GetName(), "воронеж");
}

TEST(CityTest, EqualityOperator) {
    City a("Москва");
    City b("москва");
    City c("Воронеж");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(CityTest, DisplaySimple) {
    City c("москва");
    EXPECT_EQ(c.Display(), "Москва");
}

TEST(CityTest, DisplayWithHyphen) {
    City c("санкт-петербург");
    EXPECT_EQ(c.Display(), "Санкт-Петербург");
}

TEST(CityTest, DisplayPrepositionAfterHyphen) {
    City c("ростов-на-дону");
    EXPECT_EQ(c.Display(), "Ростов-на-Дону");
}

TEST(CityTest, DisplayWithSpace) {
    City c("нижний новгород");
    EXPECT_EQ(c.Display(), "Нижний Новгород");
}

TEST(CityTest, DisplayEmpty) {
    City c("");
    EXPECT_EQ(c.Display(), "");
}

TEST(CityTest, LevenshteinSameStrings) {
    City a("москва");
    City b("москва");
    EXPECT_EQ(City::Levenshtein(a, b), 0);
}

TEST(CityTest, LevenshteinOneDiff) {
    City a("москва");
    City b("масква");
    EXPECT_EQ(City::Levenshtein(a, b), 1);
}

TEST(CityTest, LevenshteinEmpty) {
    City a("");
    City b("москва");
    EXPECT_EQ(City::Levenshtein(a, b), 6);
    EXPECT_EQ(City::Levenshtein(b, a), 6);
}

TEST(CityTest, LevenshteinBothEmpty) {
    City a("");
    City b("");
    EXPECT_EQ(City::Levenshtein(a, b), 0);
}

TEST(CityTest, LevenshteinSymmetric) {
    City a("казань");
    City b("рязань");
    EXPECT_EQ(City::Levenshtein(a, b), City::Levenshtein(b, a));
}

TEST(CityTest, LevenshteinTypo) {
    City a("санкт-петербург");
    City b("санкт-питербург");
    EXPECT_EQ(City::Levenshtein(a, b), 1);
}

TEST(CityTest, DefaultConstructor) {
    City c;
    EXPECT_EQ(c.GetName(), "");
}

TEST(CityTest, ConstructFromUpperCase) {
    City c("ВОРОНЕЖ");
    EXPECT_EQ(c.GetName(), "воронеж");
}

// CacheCities

// Test subclass with a redefined path
class TestCacheCities : public CacheCities {
public:
    explicit TestCacheCities(const std::filesystem::path& path)
        : path_(path) {}

protected:
    std::filesystem::path GetCachePath() const override { return path_; }
    json& GetCacheJson() const override { return cache_json_; }

private:
    std::filesystem::path path_;
};

static json MakeValidCitiesJson() {
    using namespace std::chrono;
    json j;
    j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
    j["cities"] = {
        {"москва",          "c213"},
        {"воронеж",         "c193"},
        {"санкт-петербург", "c2"},
        {"екатеринбург",    "c54"}
    };
    return j;
}

class CacheCitiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = MakeTempDir("cities_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));
        path_ = dir_ / "cities_cache.json";
    }

    void TearDown() override {
        RemoveTempDir(dir_);
    }

    void WriteCache(const json& j) {
        std::ofstream f(path_);
        ASSERT_TRUE(f.is_open());
        f << j.dump(1);
    }

    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(CacheCitiesTest, NeedUpdateCacheNoFile) {
    TestCacheCities cache(path_);
    EXPECT_TRUE(cache.NeedUpdateCache());
}

TEST_F(CacheCitiesTest, NeedUpdateCacheValidFile) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    EXPECT_FALSE(cache.NeedUpdateCache());
}

TEST_F(CacheCitiesTest, NeedUpdateCacheExpiredDate) {
    json j = MakeValidCitiesJson();
    j["last_update"] = "2020-01";
    WriteCache(j);
    TestCacheCities cache(path_);
    EXPECT_TRUE(cache.NeedUpdateCache());
}

TEST_F(CacheCitiesTest, NeedUpdateCacheCorruptFile) {
    std::ofstream f(path_);
    f << "not valid json{{{";
    f.close();
    TestCacheCities cache(path_);
    EXPECT_TRUE(cache.NeedUpdateCache());
}

TEST_F(CacheCitiesTest, GetCityFound) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    auto result = cache.GetCity("москва");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().GetName(), "москва");
}

TEST_F(CacheCitiesTest, GetCityFoundCaseInsensitive) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    auto result = cache.GetCity("Москва");
    ASSERT_TRUE(result.has_value());
}

TEST_F(CacheCitiesTest, GetCityNotFound) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    EXPECT_FALSE(cache.GetCity("несуществующий").has_value());
}

TEST_F(CacheCitiesTest, GetAllCities) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    auto cities = cache.GetAllCities();
    EXPECT_EQ(cities.size(), 4u);
}

TEST_F(CacheCitiesTest, GetAllCitiesEmpty) {
    json j;
    j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
    j["cities"] = json::object();
    WriteCache(j);
    TestCacheCities cache(path_);
    EXPECT_TRUE(cache.GetAllCities().empty());
}

TEST_F(CacheCitiesTest, GetCityCode) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    EXPECT_EQ(cache.GetCityCode("москва"), "c213");
    EXPECT_EQ(cache.GetCityCode("воронеж"), "c193");
}

TEST_F(CacheCitiesTest, GetCityCodeNotFound) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    EXPECT_EQ(cache.GetCityCode("абоба"), "");
}

TEST_F(CacheCitiesTest, GetCityCodeCaseInsensitive) {
    WriteCache(MakeValidCitiesJson());
    TestCacheCities cache(path_);
    EXPECT_EQ(cache.GetCityCode("МОСКВА"), "c213");
}

TEST_F(CacheCitiesTest, NoCitiesKey) {
    json j;
    j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
    WriteCache(j);
    TestCacheCities cache(path_);
    EXPECT_TRUE(cache.NeedUpdateCache());
}

// CacheTrips

// Helper class — CacheTrips with a redefined path
class TestCacheTrips : public CacheTrips {
public:
    TestCacheTrips(const std::filesystem::path& path,
                   const std::string& from_name,
                   const std::string& to_name)
        : CacheTrips("c193", "c213", "2026-04-28", from_name, to_name)
        , path_(path) {}

protected:
    std::filesystem::path GetCachePath() const override { return path_; }
    json& GetCacheJson() const override { return cache_json_; }

private:
    std::filesystem::path path_;
};

static json MakeValidTripsJson(const std::string& last_update = "") {
    json j;
    j["last_update"] = last_update.empty()
        ? TimeManager::FormatYearMonthDay(TimeManager::GetToday())
        : last_update;

    j["trips"] = json::array({
        {
            {"from_city",       "Воронеж"},
            {"to_city",         "Москва"},
            {"total_duration",  34200},
            {"transfers_count", 0},
            {"legs", json::array({
                {
                    {"transport_type",        "train"},
                    {"number",                "019С"},
                    {"thread_title",          "Ростов-на-Дону — Москва"},
                    {"carrier",               "РЖД/ФПК"},
                    {"departure",             "2026-04-28T00:59:00+03:00"},
                    {"arrival",               "2026-04-28T10:30:00+03:00"},
                    {"city_from",             "Воронеж"},
                    {"city_to",               "Москва"},
                    {"duration_seconds",      34260},
                    {"transfer_wait_seconds", 0},
                    {"transfer_city",         ""}
                }
            })}
        },
        {
            {"from_city",       "Воронеж"},
            {"to_city",         "Москва"},
            {"total_duration",  45000},
            {"transfers_count", 1},
            {"legs", json::array({
                {
                    {"transport_type",        "bus"},
                    {"number",                ""},
                    {"thread_title",          "Воронеж — Москва"},
                    {"carrier",               "ИП Тест"},
                    {"departure",             "2026-04-28T08:00:00+03:00"},
                    {"arrival",               "2026-04-28T14:00:00+03:00"},
                    {"city_from",             "Воронеж"},
                    {"city_to",               "Москва"},
                    {"duration_seconds",      21600},
                    {"transfer_wait_seconds", 7200},
                    {"transfer_city",         "Москва"}
                },
                {
                    {"transport_type",        "plane"},
                    {"number",                "SU 100"},
                    {"thread_title",          "Москва — Екатеринбург"},
                    {"carrier",               "Аэрофлот"},
                    {"departure",             "2026-04-28T16:00:00+03:00"},
                    {"arrival",               "2026-04-28T18:30:00+05:00"},
                    {"city_from",             "Москва"},
                    {"city_to",               "Екатеринбург"},
                    {"duration_seconds",      9000},
                    {"transfer_wait_seconds", 0},
                    {"transfer_city",         ""}
                }
            })}
        }
    });
    return j;
}

class CacheTripsTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = MakeTempDir("trips_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));
        path_ = dir_ / "c193-c213-2026-04-28.json";
    }

    void TearDown() override {
        RemoveTempDir(dir_);
    }

    void WriteCache(const json& j) {
        std::ofstream f(path_);
        ASSERT_TRUE(f.is_open());
        f << j.dump(1);
    }

    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(CacheTripsTest, NeedUpdateCacheNoFile) {
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    EXPECT_TRUE(cache.NeedUpdateCache());
}

TEST_F(CacheTripsTest, NeedUpdateCacheValidFile) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    EXPECT_FALSE(cache.NeedUpdateCache());
}

TEST_F(CacheTripsTest, NeedUpdateCacheExpired) {
    WriteCache(MakeValidTripsJson("2020-01-01"));
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    EXPECT_TRUE(cache.NeedUpdateCache());
}

TEST_F(CacheTripsTest, GetTripScheduleAll) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    auto trips = cache.GetTripSchedule();
    EXPECT_EQ(trips.size(), 2u);
}

TEST_F(CacheTripsTest, GetTripScheduleDirectOnly) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    auto trips = cache.GetTripSchedule(0);
    ASSERT_EQ(trips.size(), 1u);
    EXPECT_EQ(trips[0].transfers_count, 0);
}

TEST_F(CacheTripsTest, GetTripScheduleMaxOneTransfer) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    auto trips = cache.GetTripSchedule(1);
    EXPECT_EQ(trips.size(), 2u);
}

TEST_F(CacheTripsTest, GetTripScheduleFieldsParsed) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    auto trips = cache.GetTripSchedule(0);
    ASSERT_EQ(trips.size(), 1u);

    const Trip& t = trips[0];
    EXPECT_EQ(t.from_city, "Воронеж");
    EXPECT_EQ(t.to_city, "Москва");
    EXPECT_EQ(t.total_duration, 34200);
    EXPECT_EQ(t.transfers_count, 0);
    ASSERT_EQ(t.legs.size(), 1u);
    EXPECT_EQ(t.legs[0].transport_type, "train");
    EXPECT_EQ(t.legs[0].carrier, "РЖД/ФПК");
    EXPECT_EQ(t.legs[0].number, "019С");
    EXPECT_EQ(t.legs[0].city_from, "Воронеж");
    EXPECT_EQ(t.legs[0].city_to, "Москва");
}

TEST_F(CacheTripsTest, GetTripScheduleTransferFields) {
    WriteCache(MakeValidTripsJson());
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    auto trips = cache.GetTripSchedule(1);

    const Trip* with_transfer = nullptr;
    for (const auto& t : trips) {
        if (t.transfers_count == 1) { with_transfer = &t; break; }
    }
    ASSERT_NE(with_transfer, nullptr);
    ASSERT_EQ(with_transfer->legs.size(), 2u);
    EXPECT_EQ(with_transfer->legs[0].transfer_wait_seconds, 7200);
    EXPECT_EQ(with_transfer->legs[0].transfer_city, "Москва");
}

TEST_F(CacheTripsTest, GetTripScheduleEmptyTrips) {
    json j;
    j["last_update"] = TimeManager::FormatYearMonthDay(TimeManager::GetToday());
    j["trips"] = json::array();
    WriteCache(j);
    TestCacheTrips cache(path_, "Воронеж", "Москва");
    EXPECT_TRUE(cache.GetTripSchedule().empty());
}


// Printer

TEST(PrinterTest, PrintTripOutputContainsCityNames) {
    Trip trip;
    trip.from_city = "Воронеж";
    trip.to_city   = "Москва";
    trip.total_duration   = 34200;
    trip.transfers_count  = 0;

    TripLeg leg;
    leg.transport_type   = "train";
    leg.number           = "019С";
    leg.carrier          = "РЖД/ФПК";
    leg.departure        = "2026-04-28T00:59:00+03:00";
    leg.arrival          = "2026-04-28T10:30:00+03:00";
    leg.city_from        = "Воронеж";
    leg.city_to          = "Москва";
    leg.duration_seconds = 34260;
    trip.legs.push_back(leg);

    testing::internal::CaptureStdout();
    Printer::PrintTrip(trip, 1);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Воронеж"), std::string::npos);
    EXPECT_NE(output.find("Москва"),  std::string::npos);
    EXPECT_NE(output.find("019С"),    std::string::npos);
}

TEST(PrinterTest, PrintTripWithTransferShowsTransferInfo) {
    Trip trip;
    trip.from_city       = "Воронеж";
    trip.to_city         = "Екатеринбург";
    trip.total_duration  = 50000;
    trip.transfers_count = 1;

    TripLeg leg1;
    leg1.transport_type        = "bus";
    leg1.departure             = "2026-04-28T08:00:00+03:00";
    leg1.arrival               = "2026-04-28T14:00:00+03:00";
    leg1.city_from             = "Воронеж";
    leg1.city_to               = "Москва";
    leg1.duration_seconds      = 21600;
    leg1.transfer_wait_seconds = 7200;
    leg1.transfer_city         = "Москва";

    TripLeg leg2;
    leg2.transport_type   = "plane";
    leg2.number           = "SU 100";
    leg2.departure        = "2026-04-28T16:00:00+03:00";
    leg2.arrival          = "2026-04-28T18:30:00+05:00";
    leg2.city_from        = "Москва";
    leg2.city_to          = "Екатеринбург";
    leg2.duration_seconds = 9000;

    trip.legs = {leg1, leg2};

    testing::internal::CaptureStdout();
    Printer::PrintTrip(trip, 1);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Пересадка"), std::string::npos);
    EXPECT_NE(output.find("Москва"),    std::string::npos);
}

TEST(PrinterTest, PrintTripsNotFoundMessage) {
    std::vector<Trip> empty;
    City from("воронеж");
    City to("москва");

    testing::internal::CaptureStdout();
    Printer::PrintTrips(empty, from, to, 5);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("не найдены"), std::string::npos);
}

TEST(PrinterTest, PrintTripsLimitsCount) {
    std::vector<Trip> trips(5);
    for (auto& t : trips) {
        t.from_city = "Воронеж";
        t.to_city   = "Москва";
        TripLeg leg;
        leg.city_from = "Воронеж";
        leg.city_to   = "Москва";
        leg.departure = "2026-04-28T10:00:00+03:00";
        leg.arrival   = "2026-04-28T14:00:00+03:00";
        t.legs.push_back(leg);
    }

    City from("воронеж");
    City to("москва");

    testing::internal::CaptureStdout();
    Printer::PrintTrips(trips, from, to, 2);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("МАРШРУТ 1"), std::string::npos);
    EXPECT_NE(output.find("МАРШРУТ 2"), std::string::npos);
    EXPECT_EQ(output.find("МАРШРУТ 3"), std::string::npos);
}

TEST(PrinterTest, PrintTripITOGOLine) {
    Trip trip;
    trip.from_city       = "А";
    trip.to_city         = "Б";
    trip.total_duration  = 7200;
    trip.transfers_count = 0;

    TripLeg leg;
    leg.city_from = "А";
    leg.city_to   = "Б";
    leg.departure = "2026-04-28T10:00:00+03:00";
    leg.arrival   = "2026-04-28T12:00:00+03:00";
    leg.duration_seconds = 7200;
    trip.legs.push_back(leg);

    testing::internal::CaptureStdout();
    Printer::PrintTrip(trip, 1);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("ИТОГО"), std::string::npos);
    EXPECT_NE(output.find("2ч 0мин"), std::string::npos);
}

TEST(PrinterTest, PrintTripRouteHeader) {
    Trip trip;
    trip.from_city       = "Воронеж";
    trip.to_city         = "Владивосток";
    trip.total_duration  = 500000;
    trip.transfers_count = 2;

    TripLeg l1;
    l1.city_from = "Воронеж"; l1.city_to = "Москва";
    l1.departure = "2026-04-28T10:00:00+03:00";
    l1.arrival   = "2026-04-28T20:00:00+03:00";
    l1.transfer_wait_seconds = 3600;
    l1.transfer_city = "Москва";

    TripLeg l2;
    l2.city_from = "Москва"; l2.city_to = "Владивосток";
    l2.departure = "2026-04-28T21:00:00+03:00";
    l2.arrival   = "2026-04-29T10:00:00+10:00";

    trip.legs = {l1, l2};

    testing::internal::CaptureStdout();
    Printer::PrintTrip(trip, 3);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Воронеж"), std::string::npos);
    EXPECT_NE(output.find("Москва"), std::string::npos);
    EXPECT_NE(output.find("Владивосток"), std::string::npos);
    EXPECT_NE(output.find("МАРШРУТ 3"), std::string::npos);
}

// InputManager — tests via cin substitution

#include "input_manager.h"

// An auxiliary class for testing with cin substitution
class InputManagerCinTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = MakeTempDir("im_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));
        cache_path_ = dir_ / "cities_cache.json";

        // We are writing a test cache of cities
        json j;
        j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
        j["cities"] = {
            {"москва",          "c213"},
            {"воронеж",         "c193"},
            {"санкт-петербург", "c2"},
            {"екатеринбург",    "c54"},
            {"казань",          "c43"}
        };
        std::ofstream f(cache_path_);
        f << j.dump(1);
        f.close();
    }

    void TearDown() override {
        // Restoring the cin
        std::cin.rdbuf(original_cin_);
        RemoveTempDir(dir_);
    }

    // Replacing cin with the string
    void SetCin(const std::string& input) {
        stream_ = std::make_unique<std::istringstream>(input);
        original_cin_ = std::cin.rdbuf(stream_->rdbuf());
    }

    std::filesystem::path dir_;
    std::filesystem::path cache_path_;
    std::unique_ptr<std::istringstream> stream_;
    std::streambuf* original_cin_ = nullptr;
};

TEST_F(InputManagerCinTest, AskMenuValidChoice) {
    TestCacheCities cache(cache_path_);

    char prog[] = "test";
    char* argv[] = {prog};
    InputManager mgr(1, argv, cache);

    SetCin("2\n");

    testing::internal::CaptureStdout();
    testing::internal::GetCapturedStdout();
}

TEST_F(InputManagerCinTest, CollectInputValidCities) {
    TestCacheCities cache(cache_path_);

    char prog[] = "test";
    char* argv[] = {prog};
    InputManager mgr(1, argv, cache);

    SetCin("воронеж\nмосква\nзавтра\n4\n");

    testing::internal::CaptureStdout();
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(cache.GetCity("воронеж").has_value());
    EXPECT_TRUE(cache.GetCity("москва").has_value());
    EXPECT_EQ(cache.GetCityCode("воронеж"), "c193");
    EXPECT_EQ(cache.GetCityCode("москва"), "c213");
}

TEST_F(InputManagerCinTest, LevenshteinDistanceSuggestions) {
    TestCacheCities cache(cache_path_);
    auto all = cache.GetAllCities();

    City typo("масква");
    bool found_moscow = false;
    for (const auto& city : all) {
        if (City::Levenshtein(typo, city) <= 8) {
            if (city.GetName() == "москва") {
                found_moscow = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_moscow);
}

TEST(InputManagerArgvTest, DateOverrideFromArgv) {
    auto dir = MakeTempDir("argv_test");
    auto cache_path = dir / "cities_cache.json";
    json j;
    j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
    j["cities"] = {{"москва", "c213"}};
    std::ofstream f(cache_path);
    f << j.dump(1);
    f.close();

    TestCacheCities cache(cache_path);

    char prog[]  = "test";
    char date_arg[] = "--date=28.04.2026";
    char* argv[] = {prog, date_arg};

    InputManager mgr(2, argv, cache);

    RemoveTempDir(dir);
}

TEST(InputManagerArgvTest, InvalidDateArgv) {
    auto dir = MakeTempDir("argv_bad");
    auto cache_path = dir / "cities_cache.json";
    json j;
    j["last_update"] = TimeManager::FormatYearMonth(TimeManager::GetToday());
    j["cities"] = json::object();
    std::ofstream f(cache_path);
    f << j.dump(1);
    f.close();

    TestCacheCities cache(cache_path);

    char prog[]  = "test";
    char bad_date[] = "--date=notadate";
    char* argv[] = {prog, bad_date};

    testing::internal::CaptureStderr();
    EXPECT_NO_THROW({
        InputManager mgr(2, argv, cache);
    });
    testing::internal::GetCapturedStderr();

    RemoveTempDir(dir);
}

// new tests

TEST_F(InputManagerCinTest, FindSuggestionsLogic) {
    TestCacheCities cache(cache_path_);
    char prog[] = "test";
    char* argv[] = {prog};
    InputManager mgr(1, argv, cache);

    SetCin("1\n");
    City out;
    
    testing::internal::CaptureStdout();
    testing::internal::GetCapturedStdout();
}

TEST_F(InputManagerCinTest, HandleNotFoundMenu) {
    TestCacheCities cache(cache_path_);
    InputManager mgr(1, nullptr, cache);
    
    TimeManager::Date date = TimeManager::GetToday();
    int transfers = 0;

    SetCin("1\n2\n"); 
}

TEST(PrinterTest, AllTransportTypesCoverage) {
    std::vector<std::string> types = {
        "plane", "train", "suburban", "bus", "water", "helicopter", "unknown"
    };
    
    for (const auto& type : types) {
        Trip t;
        t.from_city = "A"; t.to_city = "B";
        TripLeg leg;
        leg.transport_type = type;
        leg.departure = "2026-04-28T10:00:00+03:00";
        leg.arrival = "2026-04-28T11:00:00+03:00";
        t.legs.push_back(leg);
        
        testing::internal::CaptureStdout();
        Printer::PrintTrip(t, 1);
        testing::internal::GetCapturedStdout();
    }
}

static std::string TodayStr() {
    return TimeManager::FormatYearMonthDay(TimeManager::GetToday());
}

TEST_F(CacheTripsTest, ParseRealVoronezhSpbJson) {
    std::string today = TodayStr();
    
    json real_data = {
        {"last_update", today},
        {"trips", json::array({
            {
                {"from_city", "Воронеж"},
                {"to_city", "Санкт-Петербург"},
                {"total_duration", 84120},
                {"transfers_count", 0},
                {"legs", json::array({
                    {
                        {"transport_type", "train"},
                        {"number", "251С"},
                        {"carrier", "РЖД/ФПК"},
                        {"departure", "2026-04-28T06:10:00+03:00"},
                        {"arrival", "2026-04-29T05:32:00+03:00"},
                        {"city_from", "Воронеж"},
                        {"city_to", "Санкт-Петербург"},
                        {"duration_seconds", 84120},
                        {"transfer_wait_seconds", 0},
                        {"transfer_city", ""}
                    }
                })}
            }
        })}
    };

    WriteCache(real_data);

    TestCacheTrips cache(path_, "Воронеж", "Санкт-Петербург");
    
    ASSERT_FALSE(cache.NeedUpdateCache());

    auto trips = cache.GetTripSchedule();
    ASSERT_FALSE(trips.empty());
    EXPECT_EQ(trips[0].legs[0].number, "251С");
}

TEST_F(CacheTripsTest, ParseSegmentDirectNoDetails) {
    std::string today = TodayStr();
    
    json cached_format = {
        {"last_update", today},
        {"trips", json::array({
            {
                {"from_city", "A"},
                {"to_city", "B"},
                {"total_duration", 1000},
                {"transfers_count", 0},
                {"legs", json::array({
                    {
                        {"transport_type", "bus"},
                        {"number", "123"},
                        {"city_from", "A"},
                        {"city_to", "B"},
                        {"duration_seconds", 1000}
                    }
                })}
            }
        })}
    };
    
    WriteCache(cached_format);
    TestCacheTrips cache(path_, "A", "B");
    
    auto trips = cache.GetTripSchedule();
    ASSERT_EQ(trips.size(), 1u);
    EXPECT_EQ(trips[0].legs[0].transport_type, "bus");
}

TEST_F(CacheTripsTest, DeleteOldCacheFiles) {
    std::filesystem::path trips_dir = dir_ / "cache" / "trips";
    std::filesystem::create_directories(trips_dir);
    
    for(int i = 0; i < 25; ++i) {
        std::ofstream f(trips_dir / ("test" + std::to_string(i) + ".json"));
        f << "{}";
    }
    
    EXPECT_NO_THROW({
        TestCacheTrips cache(path_, "A", "B");
    });
}

TEST_F(InputManagerCinTest, SuggestionsFlow) {
    TestCacheCities cache(cache_path_);
    char prog[] = "test";
    char* argv[] = {prog};
    InputManager mgr(1, argv, cache);

    SetCin("масква\n1\nворонеж\nсегодня\n4\n1\nn\n");

    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(mgr.Run());
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Возможно, вы имели в виду"), std::string::npos);
}


TEST(TimeManagerTest, ParseRelativeDates) {
    EXPECT_TRUE(TimeManager::ParseDate("сегодня").has_value());
    EXPECT_TRUE(TimeManager::ParseDate("завтра").has_value());
    EXPECT_TRUE(TimeManager::ParseDate("послезавтра").has_value());
    
    auto past = TimeManager::ParseDate("01.01.2000");
    EXPECT_FALSE(past.has_value());
}

TEST(UTF8Test, InvalidSequences) {
    std::string invalid = "D0"; 
    invalid[0] = (char)0xD0;
    auto cps = UTF8::ToCodepoints(invalid);
    EXPECT_NO_THROW(UTF8::ToLower(invalid));
}



int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}