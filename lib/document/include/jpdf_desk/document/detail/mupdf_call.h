#pragma once

#include <mupdf/fitz.h>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace mupdf {
// Keep each longjmp boundary limited to C calls. The callable must not create
// C++ objects needing destruction, throw C++ exceptions, or own resources.
// Perform Qt conversions/allocations outside call(), and guard owned handles
// there: MuPDF errors are translated only after its exception stack is popped.
template<typename Action>
auto call(fz_context *ctx, Action action) -> std::invoke_result_t<Action>
{
    static_assert(std::is_nothrow_invocable_v<Action>);
    using Result = std::invoke_result_t<Action>;
    static_assert(std::is_void_v<Result> || std::is_trivial_v<Result>);
    std::conditional_t<std::is_void_v<Result>, bool, Result> result{};
    int failed = 0;
    fz_var(result);
    fz_try(ctx) {
        if constexpr (std::is_void_v<Result>)
            action();
        else
            result = action();
    }
    fz_catch(ctx) { failed = 1; }
    if (failed) throw std::runtime_error(fz_caught_message(ctx));
    if constexpr (!std::is_void_v<Result>) return result;
}

// Use only outside fz_try: ordinary C++ unwinding is safe after call() returns.
template<typename T, typename Drop>
auto own(fz_context *ctx, T *handle, Drop drop)
{
    auto deleter = [ctx, drop](T *value) noexcept { drop(ctx, value); };
    return std::unique_ptr<T, decltype(deleter)>(handle, deleter);
}
}
