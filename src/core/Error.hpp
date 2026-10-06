#pragma once

#include <stdexcept>

namespace docscan {

/// Base class of every error docscan reports to the user.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// The command line asks for something invalid (exit code 2).
class UsageError : public Error {
public:
    using Error::Error;
};

/// Reading, processing or writing pages failed (exit code 1).
class ProcessingError : public Error {
public:
    using Error::Error;
};

} // namespace docscan
