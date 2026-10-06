#include "core/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace docscan;

TEST_CASE("numbers inside names sort by value")
{
    CHECK(text::naturalLess("page2.jpg", "page10.jpg"));
    CHECK_FALSE(text::naturalLess("page10.jpg", "page2.jpg"));
    CHECK(text::naturalLess("IMG_001.jpg", "IMG_02.jpg"));
    CHECK(text::naturalLess("a.jpg", "b.jpg"));
    CHECK_FALSE(text::naturalLess("same", "same"));
}

TEST_CASE("suggestions only offer close matches")
{
    const std::vector<std::string> names{"crop", "contrast", "resize"};
    CHECK(text::suggestion("crpo", names) == " (did you mean 'crop'?)");
    CHECK(text::suggestion("resise", names) == " (did you mean 'resize'?)");
    CHECK(text::suggestion("xyz", names).empty());
}
