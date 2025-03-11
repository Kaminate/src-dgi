#pragma once

#include <string>
#include <stdarg.h> /* va_start, etc */

namespace wyre {

/* Container namespace for result types */
namespace types {

template <typename T>
struct ok {
    ok(const T& val) : val(val) {}
    ok(T&& val) : val(std::move(val)) {}

    T val;
};

template <>
struct ok<void> {};

struct err {};

}  // namespace types

namespace hidden {

thread_local extern std::string error;

}

/* Functions for making an Ok result */
template <typename T, typename CT = typename std::decay<T>::type>
inline types::ok<CT> Ok(T&& val) {
    return types::ok<CT>(std::forward<T>(val));
}

/* Functions for making a void Ok result */
inline types::ok<void> Ok() { return types::ok<void>(); }

/* Functions for making an Err result */
types::err Err(const std::string fmt, ...);
types::err Err(const char* msg);

/**
 * @brief Rust inspired `Result<T>` type for propagating errors.
 *
 * @warning This version of `Result<T>` doesn't have a configurable `Error` type.
 * Also, only 1 `Error` can exist at once on a thread, it is stored as a `static thread_local`.
 */
template <typename T>
class Result {
    /* '0' = Ok, '>0' = Err */
    uint8_t ok = 0u;
    T inner;

   public:
    Result() : ok(0u) {}
    Result(types::ok<T>&& ok) : ok(0u) { inner = std::move(ok.val); }
    Result(types::err&& err) : ok(1u) {}

    /** @return True if the result contains an error. */
    inline bool is_err() const { return ok > 0u; }
    /** @return True if the result contains a value. */
    inline bool is_ok() const { return !is_err(); }

    /**
     * @brief Boolean operator.
     * @return True if this result is ok.
     */
    inline explicit operator bool() const { return is_ok(); }

    /**
     * @brief <UNSAFE> Way to extract value from result.
     * (Will terminate the program if the result is an error)
     */
    inline T unwrap(bool noexpect = false) const {
        if (noexpect == false && is_err()) {
            std::fprintf(stderr, "unwrap failed!\nreason: %s\n", hidden::error.c_str());
            std::terminate();
        }
        return inner;
    }

    /**
     * @brief <UNSAFE> Way to extract value from result.
     * (Will terminate the program if the result is an error)
     */
    inline T expect(std::string_view msg, bool noexpect = false) const {
        if (noexpect == false && is_err()) {
            std::fprintf(stderr, "error: %s\nreason: %s\n", msg.data(), hidden::error.c_str());
            std::terminate();
        }
        return inner;
    }

    /**
     * @brief Way to extract value from result.
     * (Returns the default value if the result is an error)
     */
    inline T unwrap_or_default() const {
        if (is_err()) return T{};
        return inner;
    }

    /**
     * @brief <UNSAFE> Way to extract error from result.
     * (Will terminate the program if the result is not an error)
     */
    inline const std::string& unwrap_err() const {
        if (is_ok()) {
            std::fprintf(stderr, "unwrap_err failed!\n");
            std::terminate();
        }
        return hidden::error;
    }
};

/**
 * @brief Rust inspired `Result<void>` type for propagating errors.
 *
 * @warning This version of `Result<void>` doesn't have a configurable `Error` type.
 * Also, only 1 `Error` can exist at once on a thread, it is stored as a `static thread_local`.
 */
template <>
class Result<void> {
    /* '0' = Ok, '>0' = Err */
    uint8_t ok = 0u;

   public:
    Result() : ok(0u) {}
    Result(types::ok<void>&& ok) : ok(0u) {}
    Result(types::err&& err) : ok(1u) {}

    /** @return True if the result contains an error. */
    inline bool is_err() const { return ok > 0u; }
    /** @return True if the result contains a value. */
    inline bool is_ok() const { return !is_err(); }

    /**
     * @brief Boolean operator.
     * @return True if this result is ok.
     */
    inline explicit operator bool() const { return is_ok(); }

    /**
     * @brief <UNSAFE> Way to extract value from result.
     * (Will terminate the program if the result is an error)
     */
    inline void unwrap() const {
        if (is_err()) {
            std::fprintf(stderr, "unwrap failed!\nreason: %s\n", hidden::error.c_str());
            std::terminate();
        }
    }

    /**
     * @brief <UNSAFE> Way to extract value from result.
     * (Will terminate the program if the result is an error)
     */
    inline void expect(std::string_view msg) const {
        if (is_err()) {
            std::fprintf(stderr, "error: %s\nreason: %s\n", msg.data(), hidden::error.c_str());
            std::terminate();
        }
    }

    /**
     * @brief <UNSAFE> Way to extract error from result.
     * (Will terminate the program if the result is not an error)
     */
    inline const std::string& unwrap_err() const {
        if (is_ok()) {
            std::fprintf(stderr, "unwrap_err failed!\n");
            std::terminate();
        }
        return hidden::error;
    }
};

}  // namespace wyre
