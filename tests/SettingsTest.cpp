#include "core/Error.hpp"
#include "core/Settings.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace docscan;
using Catch::Matchers::ContainsSubstring;

namespace {

const std::vector<ParamSpec> kSpecs{
    ParamSpec::integer("size", 10, 1, 100, ""),
    ParamSpec::number("ratio", 0.5, 0, 1, ""),
    ParamSpec::choice("mode", {"fast", "slow"}, ""),
    ParamSpec::boolean("on", false, ""),
};

} // namespace

TEST_CASE("defaults are filled in")
{
    const Settings settings("test", kSpecs, {});
    CHECK(settings.get<int>("size") == 10);
    CHECK(settings.get<double>("ratio") == 0.5);
    CHECK(settings.get<std::string>("mode") == "fast");
    CHECK_FALSE(settings.get<bool>("on"));
}

TEST_CASE("given values are parsed by type")
{
    const Settings settings("test", kSpecs, {{"size", "42"}, {"ratio", "0.25"}, {"mode", "slow"}, {"on", "yes"}});
    CHECK(settings.get<int>("size") == 42);
    CHECK(settings.get<double>("ratio") == 0.25);
    CHECK(settings.get<std::string>("mode") == "slow");
    CHECK(settings.get<bool>("on"));
}

TEST_CASE("bad values are rejected")
{
    const Arguments bad[] = {{{"size", "abc"}}, {{"size", "0"}},  {{"size", "1.5"}},
                             {{"ratio", "2"}},  {{"mode", "x"}}, {{"on", "maybe"}}};
    for (const auto& arguments : bad)
        CHECK_THROWS_AS(Settings("test", kSpecs, arguments), UsageError);
    CHECK_THROWS_WITH(Settings("test", kSpecs, {{"size", "0"}}),
                      ContainsSubstring("test.size: must be between 1 and 100"));
}

TEST_CASE("unknown keys suggest the closest setting")
{
    CHECK_THROWS_WITH(Settings("test", kSpecs, {{"sise", "1"}}), ContainsSubstring("did you mean 'size'"));
}
