in_range_ext/in_range_ext.h

This header provides an equivalent to C++20's std::in_range<integer>(value) for floating-point values.

It defines the following in namespace in_range_ext

concept integer

  matches std::integral types other than character types and bool
  (to match std::in_range's type restrictions)

template<integer I, std::floating_point F> constexpr bool in_range(F f)

  returns true iff the floating-point value f is in range for integer type I

For this one function, most of the work is in finding, at compile time, the highest and lowest
floating-point values in the range of the integer type. This cannot be done by direct conversion
in the general case in C++ because of rounding. For example on a typical 32-/64-bit system with

  numeric_limits<int>::digits   31
  numeric_limits<int>::max()    0x7fffffff (2^31 - 1)
  numeric_limits<float>::radix  2
  numeric_limits<float>::digts  24

float(0x7fffffff) rounded to nearest is 0x80000000 (2^31), just outside int's range.

Instead, the code establishes these boundaries "manually" using a decomposed floating-point
representation.

It is designed to work for arbitrary integer width, floating-point radix, and precision,
including the common cases where the floating-point range is much larger, and less common ones
where it may be smaller, for example IEEE half-precision aka binary16.
