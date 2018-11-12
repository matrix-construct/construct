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

extern "C"
{
	struct sigaction;
};

namespace ircd::fpe
{
	struct errors_handle;
	struct errors_throw;

	string_view reflect_sicode(const int &);
	string_view reflect(const ushort &flag);
	string_view reflect(const mutable_buffer &, const ushort &flags);
	void throw_errors(const ushort &flags);
}

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

struct ircd::fpe::errors_throw
{
	custom_ptr<struct sigaction> their_sa;
	long their_fenabled;
	fexcept_t their_fe;

  public:
	errors_throw();
	~errors_throw() noexcept;
};
