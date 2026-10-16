#include "core/Download.hpp"

#include "Version.hpp"
#include "core/Error.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>

#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <system_error>

#include <unistd.h>

namespace docscan {

namespace {

constexpr long kConnectTimeout = 30; // seconds
constexpr long kStallSpeed = 1024;   // bytes per second; slower than this for kStallTime seconds gives up
constexpr long kStallTime = 60;
constexpr long kMaxRedirects = 5;

/// Where the received bytes go: the file being written and the running checksum.
struct Sink {
    std::ofstream file;
    EVP_MD_CTX* hash;
};

/// libcurl's write callback; returning less than it was given makes libcurl stop with an error.
std::size_t receive(char* data, std::size_t size, std::size_t count, void* user)
{
    auto& sink = *static_cast<Sink*>(user);
    const std::size_t bytes = size * count;
    if (!sink.file.write(data, std::streamsize(bytes)) || EVP_DigestUpdate(sink.hash, data, bytes) != 1)
        return 0;
    return bytes;
}

std::string hex(const unsigned char* bytes, unsigned size)
{
    std::string text;
    for (unsigned i = 0; i < size; ++i)
        text += std::format("{:02x}", bytes[i]);
    return text;
}

/// Deletes a file when it goes out of scope, whether or not it still exists.
struct RemoveOnExit {
    const std::filesystem::path& path;
    ~RemoveOnExit()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

} // namespace

void Download::fetch(const std::string& url, const std::filesystem::path& target, std::string_view sha256)
{
    const auto failed = [&](std::string_view reason) {
        return ProcessingError(std::format("cannot download {}: {}", url, reason));
    };

    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error)
        throw failed(std::format("cannot create {}: {}", target.parent_path().string(), error.message()));

    // Written next to the target and renamed over it once checked, so a cut-off download is never used.
    const std::filesystem::path part = std::format("{}.{}.part", target.string(), ::getpid());
    const RemoveOnExit cleanup{part};

    const std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!hash || !curl || EVP_DigestInit_ex(hash.get(), EVP_sha256(), nullptr) != 1)
        throw failed("cannot set up libcurl or OpenSSL");

    Sink sink{std::ofstream(part, std::ios::binary), hash.get()};
    if (!sink.file)
        throw failed(std::format("cannot write {}", part.string()));

    const std::string agent = std::format("docscan/{}", kVersion);
    std::array<char, CURL_ERROR_SIZE> message{};
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, kMaxRedirects);
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeout);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, kStallSpeed);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, kStallTime);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L); // no SIGALRM for DNS timeouts in a threaded program
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, message.data());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &sink);
    if (const CURLcode code = curl_easy_perform(curl.get()); code != CURLE_OK)
        throw failed(message[0] ? message.data() : curl_easy_strerror(code));
    sink.file.close();
    if (!sink.file)
        throw failed(std::format("cannot write {}", part.string()));

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned size = 0;
    if (EVP_DigestFinal_ex(hash.get(), digest.data(), &size) != 1)
        throw failed("cannot compute its checksum");
    if (const std::string actual = hex(digest.data(), size); actual != sha256)
        throw failed(std::format("its SHA-256 is {}, expected {}", actual, sha256));

    std::filesystem::rename(part, target, error);
    if (error)
        throw failed(std::format("cannot save it as {}: {}", target.string(), error.message()));
}

} // namespace docscan
