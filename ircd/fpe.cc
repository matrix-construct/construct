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
	window_buffer wb{buf};
	const auto append{[&wb](const auto &flag)
	{
		wb([&flag](const mutable_buffer &buf)
		{
			return strlcpy(buf, reflect(flag));
		});
	}};

	for(size_t i(0); i < sizeof(flags) * 8; ++i)
		if(flags & (1 << i))
			append(1 << i);

	return wb.completed();
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
