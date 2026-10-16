#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace docscan {

/// A float tensor read from a checkpoint, stored contiguously in row-major order.
struct Tensor {
    std::vector<std::int64_t> shape;
    std::vector<float> data;

    std::int64_t numel() const;
};

/// Reads the float tensors of a state dict saved by PyTorch's torch.save (the zip format of PyTorch 1.6 and
/// later) without PyTorch: a small zip reader and a pickle interpreter that knows just enough of the format.
/// `entry` names the dict inside the saved object that holds the state dict, such as "model_state";
/// empty when the saved object is the state dict itself. Integer tensors (like BatchNorm's batch counter) are
/// skipped. Throws std::runtime_error when the file is not such a checkpoint.
std::map<std::string, Tensor> loadTorchStateDict(const std::filesystem::path& path, const std::string& entry);

} // namespace docscan
