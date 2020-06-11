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
#define HAVE_IRCD_SYS_H

namespace ircd::sys
{
	enum call :uint;

	template<enum call = (enum call)0,
	         class function,
	         class... args>
	long call(function&&, args&&...);

	template<long number,
	         enum call = (enum call)0,
	         class... args>
	long call(args&&... a);

	// sysfs read to buffer
	string_view get(const mutable_buffer &out, const string_view &path);

	// sysfs read cast to type or defaults
	template<class R = size_t,
	         size_t bufmax = 32>
	R get(const string_view &path, const R &def = 0);

	extern log::log log;
}

/// System call template options; changes behaviors.
enum ircd::sys::call
:uint
{
	NOTHROW            = 0x01,
	UNINTERRUPTIBLE    = 0x02,
};

/// Return a lex_cast'able (an integer) from a sysfs target.
template<class T,
         size_t bufmax>
inline T
ircd::sys::get(const string_view &path,
               const T &def)
{
	char buf[bufmax];
	const string_view val
	{
		get(buf, path)
	};

	return lex_castable<T>(val)?
		lex_cast<T>(val):
		def;
}

/// This template requires a system call number in the parameters. The
/// arguments are only the actual arguments passed to the syscall because
/// the number is given in the template.
///
template<long number,
         enum ircd::sys::call opts,
         class... args>
inline long
ircd::sys::call(args&&... a)
{
	return call<opts>(::syscall, number, std::forward<args>(a)...);
}

/// Posix system call template to check for returned error value and throw the
/// approps errno in the proper std::system_error exception. Note the usage
/// here, the libc wrapper function is the first argument i.e:
/// syscall(::foo, bar, baz) not syscall(::foo(bar, baz));
///
/// when uninterruptible posix system call template; the syscall is
/// restarted until it no longer returns with EINTR.
///
template<enum ircd::sys::call opts,
         class function,
         class... args>
inline long
ircd::sys::call(function&& f,
                args&&... a)
{
	constexpr auto uninterruptible
	{
		opts & call::UNINTERRUPTIBLE
	};

	long ret; do
	{
		ret = f(std::forward<args>(a)...);
	}
	while(unlikely(uninterruptible && ret == -1 && errno == EINTR));

	constexpr auto nothrow
	{
		opts & call::NOTHROW
	};

	if(unlikely(!nothrow && ret == -1L))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// Legacy convenience forwarder interface
//
namespace ircd {

template<class function,
         class... args>
inline auto
syscall(function&& f,
        args&&... a)
{
	return sys::call(std::forward<function>(f), std::forward<args>(a)...);
}

template<long number,
         class... args>
inline auto
syscall(args&&... a)
{
	return sys::call<number>(std::forward<args>(a)...);
}

template<class function,
         class... args>
inline auto
syscall_nointr(function&& f,
               args&&... a)
{
	constexpr enum sys::call opts
	{
		sys::call::UNINTERRUPTIBLE
	};

	return sys::call<opts>(std::forward<function>(f), std::forward<args>(a)...);
}

template<long number,
         class... args>
inline auto
syscall_nointr(args&&... a)
{
	constexpr enum sys::call opts
	{
		sys::call::UNINTERRUPTIBLE
	};

	return sys::call<number, opts>(std::forward<args>(a)...);
}

} // namespace ircd
