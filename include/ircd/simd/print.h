// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_PRINT_H

namespace ircd::simd
{
	struct print;

	/// Print the contents of the vector in "register layout" which are
	/// little-endian hex integer preceded by '0x' with each lane being
	/// space-separated. The fmt argument is reserved to offer some additional
	/// variations on the output format.
	template<class T>
	string_view
	print_reg(const mutable_buffer &buf,
	          const T,
	          const uint &fmt = 0)
	noexcept;

	/// Print the contents of the vector in "memory layout" which are indice-
	/// ordered hex values for each byte space-separated by lane. The fmt
	/// argument is reserved to offer some additional variations on the output
	/// format.
	template<class T>
	string_view
	print_mem(const mutable_buffer &buf,
	          const T,
	          const uint &fmt = 0)
	noexcept;

	/// Print the contents of the vector as characters for each byte
	/// space-separated by lane. The fmt argument is reserved to offer some
	/// additional variations on the output format.
	template<class T>
	string_view
	print_chr(const mutable_buffer &buf,
	          const T,
	          const uint &fmt = 0)
	noexcept;
}

/// Print the contents of the vector to stdout; developer convenience.
/// One of the other functions in this suite must be selected by this
/// template to generate the desired string.
class ircd::simd::print
{
	bool output(const mutable_buffer &, const string_view &, const bool lf);

  public:
	template<class T,
	         class F = decltype(print_mem<T>)>
	print(const T,
	      F&& printer      = print_mem<T>,
	      const uint &fmt  = 0,
	      const bool &lf   = true);
};

/// Developer convenience. Reference another print_*() in the template. All
/// arguments are passed thru.
template<class T,
         class F>
inline
ircd::simd::print::print(const T vec,
                         F&& printer,
                         const uint &fmt,
                         const bool &lf)
{
	thread_local char buf[1024];
	const string_view str
	{
		printer(buf, vec, fmt)
	};

	const bool done
	{
		output(buf, str, lf)
	};
}
