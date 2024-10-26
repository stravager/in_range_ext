#include "in_range_ext.h"

bool in_int_range(float f)
{
    return in_range_ext::in_range<int>(f);
}

bool in_int_range(double f)
{
    return in_range_ext::in_range<int>(f);
}

int main()
{
    using dfloat = in_range_ext::detail::decomp<float>;
    using flimits = std::numeric_limits<float>;
    constexpr int fradix = flimits::radix;

    IN_RANGE_EXT_ASSERT(float(dfloat(-0.0f)) == -0.0f && std::signbit(float(dfloat(-0.0f))));
    IN_RANGE_EXT_ASSERT(float(dfloat(+0.0f)) == +0.0f && !std::signbit(float(dfloat(+0.0f))));

    // may not be true at runtime if subnormals are flushed to zero
    // IN_RANGE_EXT_ASSERT(float(dfloat(-FLT_TRUE_MIN)) == -FLT_TRUE_MIN);
    // IN_RANGE_EXT_ASSERT(float(dfloat(+FLT_TRUE_MIN)) == +FLT_TRUE_MIN);

    IN_RANGE_EXT_ASSERT(float(dfloat(-FLT_MIN)) == -FLT_MIN);
    IN_RANGE_EXT_ASSERT(float(dfloat(+FLT_MIN)) == +FLT_MIN);
    IN_RANGE_EXT_ASSERT(float(dfloat(-1.0f)) == -1.0f);
    IN_RANGE_EXT_ASSERT(float(dfloat(+1.0f)) == +1.0f);
    IN_RANGE_EXT_ASSERT(float(dfloat(-FLT_MAX)) == -FLT_MAX);
    IN_RANGE_EXT_ASSERT(float(dfloat(+FLT_MAX)) == +FLT_MAX);

    IN_RANGE_EXT_ASSERT(float(dfloat(0)) == 0.0f && !std::signbit(float(dfloat(0))));
    IN_RANGE_EXT_ASSERT(float(dfloat(-1)) == -1);
    IN_RANGE_EXT_ASSERT(float(dfloat(+1)) == +1);
    IN_RANGE_EXT_ASSERT(float(dfloat(-(fradix - 1))) == -(fradix - 1));
    IN_RANGE_EXT_ASSERT(float(dfloat(+(fradix - 1))) == +(fradix - 1));

    if (flimits::radix == 2 && flimits::digits == 24)
    {
        static_assert(int32_t(float(INT32_MIN)) == INT32_MIN);
        static_assert(int32_t(float(0x7fffff80)) == 0x7fffff80);
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(-flimits::quiet_NaN()));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(-flimits::infinity()));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(flimits::lowest()));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(std::nextafterf(float(INT32_MIN), -INFINITY)));
        IN_RANGE_EXT_ASSERT(in_range_ext::in_range<int32_t>(float(INT32_MIN)));
        IN_RANGE_EXT_ASSERT(in_range_ext::in_range<int32_t>(float(0x7fffff80)));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(std::nextafterf(float(0x7fffff80), +INFINITY)));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(flimits::max()));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(+flimits::infinity()));
        IN_RANGE_EXT_ASSERT(!in_range_ext::in_range<int32_t>(+flimits::quiet_NaN()));
    }
}
