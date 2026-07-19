#pragma once

#include <string>
#include <utility>
#include <variant>
#include <windows.h>

namespace nfui {

enum class ErrorCode {
    ok,
    invalid_argument,
    invalid_state,
    win32_error,
};

class Error {
public:
    Error() = default;

    Error(ErrorCode code, std::wstring message, DWORD win32_code = ERROR_SUCCESS) noexcept
        : code_(code),
          win32_code_(win32_code),
          message_(std::move(message)) {
    }

    [[nodiscard]] static Error from_win32(DWORD win32_code, std::wstring message) noexcept {
        return Error(ErrorCode::win32_error, std::move(message), win32_code);
    }

    [[nodiscard]] ErrorCode code() const noexcept {
        return code_;
    }

    [[nodiscard]] DWORD win32_code() const noexcept {
        return win32_code_;
    }

    [[nodiscard]] const std::wstring& message() const noexcept {
        return message_;
    }

private:
    ErrorCode code_{ErrorCode::ok};
    DWORD win32_code_{ERROR_SUCCESS};
    std::wstring message_{};
};

template <typename T>
class Result {
public:
    [[nodiscard]] static Result success(T value) noexcept {
        return Result(std::move(value));
    }

    [[nodiscard]] static Result failure(Error error) noexcept {
        return Result(std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(state_);
    }

    [[nodiscard]] const T& value() const noexcept {
        return std::get<T>(state_);
    }

    [[nodiscard]] const Error& error() const noexcept {
        return std::get<Error>(state_);
    }

private:
    explicit Result(T value) noexcept
        : state_(std::move(value)) {
    }

    explicit Result(Error error) noexcept
        : state_(std::move(error)) {
    }

    std::variant<T, Error> state_;
};

} // namespace nfui
