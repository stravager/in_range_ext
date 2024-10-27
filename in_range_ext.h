// This header provides an equivalent to C++20's std::in_range<integer>(value) for floating-point values.
//
// It defines the following in namespace in_range_ext
//
// concept integer
//
//   matches std::integral types other than character types and bool
//   (to match std::in_range's type restrictions)
//
// template<integer I, std::floating_point F> constexpr bool in_range(F f)
//
//   returns true iff the floating-point value f is in range for integer type I
//
// -------------------------------------------------------------------------------------------------
//
// For this one function, most of the work is in finding, at compile time, the highest and lowest
// floating-point values in the range of the integer type. This cannot be done by direct conversion
// in the general case in C++ because of rounding. For example on a typical 32-/64-bit system with
//
//   numeric_limits<int>::digits   31
//   numeric_limits<int>::max()    0x7fffffff (2^31 - 1)
//   numeric_limits<float>::radix  2
//   numeric_limits<float>::digts  24
//
// float(0x7fffffff) rounded to nearest is 0x80000000 (2^31), just outside int's range.
//
// Instead, the code establishes these boundaries "manually" using a decomposed floating-point
// representation.
//
// It is designed to work for arbitrary integer width, floating-point radix, and precision,
// including the common cases where the floating-point range is much larger, and less common ones
// where it may be smaller, for example IEEE half-precision aka binary16.
//
// ---
// 
// MIT License
// 
// Copyright (c) 2024 stravager
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef IN_RANGE_EXT_H
#define IN_RANGE_EXT_H

#include <algorithm>
#include <array>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <version>

namespace in_range_ext
{
// concept integer: integral excluding bool and character types.
template <class T>
concept integer = std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool> && !std::is_same_v<std::remove_cv_t<T>, char> &&
                  !std::is_same_v<std::remove_cv_t<T>, char8_t> && !std::is_same_v<std::remove_cv_t<T>, char16_t> &&
                  !std::is_same_v<std::remove_cv_t<T>, char32_t> && !std::is_same_v<std::remove_cv_t<T>, wchar_t>;

#define IN_RANGE_EXT_ASSERT(expr) (void)(!!(expr) ? 0 : (fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #expr), std::abort(), 0))

namespace detail
{
namespace constexpr_cmath
{
// Need a few constexpr <cmath> functions, but these aren't widely available yet. The
// substitutes are slow, but only used to compute the compile-time constants.
#if __cpp_lib_constexpr_cmath >= 202202L
using std::copysign;
using std::fpclassify;
using std::ilogb;
using std::scalbn;
using std::signbit;
#else // !(__cpp_lib_constexpr_cmath >= 202202L)

// If true, disables forwarding to the std:: equivalents at runtime, for test purposes.
constexpr bool runtime_test = false;

template <std::floating_point F> constexpr bool signbit(F f)
{
    if (!runtime_test && !std::is_constant_evaluated())
        return std::signbit(f);

#if defined __clang__ || defined __GNUC__
    return __builtin_copysign(F(1.0), f) < 0;
#else
    // Works straightforwardly for subnormal/normal/infinity.
    // Best effort for zero (works assuming no unusual padding).
    // Does NOT work for NaNs, which require platform-specific code.
    using fbytes = std::array<std::byte, sizeof(F)>;
    return f < 0 || (f == 0 && std::bit_cast<fbytes>(f) == std::bit_cast<fbytes>(-F(0)));
#endif
}

static_assert(signbit(-float(0)));
static_assert(signbit(-double(0)));
static_assert(signbit(-(long double)(0)));
static_assert(!signbit(+float(0)));
static_assert(!signbit(+double(0)));
static_assert(!signbit(+(long double)(0)));

template <std::floating_point F> constexpr F copysign(F f, F s)
{
    if (!runtime_test && !std::is_constant_evaluated())
        return std::copysign(f, s);

    return signbit(f) == signbit(s) ? f : -f;
}

static_assert(signbit(copysign(float(0), -float(0))));
static_assert(signbit(copysign(double(0), -double(0))));
static_assert(signbit(copysign((long double)(0), -(long double)(0))));
static_assert(!signbit(copysign(-float(0), +float(0))));
static_assert(!signbit(copysign(-double(0), +double(0))));
static_assert(!signbit(copysign(-(long double)(0), +(long double)(0))));

static_assert(signbit(copysign(float(0), -float(1))));
static_assert(signbit(copysign(double(0), -double(1))));
static_assert(signbit(copysign((long double)(0), -(long double)(1))));
static_assert(!signbit(copysign(-float(0), +float(1))));
static_assert(!signbit(copysign(-double(0), +double(1))));
static_assert(!signbit(copysign(-(long double)(0), +(long double)(1))));

static_assert(signbit(copysign(float(1), -float(0))));
static_assert(signbit(copysign(double(1), -double(0))));
static_assert(signbit(copysign((long double)(1), -(long double)(0))));
static_assert(!signbit(copysign(-float(1), +float(0))));
static_assert(!signbit(copysign(-double(1), +double(0))));
static_assert(!signbit(copysign(-(long double)(1), +(long double)(0))));

template <std::floating_point F> constexpr int fpclassify(F f)
{
    if (!runtime_test && !std::is_constant_evaluated())
        return std::fpclassify(f);

    if (f == 0)
        return FP_ZERO;
    else if (-std::numeric_limits<F>::min() < f && f < +std::numeric_limits<F>::min())
        return FP_SUBNORMAL;
    else if (std::numeric_limits<F>::lowest() <= f && f <= std::numeric_limits<F>::max())
        return FP_NORMAL;
    else if (f == f)
        return FP_INFINITE;
    else
        return FP_NAN;
}

template <std::floating_point F> constexpr int ilogb(F f)
{
    if (!runtime_test && !std::is_constant_evaluated())
        return std::ilogb(f);

    if (f == 0)
        return FP_ILOGB0;
    else if (std::numeric_limits<F>::lowest() <= f && f <= std::numeric_limits<F>::max())
    {
        if (f < 0)
            f = -f;

        constexpr F radix = std::numeric_limits<F>::radix;

        int exp = 0;
        for (; f < 1; f *= radix)
            --exp;
        for (; f >= radix; f /= radix)
            ++exp;

        return exp;
    }
    else if (f == f)
        return INT_MAX;
    else
        return FP_ILOGBNAN;
}

template <std::floating_point F> constexpr F scalbn(F f, int exp)
{
    if (!runtime_test && !std::is_constant_evaluated())
        return std::scalbn(f, exp);

    if (f == 0 || !(std::numeric_limits<F>::lowest() <= f && f <= std::numeric_limits<F>::max()))
        return f;

    constexpr F radix = std::numeric_limits<F>::radix;

    for (; exp < 0; ++exp)
        f /= radix;
    for (; exp > 0; --exp)
        f *= radix;

    return f;
}
#endif // !(__cpp_lib_constexpr_cmath >= 202202L)
} // namespace constexpr_cmath

// Decomposed floating point representation for easier manipulation.
template <int radix, int num_digits> struct decomp_rep
{
    int category{};
    bool signbit{};
    std::array<int, num_digits> digits{};
    int ilogb{};
};

// Utilities.
template <int radix, int num_digits> constexpr bool isnan(const decomp_rep<radix, num_digits> &x)
{
    return x.category == FP_NAN;
}

template <int radix, int num_digits> constexpr bool isinf(const decomp_rep<radix, num_digits> &x)
{
    return x.category == FP_INFINITE;
}

template <int radix, int num_digits> constexpr bool iszero(const decomp_rep<radix, num_digits> &x)
{
    return x.category == FP_ZERO;
}

template <int radix, int num_digits> constexpr bool ispos(const decomp_rep<radix, num_digits> &x)
{
    return !x.signbit && !isnan(x) && !iszero(x);
}

template <int radix, int num_digits> constexpr bool isneg(const decomp_rep<radix, num_digits> &x)
{
    return x.signbit && !isnan(x) && !iszero(x);
}

template <int radix, int num_digits> constexpr bool isposinf(const decomp_rep<radix, num_digits> &x)
{
    return !x.signbit && isinf(x);
}

template <int radix, int num_digits> constexpr bool isneginf(const decomp_rep<radix, num_digits> &x)
{
    return x.signbit && isinf(x);
}

// "less" comparator.
template <int radix, int num_digits> constexpr bool operator<(const decomp_rep<radix, num_digits> &lhs, const decomp_rep<radix, num_digits> &rhs)
{
    if (isnan(lhs) || isnan(rhs))
        return false;
    else if (isinf(lhs) || isinf(rhs))
    {
        return (isneginf(lhs) && !isneginf(rhs))     // lhs == -inf && rhs > -inf
               || (!isposinf(lhs) && isposinf(rhs)); // lhs < +inf && rhs == +inf
    }
    else if (iszero(lhs) || iszero(rhs))
    {
        return (iszero(lhs) && ispos(rhs))     // lhs == 0 && rhs > 0
               || (isneg(lhs) && iszero(rhs)); // lhs < 0 && rhs == 0
    }
    else
    {
        // Both values subnormal or normal.
        if (isneg(lhs) != isneg(rhs))
            return isneg(lhs); // lhs < 0 && rhs > 0
        else
        {
            if (lhs.ilogb != rhs.ilogb)
                return isneg(lhs) ? lhs.ilogb > rhs.ilogb : lhs.ilogb < rhs.ilogb;
            else
            {
                for (int d = 0; d < num_digits; ++d)
                    if (lhs.digits[d] != rhs.digits[d])
                        return isneg(lhs) ? lhs.digits[d] > rhs.digits[d] : lhs.digits[d] < rhs.digits[d];
                return false;
            }
        }
    }
}

// Count number of digits required to represent integer i in given radix.
template <int radix, integer I> constexpr int count_digits(I i)
{
    using U = std::make_unsigned_t<I>;
    U u = i < 0 ? U(0) - static_cast<U>(i) : i;
    int num_digits = 1;
    while (u >= radix)
    {
        ++num_digits;
        u /= radix;
    }
    return num_digits;
};

// Helper class for converting floating_point values to and from the decomposed
// representation; and converting from integer, truncating to F's precision if needed.
// (Conversion TO integer is not supported and not needed here).
template <std::floating_point F> class decomp
{
    using float_limits = std::numeric_limits<F>;
    static constexpr int radix = float_limits::radix;
    static constexpr int num_digits = float_limits::digits;

    // Representation.
    decomp_rep<std::numeric_limits<F>::radix, std::numeric_limits<F>::digits> rep;

public:
    constexpr decomp() = default;

    // Conversion from F.
    constexpr explicit decomp(F f) : rep{.category = constexpr_cmath::fpclassify(f), .signbit = constexpr_cmath::signbit(f), .ilogb = constexpr_cmath::ilogb(f)}
    {
        // Need extra handling below if fpclassify can return any non-standard values.
        IN_RANGE_EXT_ASSERT(rep.category == FP_ZERO || rep.category == FP_SUBNORMAL || rep.category == FP_NORMAL || rep.category == FP_INFINITE ||
                            rep.category == FP_NAN);

        if (rep.category == FP_NORMAL || rep.category == FP_SUBNORMAL)
        {
            // Use absolute value for digit extraction.
            if (rep.signbit)
                f = -f;

            // Normalize.
            if (rep.ilogb == INT_MIN) // -rep.ilogb would overflow.
                f = constexpr_cmath::scalbn(f, INT_MAX) * radix;
            else
                f = constexpr_cmath::scalbn(f, -rep.ilogb);

            // Extract digits, most significant first.
            for (int d = 0; d < num_digits; ++d)
            {
                int digit = static_cast<int>(f);
                rep.digits[d] = digit;
                f -= static_cast<F>(digit);
                if (f == 0)
                    break;
                f *= radix;
            }
        }
    }

    // Conversion to F.
    constexpr explicit operator F() const
    {
        F f = 0;
        if (rep.category == FP_ZERO)
            f = 0;
        else if (rep.category == FP_SUBNORMAL || rep.category == FP_NORMAL)
        {
            for (int d = 0; d < num_digits; ++d)
            {
                F digit = static_cast<F>(rep.digits[d]);
                f += constexpr_cmath::scalbn(digit, -d);
            }
            f = constexpr_cmath::scalbn(f, rep.ilogb);
        }
        else if (rep.category == FP_INFINITE)
        {
            IN_RANGE_EXT_ASSERT(float_limits::has_infinity);
            f = float_limits::infinity();
        }
        else if (rep.category == FP_NAN)
        {
            IN_RANGE_EXT_ASSERT(float_limits::has_quiet_NaN);
            f = float_limits::quiet_NaN();
        }

        if (rep.signbit)
            f = constexpr_cmath::copysign(f, F(-1));

        return f;
    }

    // Conversion from integer.
    // Result is truncated, not rounded, to precision of F.
    // Result is infinite (rep.category == FP_INFINITE) if out of range for F.
    template <integer I>
    constexpr explicit decomp(I i)
        : rep{
              .category = i != 0 ? FP_NORMAL : FP_ZERO,
              .signbit = i < 0,
          }
    {
        if (i == 0)
            return;

        const int idigits = count_digits<radix>(i);

        // Use absolute value for digit extraction.
        using U = std::make_unsigned_t<I>;
        U u = i < 0 ? U(0) - static_cast<U>(i) : i;

        // Extract digits, least significant first.
        for (int d = idigits - 1; d >= 0; --d)
        {
            if (d < num_digits) // Drop digits beyond F's precision.
                rep.digits[d] = u % radix;
            u /= radix;
        }
        rep.ilogb = idigits - 1;

        // Handle integer values out of range for F, most likely to be encountered with
        // small floating-point types like IEEE binary16 (max absolute value = 65504).
        constexpr decomp<F> dfmin(float_limits::lowest());
        constexpr decomp<F> dfmax(float_limits::max());
        if (*this < dfmin || dfmax < *this)
        {
            rep.category = FP_INFINITE;
            rep.ilogb = 0;
            rep.digits = {};
        }
    }

    friend constexpr bool operator<(const decomp &lhs, const decomp &rhs)
    {
        return lhs.rep < rhs.rep;
    }
};

// Verify round-trip on some basic values.
using dfloat = decomp<float>;
using float_limits = std::numeric_limits<float>;
constexpr int dfloat_radix = std::numeric_limits<float>::radix;

static_assert(float(dfloat(-0.0f)) == -0.0f && constexpr_cmath::signbit(float(dfloat(-0.0f))));
static_assert(float(dfloat(+0.0f)) == +0.0f && !constexpr_cmath::signbit(float(dfloat(+0.0f))));
static_assert(float(dfloat(-float_limits::denorm_min())) == -float_limits::denorm_min());
static_assert(float(dfloat(+float_limits::denorm_min())) == +float_limits::denorm_min());
static_assert(float(dfloat(-float_limits::min())) == -float_limits::min());
static_assert(float(dfloat(+float_limits::min())) == +float_limits::min());
static_assert(float(dfloat(-1.0f)) == -1.0f);
static_assert(float(dfloat(+1.0f)) == +1.0f);
static_assert(float(dfloat(-float_limits::max())) == -float_limits::max());
static_assert(float(dfloat(+float_limits::max())) == +float_limits::max());

static_assert(float(dfloat(0)) == 0.0f && !constexpr_cmath::signbit(float(dfloat(0))));
static_assert(float(dfloat(-1)) == -1);
static_assert(float(dfloat(+1)) == +1);
static_assert(float(dfloat(-(dfloat_radix - 1))) == -(dfloat_radix - 1));
static_assert(float(dfloat(+(dfloat_radix - 1))) == +(dfloat_radix - 1));
} // namespace detail

// Finally can implement in_range itself.
template <integer I, std::floating_point F> constexpr bool in_range(F f)
{
    using fdecomp = detail::decomp<F>;
    using flimits = std::numeric_limits<F>;
    using ilimits = std::numeric_limits<I>;

    constexpr F min_in_range(std::max(fdecomp(flimits::lowest()), fdecomp(ilimits::lowest())));
    constexpr F max_in_range(std::min(fdecomp(flimits::max()), fdecomp(ilimits::max())));

    return min_in_range <= f && f <= max_in_range;
}

namespace detail
{
// Spot check some typical boundaries.
constexpr bool float_is_binary32 = std::numeric_limits<float>::radix == 2 && std::numeric_limits<float>::digits == 24 && std::numeric_limits<float>::is_iec559;
constexpr bool double_is_binary64 =
    std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits == 53 && std::numeric_limits<double>::is_iec559;

#ifdef INT32_MAX
static_assert(!float_is_binary32 || in_range<int32_t>(float(INT32_MIN)));
static_assert(!float_is_binary32 || !in_range<int32_t>(float(INT32_MAX)));
static_assert(!double_is_binary64 || in_range<int32_t>(double(INT32_MIN)));
static_assert(!double_is_binary64 || in_range<int32_t>(double(INT32_MAX)));
#endif
#ifdef INT64_MAX
static_assert(!float_is_binary32 || in_range<int64_t>(float(INT64_MIN)));
static_assert(!float_is_binary32 || !in_range<int64_t>(float(INT64_MAX)));
static_assert(!double_is_binary64 || in_range<int64_t>(double(INT64_MIN)));
static_assert(!double_is_binary64 || !in_range<int64_t>(double(INT64_MAX)));
#endif
} // namespace detail
} // namespace in_range_ext

#endif // IN_RANGE_EXT_H
