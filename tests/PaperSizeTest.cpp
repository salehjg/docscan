#include "core/Error.hpp"
#include "export/PaperSize.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace docscan;
using Catch::Matchers::WithinAbs;

TEST_CASE("a portrait page is centered on portrait paper")
{
    const Placement p = PaperSize::fromName("a4").place({1000, 2000});
    CHECK_THAT(p.pageWidth, WithinAbs(595.276, 1e-3));
    CHECK_THAT(p.pageHeight, WithinAbs(841.890, 1e-3));
    CHECK_THAT(p.height, WithinAbs(841.890, 1e-3));
    CHECK_THAT(p.width, WithinAbs(420.945, 1e-3));
    CHECK_THAT(p.x, WithinAbs((595.276 - 420.945) / 2, 1e-3));
    CHECK_THAT(p.y, WithinAbs(0, 1e-9));
}

TEST_CASE("a landscape page gets landscape paper")
{
    const Placement p = PaperSize::fromName("letter").place({2200, 1700});
    CHECK_THAT(p.pageWidth, WithinAbs(792, 1e-9));
    CHECK_THAT(p.pageHeight, WithinAbs(612, 1e-9));
}

TEST_CASE("fit pages take the image's shape")
{
    const Placement p = PaperSize::fromName("fit").place({1000, 2000});
    CHECK_THAT(p.pageHeight, WithinAbs(841.890, 1e-3));
    CHECK_THAT(p.pageWidth, WithinAbs(420.945, 1e-3));
    CHECK_THAT(p.x, WithinAbs(0, 1e-9));
}

TEST_CASE("unknown paper sizes are rejected")
{
    CHECK_THROWS_AS(PaperSize::fromName("a3"), UsageError);
}
