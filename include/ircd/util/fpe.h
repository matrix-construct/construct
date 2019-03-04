// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_FPE_H

namespace ircd::util::fpe
{
	struct errors_handle;

	string_view reflect_sicode(const int &);
	string_view reflect(const ushort &flag);
	string_view reflect(const mutable_buffer &, const ushort &flags);

	void throw_errors(const ushort &flags);
	std::fexcept_t set(const ushort &flag);
}

/// Perform a single floating point operation at a time within the scope
/// of fpe::errors_handle. After each operation check the floating point
/// unit for an error status flag and throw a C++ exception.
struct ircd::util::fpe::errors_handle
{
	std::fexcept_t theirs;

  public:
	ushort pending() const;
	void throw_pending() const;
	void clear_pending();

	errors_handle();
	~errors_handle() noexcept(false);
};
