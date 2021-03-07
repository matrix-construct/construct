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
#define HAVE_IRCD_FPE_H

namespace ircd::fpe
{
	struct errors_handle;
	struct scope_round;

	template<class T>
	string_view classify(const T &) noexcept;

	string_view reflect_sicode(const int &) noexcept;
	string_view reflect(const ushort &flag) noexcept;
	string_view reflect(const mutable_buffer &, const ushort &flags);

	[[noreturn]] void _throw_errors(const ushort &flags);
	void throw_errors(const ushort &flags);

	void set_round(const std::float_round_style &style);
	std::fexcept_t set_excepts(const ushort &flag);
}

/// scope_round
///
struct ircd::fpe::scope_round
{
	int theirs
	{
		std::fegetround()
	};

	scope_round(const std::float_round_style &ours) noexcept
	{
		std::fesetround(ours);
	}

	scope_round(const scope_round &) = delete;
	scope_round &operator=(const scope_round &) = delete;
	~scope_round() noexcept
	{
		std::fesetround(theirs);
	}
};

/// errors_handle
///
/// Perform a single floating point operation at a time within the scope
/// of fpe::errors_handle. After each operation check the floating point
/// unit for an error status flag and throw a C++ exception.
struct ircd::fpe::errors_handle
{
	std::fexcept_t theirs;

  public:
	ushort pending() const;
	void throw_pending() const;
	void clear_pending();

	errors_handle();
	~errors_handle() noexcept(false);
};

[[gnu::always_inline]] inline
ircd::fpe::errors_handle::errors_handle()
{
	syscall(std::fegetexceptflag, &theirs, FE_ALL_EXCEPT);
	clear_pending();
}

[[gnu::always_inline]] inline
ircd::fpe::errors_handle::~errors_handle()
noexcept(false)
{
	const unwind reset{[this]
	{
		syscall(std::fesetexceptflag, &theirs, FE_ALL_EXCEPT);
	}};

	throw_pending();
}

inline void
ircd::fpe::errors_handle::clear_pending()
{
	syscall(std::feclearexcept, FE_ALL_EXCEPT);
}

inline void
ircd::fpe::errors_handle::throw_pending()
const
{
	const auto pending
	{
		this->pending()
	};

	if(unlikely(pending))
		_throw_errors(pending);
}

inline ushort
ircd::fpe::errors_handle::pending()
const
{
	return std::fetestexcept(FE_ALL_EXCEPT);
}

//
// ircd::fpe
//

inline std::fexcept_t
ircd::fpe::set_excepts(const ushort &flags)
{
	std::fexcept_t theirs;
	syscall(std::fesetexceptflag, &theirs, flags);
	return theirs;
}

inline void
ircd::fpe::set_round(const std::float_round_style &style)
{
	syscall(std::fesetround, style);
}
