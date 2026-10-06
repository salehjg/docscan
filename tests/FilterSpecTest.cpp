#include "core/Error.hpp"
#include "core/FilterSpec.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace docscan;

TEST_CASE("a bare name has no arguments")
{
    const auto spec = FilterSpec::parse("gray");
    CHECK(spec.name == "gray");
    CHECK(spec.args.empty());
    CHECK(spec.str() == "gray");
}

TEST_CASE("keyed arguments keep their order")
{
    const auto spec = FilterSpec::parse("bw:method=otsu,window=31");
    CHECK(spec.name == "bw");
    CHECK(spec.args == Arguments{{"method", "otsu"}, {"window", "31"}});
    CHECK(spec.str() == "bw:method=otsu,window=31");
}

TEST_CASE("a bare value is stored under the empty key")
{
    const auto spec = FilterSpec::parse("rotate:90");
    CHECK(spec.args == Arguments{{"", "90"}});
    CHECK(spec.str() == "rotate:90");
}

TEST_CASE("a later value replaces an earlier one")
{
    CHECK(FilterSpec::parse("resize:max=1,max=2").args == Arguments{{"max", "2"}});
}

TEST_CASE("malformed specs are rejected")
{
    for (const char* text : {"", ":90", "rotate:", "bw:=otsu", "bw:method=", "bw:method=otsu,"})
        CHECK_THROWS_AS(FilterSpec::parse(text), UsageError);
}
