#include "ml/TorchCheckpoint.hpp"

#include <cstring>
#include <format>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace docscan {

std::int64_t Tensor::numel() const
{
    std::int64_t n = 1;
    for (const auto d : shape)
        n *= d;
    return n;
}

namespace {

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error("model checkpoint: " + message); }

template <typename T>
T readLittle(const char* p)
{
    T value;
    std::memcpy(&value, p, sizeof(T)); // x86 and ARM Linux are little-endian, like the format
    return value;
}

/// The stored (uncompressed) members of a zip archive, as torch.save writes them.
class ZipArchive {
public:
    explicit ZipArchive(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            fail(std::format("cannot open {}", path.string()));
        bytes_.assign(std::istreambuf_iterator<char>(file), {});

        // The end-of-central-directory record sits at the very end, before an optional comment.
        constexpr std::size_t kEndRecord = 22;
        if (bytes_.size() < kEndRecord)
            fail("not a zip file");
        std::size_t end = bytes_.size() - kEndRecord;
        while (readLittle<std::uint32_t>(&bytes_[end]) != 0x06054b50) {
            if (end == 0 || bytes_.size() - end > kEndRecord + 0xffff)
                fail("not a zip file");
            --end;
        }
        const auto count = readLittle<std::uint16_t>(&bytes_[end + 10]);
        std::size_t entry = readLittle<std::uint32_t>(&bytes_[end + 16]);
        for (int i = 0; i < count; ++i) {
            if (entry + 46 > bytes_.size() || readLittle<std::uint32_t>(&bytes_[entry]) != 0x02014b50)
                fail("damaged zip directory");
            const auto method = readLittle<std::uint16_t>(&bytes_[entry + 10]);
            const auto size = readLittle<std::uint32_t>(&bytes_[entry + 20]);
            const auto nameLength = readLittle<std::uint16_t>(&bytes_[entry + 28]);
            const auto extraLength = readLittle<std::uint16_t>(&bytes_[entry + 30]);
            const auto commentLength = readLittle<std::uint16_t>(&bytes_[entry + 32]);
            const auto local = readLittle<std::uint32_t>(&bytes_[entry + 42]);
            std::string name(&bytes_[entry + 46], nameLength);
            if (method == 0) {
                if (std::size_t(local) + 30 > bytes_.size())
                    fail("damaged zip member");
                const std::size_t data = local + 30 + readLittle<std::uint16_t>(&bytes_[local + 26])
                                       + readLittle<std::uint16_t>(&bytes_[local + 28]);
                if (data + size > bytes_.size())
                    fail("damaged zip member");
                members_[name] = {data, size};
            }
            entry += 46 + nameLength + extraLength + commentLength;
        }
    }

    /// The member whose name ends with `suffix` (torch.save puts everything in one top folder).
    std::string_view member(std::string_view suffix) const
    {
        for (const auto& [name, span] : members_) {
            if (name.size() >= suffix.size() && name.ends_with(suffix)
                && (name.size() == suffix.size() || name[name.size() - suffix.size() - 1] == '/'))
                return {&bytes_[span.first], span.second};
        }
        fail(std::format("missing {} (only uncompressed archives are read)", suffix));
    }

private:
    std::vector<char> bytes_;
    std::map<std::string, std::pair<std::size_t, std::size_t>> members_;
};

struct Value;
using Ref = std::shared_ptr<Value>;
struct Mark {};
struct Global {
    std::string name;
};
struct Storage {
    std::string key;
    std::string type;
};
struct TensorRef {
    Storage storage;
    std::int64_t offset;
    std::vector<std::int64_t> shape;
    std::vector<std::int64_t> stride;
};
using Tuple = std::vector<Ref>;
using Dict = std::vector<std::pair<Ref, Ref>>;

/// A Python object, as far as state dicts need them (None is std::monostate).
struct Value {
    using Variant = std::variant<std::monostate, Mark, bool, std::int64_t, double, std::string, //
                                 Global, Storage, TensorRef, Tuple, Dict>;
    Variant v;
};

Ref make(auto value) { return std::make_shared<Value>(Value{std::move(value)}); }

/// Just enough of Python's unpickler for the state dicts torch.save writes.
class Unpickler {
public:
    explicit Unpickler(std::string_view data) : data_(data) {}

    Ref run()
    {
        while (true) {
            const auto op = std::uint8_t(next(1)[0]);
            switch (op) {
            case 0x80: next(1); break; // PROTO
            case '}': push(make(Dict{})); break;
            case ')': push(make(Tuple{})); break;
            case '(': push(make(Mark{})); break;
            case 'N': push(make(std::monostate{})); break;
            case 0x88: push(make(true)); break;
            case 0x89: push(make(false)); break;
            case 'K': push(make(std::int64_t(std::uint8_t(next(1)[0])))); break;
            case 'M': push(make(std::int64_t(readLittle<std::uint16_t>(next(2).data())))); break;
            case 'J': push(make(std::int64_t(readLittle<std::int32_t>(next(4).data())))); break;
            case 'G': {
                const auto raw = next(8);
                std::uint64_t bits = 0;
                for (const char c : raw)
                    bits = (bits << 8) | std::uint8_t(c); // big-endian
                double value;
                std::memcpy(&value, &bits, 8);
                push(make(value));
                break;
            }
            case 'X': push(make(std::string(next(readLittle<std::uint32_t>(next(4).data()))))); break;
            case 0x8c: push(make(std::string(next(std::uint8_t(next(1)[0]))))); break;
            case 'c': {
                const std::string module(line());
                push(make(Global{module + "." + std::string(line())}));
                break;
            }
            case 'q': memo_[std::uint8_t(next(1)[0])] = top(); break;
            case 'r': memo_[readLittle<std::uint32_t>(next(4).data())] = top(); break;
            case 0x94: memo_[memo_.size()] = top(); break; // MEMOIZE
            case 'h': push(memo(std::uint8_t(next(1)[0]))); break;
            case 'j': push(memo(readLittle<std::uint32_t>(next(4).data()))); break;
            case 't': push(make(popToMark())); break;
            case 0x85: push(make(Tuple{pop()})); break;
            case 0x86: {
                auto b = pop();
                push(make(Tuple{pop(), b}));
                break;
            }
            case 0x87: {
                auto c = pop();
                auto b = pop();
                push(make(Tuple{pop(), b, c}));
                break;
            }
            case 'Q': push(persistent(pop())); break; // BINPERSID
            case 'R': {
                auto args = pop();
                push(reduce(pop(), args));
                break;
            }
            case 's': {
                auto value = pop();
                auto key = pop();
                dict(top()).emplace_back(key, value);
                break;
            }
            case 'u': {
                auto items = popToMark();
                auto& d = dict(top());
                for (std::size_t i = 0; i + 1 < items.size(); i += 2)
                    d.emplace_back(items[i], items[i + 1]);
                break;
            }
            case 'b': pop(); break; // BUILD: object state, not needed for a state dict
            case '.': return pop();
            default: fail(std::format("unsupported pickle opcode 0x{:02x}", op));
            }
        }
    }

private:
    std::string_view next(std::size_t n)
    {
        if (pos_ + n > data_.size())
            fail("truncated pickle");
        const auto out = data_.substr(pos_, n);
        pos_ += n;
        return out;
    }
    std::string_view line()
    {
        const auto end = data_.find('\n', pos_);
        if (end == std::string_view::npos)
            fail("truncated pickle");
        const auto out = data_.substr(pos_, end - pos_);
        pos_ = end + 1;
        return out;
    }
    void push(Ref value) { stack_.push_back(std::move(value)); }
    Ref top() const
    {
        if (stack_.empty())
            fail("pickle stack underflow");
        return stack_.back();
    }
    Ref pop()
    {
        auto value = top();
        stack_.pop_back();
        return value;
    }
    Ref memo(std::size_t index) const
    {
        const auto found = memo_.find(index);
        if (found == memo_.end())
            fail("bad pickle memo reference");
        return found->second;
    }
    Tuple popToMark()
    {
        Tuple items;
        while (!std::holds_alternative<Mark>(top()->v))
            items.insert(items.begin(), pop());
        pop();
        return items;
    }
    static Dict& dict(const Ref& value)
    {
        if (auto* d = std::get_if<Dict>(&value->v))
            return *d;
        fail("pickle sets an item on something that is not a dict");
    }
    template <typename T>
    static const T& as(const Ref& value)
    {
        if (const auto* x = std::get_if<T>(&value->v))
            return *x;
        fail("unexpected value in pickle");
    }
    static std::vector<std::int64_t> ints(const Ref& value)
    {
        std::vector<std::int64_t> out;
        for (const auto& item : as<Tuple>(value))
            out.push_back(as<std::int64_t>(item));
        return out;
    }

    static Ref persistent(const Ref& id)
    {
        // ('storage', torch.FloatStorage, key, location, numel)
        const auto& t = as<Tuple>(id);
        if (t.size() < 3 || as<std::string>(t[0]) != "storage")
            fail("unexpected persistent id");
        return make(Storage{as<std::string>(t[2]), as<Global>(t[1]).name});
    }

    static Ref reduce(const Ref& callable, const Ref& args)
    {
        const auto& name = as<Global>(callable).name;
        const auto& a = as<Tuple>(args);
        if (name == "collections.OrderedDict")
            return make(Dict{});
        if (name == "torch._utils._rebuild_tensor_v2" && a.size() >= 4)
            return make(TensorRef{as<Storage>(a[0]), as<std::int64_t>(a[1]), ints(a[2]), ints(a[3])});
        if (name == "torch._utils._rebuild_parameter" && !a.empty())
            return a[0];
        fail(std::format("unsupported object {} in pickle", name));
    }

    std::string_view data_;
    std::size_t pos_ = 0;
    std::vector<Ref> stack_;
    std::unordered_map<std::size_t, Ref> memo_;
};

Tensor readTensor(const ZipArchive& zip, const TensorRef& ref)
{
    Tensor tensor;
    tensor.shape = ref.shape;
    // Contiguous row-major strides, the only layout state dicts use in practice.
    std::int64_t expected = 1;
    for (std::size_t i = ref.shape.size(); i-- > 0;) {
        if (ref.shape[i] != 1 && ref.stride[i] != expected)
            fail("non-contiguous tensor");
        expected *= ref.shape[i];
    }
    const auto bytes = zip.member("data/" + ref.storage.key);
    const auto n = tensor.numel();
    if (std::int64_t(bytes.size()) < (ref.offset + n) * 4)
        fail("tensor runs past its storage");
    tensor.data.resize(std::size_t(n));
    std::memcpy(tensor.data.data(), bytes.data() + ref.offset * 4, std::size_t(n) * 4);
    return tensor;
}

} // namespace

std::map<std::string, Tensor> loadTorchStateDict(const std::filesystem::path& path, const std::string& entry)
{
    const ZipArchive zip(path);
    Ref root = Unpickler(zip.member("data.pkl")).run();
    if (!entry.empty()) {
        Ref found;
        for (const auto& [key, value] : std::get<Dict>(root->v)) {
            if (const auto* s = std::get_if<std::string>(&key->v); s && *s == entry)
                found = value;
        }
        if (!found)
            fail(std::format("no '{}' in {}", entry, path.string()));
        root = found;
    }
    const auto* dict = std::get_if<Dict>(&root->v);
    if (!dict)
        fail("not a state dict");

    std::map<std::string, Tensor> tensors;
    for (const auto& [key, value] : *dict) {
        const auto* name = std::get_if<std::string>(&key->v);
        const auto* ref = std::get_if<TensorRef>(&value->v);
        if (name && ref && ref->storage.type == "torch.FloatStorage")
            tensors[*name] = readTensor(zip, *ref);
    }
    return tensors;
}

} // namespace docscan
