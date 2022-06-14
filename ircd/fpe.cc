// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifndef __GNUC__
#pragma STDC FENV_ACCESS on
#endif

[[gnu::visibility("protected")]]
void
ircd::fpe::debug_info()
{
	log::logf
	{
		log::star, log::DEBUG,
		"FLT RAD=%d DIG=%-2d MANT=%-3d EPS=%-7lf MIN=%-8lf MAX=%-7lf"
		" EXPMIN=%-7d EXPMAX=%-6d EXP10MIN=%-6d EXP10MAX=%-5d",
		FLT_RADIX,
		FLT_DIG,
		FLT_MANT_DIG,
		FLT_EPSILON,
		FLT_MIN,
		FLT_MAX,
		FLT_MIN_EXP,
		FLT_MAX_EXP,
		FLT_MIN_10_EXP,
		FLT_MAX_10_EXP,
	};

	log::logf
	{
		log::star, log::DEBUG,
		"DBL RAD=%d DIG=%-2d MANT=%-3d EPS=%-7lf MIN=%-8lf MAX=%-7lf"
		" EXPMIN=%-7d EXPMAX=%-6d EXP10MIN=%-6d EXP10MAX=%-5d",
		FLT_RADIX,
		DBL_DIG,
		DBL_MANT_DIG,
		DBL_EPSILON,
		DBL_MIN,
		DBL_MAX,
		DBL_MIN_EXP,
		DBL_MAX_EXP,
		DBL_MIN_10_EXP,
		DBL_MAX_10_EXP,
	};

	log::logf
	{
		log::star, log::DEBUG,
		"LDB RAD=%d DIG=%-2d MANT=%-3d EPS=%-7lf MIN=%-8lf MAX=%-7lf"
		" EXPMIN=%-7d EXPMAX=%-6d EXP10MIN=%-6d EXP10MAX=%-5d",
		FLT_RADIX,
		LDBL_DIG,
		LDBL_MANT_DIG,
		LDBL_EPSILON,
		LDBL_MIN,
		LDBL_MAX,
		LDBL_MIN_EXP,
		LDBL_MAX_EXP,
		LDBL_MIN_10_EXP,
		LDBL_MAX_10_EXP,
	};
}

[[gnu::cold]]
void
ircd::fpe::_throw_errors(const ushort &flags)
{
	assert(flags);

	char buf[128];
	throw std::domain_error
	{
		reflect(buf, flags)
	};
}

ircd::string_view
ircd::fpe::reflect(const mutable_buffer &buf,
                   const ushort &flags)
{
	window_buffer window{buf};
	const auto append{[&window](const auto &flag)
	{
		window([&flag](const mutable_buffer &buf)
		{
			return strlcpy(buf, reflect(flag));
		});
	}};

	for(size_t i(0); i < sizeof(flags) * 8; ++i)
		if(flags & (1 << i))
			append(1 << i);

	return window.completed();
}

ircd::string_view
ircd::fpe::reflect(const ushort &flag)
noexcept
{
	switch(flag)
	{
		case 0:             return "";
		case FE_INVALID:    return "INVALID";
		case FE_DIVBYZERO:  return "DIVBYZERO";
		case FE_UNDERFLOW:  return "UNDERFLOW";
		case FE_OVERFLOW:   return "OVERFLOW";
		case FE_INEXACT:    return "INEXACT";
	}

	return "?????";
}

ircd::string_view
ircd::fpe::reflect_sicode(const int &code)
noexcept
{
	switch(code)
	{
		#if defined(HAVE_SIGNAL_H) && defined(FPE_INTDIV)
		case FPE_INTDIV:  return "INTDIV";
		case FPE_INTOVF:  return "INTOVF";
		case FPE_FLTDIV:  return "FLTDIV";
		case FPE_FLTOVF:  return "FLTOVF";
		case FPE_FLTUND:  return "FLTUND";
		case FPE_FLTRES:  return "FLTRES";
		case FPE_FLTINV:  return "FLTINV";
		case FPE_FLTSUB:  return "FLTSUB";
		#endif // HAVE_SIGNAL_H
	}

	return "?????";
}

template<class T>
ircd::string_view
ircd::fpe::classify(const T &a)
noexcept
{
	switch(std::fpclassify(a))
	{
		case FP_NAN:          return "NAN";
		case FP_ZERO:         return "ZERO";
		case FP_NORMAL:       return "NORMAL";
		case FP_SUBNORMAL:    return "SUBNORMAL";
		case FP_INFINITE:     return "INFINITE";
	}

	return "????";
}
template ircd::string_view ircd::fpe::classify(const long double &a) noexcept;
template ircd::string_view ircd::fpe::classify(const double &a) noexcept;
template ircd::string_view ircd::fpe::classify(const float &a) noexcept;
