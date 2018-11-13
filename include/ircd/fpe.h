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
#define HAVE_IRCD_FPE_H

namespace ircd::fpe
{
	struct errors_handle;
	struct errors_throw;

	string_view reflect_sicode(const int &);
	string_view reflect(const ushort &flag);
	string_view reflect(const mutable_buffer &, const ushort &flags);
	void throw_errors(const ushort &flags);
}

/// Perform a single floating point operation at a time within the scope
/// of fpe::errors_handle. After each operation check the floating point
/// unit for an error status flag and throw a C++ exception.
struct ircd::fpe::errors_handle
{
	fexcept_t theirs;

  public:
	ushort pending() const;
	void throw_pending() const;
	void clear_pending();

	errors_handle();
	~errors_handle() noexcept(false);
};

// We don't include <signal.h> in IRCd's standard include list. Instead that
// is conditionally included in the appropriate .cc file. All we require here
// is a forward declaration.
extern "C"
{
	struct sigaction;
};

/// Experimental floating-point error handling strategy which throws
/// a C++ exception directly from the instruction which faulted the FPU.
/// The stack will unwind directly from that point in the execution and
/// control will be transferred to the appropriate catch block.
struct ircd::fpe::errors_throw
{
	custom_ptr<struct sigaction> their_sa;
	long their_fenabled;
	fexcept_t their_fe;

  public:
	errors_throw();
	~errors_throw() noexcept;
};
