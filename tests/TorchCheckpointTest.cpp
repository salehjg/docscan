#include "ml/TorchCheckpoint.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace docscan;

namespace {

// Saved by PyTorch 2.14 with torch.save({"epoch": 3, "model_state": state}).
const std::filesystem::path kCheckpoint = std::filesystem::path(DOCSCAN_TEST_DATA) / "tiny_checkpoint.pt";

} // namespace

TEST_CASE("a state dict saved by PyTorch is read without PyTorch")
{
    const auto tensors = loadTorchStateDict(kCheckpoint, "model_state");

    const Tensor& weight = tensors.at("conv.weight");
    CHECK(weight.shape == std::vector<std::int64_t>{2, 1, 3, 3});
    REQUIRE(weight.data.size() == 18);
    for (std::size_t i = 0; i < weight.data.size(); ++i)
        CHECK(weight.data[i] == float(i) / 10);
    CHECK(tensors.at("conv.bias").data == std::vector<float>{0.5f, -1.5f});
    CHECK(tensors.at("bn.running_mean").data == std::vector<float>{1.0f, 2.0f});

    // A view into the middle of a larger storage.
    const Tensor& view = tensors.at("view.weight");
    CHECK(view.shape == std::vector<std::int64_t>{2, 4});
    CHECK(view.data == std::vector<float>{4, 5, 6, 7, 8, 9, 10, 11});

    // Integer tensors, like BatchNorm's batch counter, are left out.
    CHECK_FALSE(tensors.contains("bn.num_batches_tracked"));
}

TEST_CASE("reading a checkpoint fails clearly on other files")
{
    CHECK_THROWS_AS(loadTorchStateDict(kCheckpoint, "no_such_entry"), std::runtime_error);
    CHECK_THROWS_AS(loadTorchStateDict(std::filesystem::path(DOCSCAN_TEST_DATA) / "missing.pt", ""),
                    std::runtime_error);

    const auto notZip = std::filesystem::temp_directory_path() / "docscan-not-a-checkpoint.pt";
    std::ofstream(notZip) << "just some text, not a zip archive";
    CHECK_THROWS_AS(loadTorchStateDict(notZip, ""), std::runtime_error);
    std::filesystem::remove(notZip);
}
