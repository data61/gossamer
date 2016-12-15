// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef NUMERIC_CONVERSION_UTIL_HH
#define NUMERIC_CONVERSION_UTIL_HH

#ifndef STD_LIMITS
#include <limits>
#define STD_LIMITS
#endif // STD_LIMITS

template<typename T, typename Wrapped>
struct WrappedNumericLimits
{
    typedef typename std::numeric_limits<Wrapped> wrapped_limits;

    static const bool is_specialized = true;

    static T min() throw() { return T(wrapped_limits::min()); }
    static T max() throw() { return T(wrapped_limits::max()); }

    static const int digits = wrapped_limits::digits;
    static const int digits10 = wrapped_limits::digits10;
    static const int max_digits10 = wrapped_limits::max_digits10;

    static const bool is_signed = wrapped_limits::is_signed;
    static const bool is_integer = wrapped_limits::is_integer;
    static const bool is_exact = wrapped_limits::is_exact;
    static const int radix = wrapped_limits::radix;
    static T epsilon() throw() { return T(wrapped_limits::epsilon()); }
    static T round_error() throw() { return T(wrapped_limits::round_error()); }

    static const int min_exponent = wrapped_limits::min_exponent;
    static const int min_exponent10 = wrapped_limits::min_exponent10;
    static const int max_exponent = wrapped_limits::max_exponent;
    static const int max_exponent10 = wrapped_limits::max_exponent10;

    static const bool has_infinity = wrapped_limits::has_infinity;
    static const bool has_quiet_NaN = wrapped_limits::has_quiet_NaN;
    static const bool has_signaling_NaN = wrapped_limits::has_signaling_NaN;
    static const std::float_denorm_style has_denorm = wrapped_limits::has_denorm;
    static const bool has_denorm_loss = wrapped_limits::has_denorm_loss;

    static T infinity() throw() { return T(wrapped_limits::infinity()); }
    static T quiet_NaN() throw() { return T(wrapped_limits::quiet_NaN()); }
    static T signaling_NaN() throw() { return T(wrapped_limits::signaling_NaN()); }
    static T denorm_min() throw() { return T(wrapped_limits::denorm_min()); }

    static const bool is_iec559 = wrapped_limits::is_iec559;
    static const bool is_bounded = wrapped_limits::is_bounded;
    static const bool is_modulo = wrapped_limits::is_modulo;

    static const bool traps = wrapped_limits::traps;
    static const bool tinyness_before = wrapped_limits::tinyness_before;
    static const std::float_round_style round_style = wrapped_limits::round_style;
};


struct UnsignedIntegralNumericLimitsBase
{
    static const bool is_specialized = true;

    static const bool is_signed = false;
    static const bool is_integer = true;
    static const bool is_exact = true;
    static const int radix = 2;

    static const int min_exponent = 0;
    static const int min_exponent10 = 0;
    static const int max_exponent = 0;
    static const int max_exponent10 = 0;

    static const bool has_infinity = false;
    static const bool has_quiet_NaN = false;
    static const bool has_signaling_NaN = false;
    static const std::float_denorm_style has_denorm = std::denorm_absent;
    static const bool has_denorm_loss = false;

    static const bool is_iec559 = false;
    static const bool is_bounded = true;
    static const bool is_modulo = true;

    static const bool traps = false;
    static const bool tinyness_before = false;
    static const std::float_round_style round_style = std::round_toward_zero;
};

template<typename T, int Bits>
struct UnsignedIntegralNumericLimits
    : public UnsignedIntegralNumericLimitsBase
{
    static T min() throw() { return T(0); }
    static T max() throw() { return ~T(0); }

    static const int digits = Bits;
    static const int digits10 = (int)(Bits * 0.30102999566398114);
    static const int max_digits10 = digits10 + 1;

    static T epsilon() throw() { return T(0); }
    static T round_error() throw() { return T(0); }

    static T infinity() throw() { return T(0); }
    static T quiet_NaN() throw() { return T(0); }
    static T signaling_NaN() throw() { return T(0); }
    static T denorm_min() throw() { return T(0); }
};

template<class Traits>
struct WrapConverterPolicy
{
    typedef typename Traits::result_type result_type;
    typedef typename Traits::argument_type argument_type;

    static result_type
    low_level_convert(argument_type pArg)
    {
        return result_type(pArg);
    }
};

template<class Traits>
struct UnwrapConverterPolicy
{
    typedef typename Traits::result_type result_type;
    typedef typename Traits::argument_type argument_type;

    static result_type
    low_level_convert(argument_type pArg)
    {
        return pArg.value();
    }
};


#endif // NUMERIC_CONVERSION_UTIL_HH
