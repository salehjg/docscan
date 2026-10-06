#include "core/Error.hpp"
#include "pipeline/PipelineBuilder.hpp"
#include "pipeline/Profile.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace docscan;
using Catch::Matchers::ContainsSubstring;

namespace {

const FilterRegistry& registry() { return FilterRegistry::builtin(); }
const Profile& grayProfile() { return ProfileRegistry::builtin().find("grey"); }

std::vector<FilterSpec> specs(std::initializer_list<const char*> texts)
{
    std::vector<FilterSpec> result;
    for (const char* text : texts)
        result.push_back(FilterSpec::parse(text));
    return result;
}

} // namespace

TEST_CASE("a profile's chain is resolved to canonical specs")
{
    PipelineBuilder builder(registry());
    builder.start(grayProfile().chain);
    CHECK(describe(builder.chain()) == "crop → resize:max=2400 → gray → light → contrast:amount=1.3 → sharpen");
}

TEST_CASE("set (-s) changes a setting of a profile step")
{
    PipelineBuilder builder(registry());
    builder.start(grayProfile().chain).set("resize.max=1600");
    CHECK(builder.chain()[1].str() == "resize:max=1600");
    builder.set("resize=1200").set("contrast.amount=1.5");
    CHECK(builder.chain()[1].str() == "resize:max=1200");
    CHECK(builder.chain()[4].str() == "contrast:amount=1.5");
}

TEST_CASE("skip (-x) removes and add (-a) appends steps")
{
    PipelineBuilder builder(registry());
    builder.start(grayProfile().chain).skip("sharpen").skip("grey").add(FilterSpec::parse("rotate:90"));
    CHECK(describe(builder.chain()) == "crop → resize:max=2400 → light → contrast:amount=1.3 → rotate:angle=90");
}

TEST_CASE("the order given with -f is kept")
{
    PipelineBuilder builder(registry());
    builder.start(specs({"crop", "bw", "resize:1000"}));
    CHECK(describe(builder.chain()) == "crop → bw → resize:max=1000");
    CHECK(builder.build().size() == 3);
}

TEST_CASE("changing a filter that is not in the chain explains how to add it")
{
    PipelineBuilder builder(registry());
    builder.start(grayProfile().chain);
    CHECK_THROWS_WITH(builder.set("bw.method=otsu"), ContainsSubstring("add it with -a bw"));
    CHECK_THROWS_AS(builder.skip("rotate"), UsageError);
}

TEST_CASE("mistakes are reported with suggestions")
{
    PipelineBuilder builder(registry());
    builder.start(grayProfile().chain);
    CHECK_THROWS_WITH(builder.add(FilterSpec::parse("blurr")), ContainsSubstring("unknown filter 'blurr'"));
    CHECK_THROWS_WITH(builder.set("resize.maxx=3"), ContainsSubstring("did you mean 'max'"));
    CHECK_THROWS_WITH(builder.set("resize.max=big"), ContainsSubstring("expected a whole number"));
    CHECK_THROWS_WITH(builder.set("resize"), ContainsSubstring("expected FILTER.SETTING=VALUE"));
    CHECK_THROWS_AS(builder.add(FilterSpec::parse("gray:1")), UsageError);
}

TEST_CASE("every built-in profile builds")
{
    for (const auto& profile : ProfileRegistry::builtin().all()) {
        PipelineBuilder builder(registry());
        CHECK(builder.start(profile.chain).build().size() == profile.chain.size());
    }
}
