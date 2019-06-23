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
#define HAVE_IRCD_UTIL_SYSCALL_H

namespace ircd {
inline namespace util
{
	template<class function, class... args> long syscall(function&& f, args&&... a);
	template<long number, class... args> long syscall(args&&... a);

	template<class function, class... args> long syscall_nointr(function&& f, args&&... a);
	template<long number, class... args> long syscall_nointr(args&&... a);
}}

/// Posix system call template to check for returned error value and throw the
/// approps errno in the proper std::system_error exception. Note the usage
/// here, the libc wrapper function is the first argument i.e:
/// syscall(::foo, bar, baz) not syscall(::foo(bar, baz));
///
template<class function,
         class... args>
long
ircd::util::syscall(function&& f,
                    args&&... a)
{
	const auto ret
	{
		f(std::forward<args>(a)...)
	};

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

/// Posix system call template to check for returned error value and throw the
/// approps errno in the proper std::system_error exception. This template
/// requires a system call number in the parameters. The arguments are only
/// the actual arguments passed to the syscall because the number is given
/// in the template.
///
template<long number,
         class... args>
long
ircd::util::syscall(args&&... a)
{
	const auto ret
	{
		::syscall(number, std::forward<args>(a)...)
	};

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

/// Uninterruptible posix system call template to check for returned error
/// value and throw the approps errno in the proper std::system_error
/// exception. Note the usage here, the libc wrapper function is the first
/// argument i.e: syscall(::foo, bar, baz) not syscall(::foo(bar, baz));
///
/// The syscall is restarted until it no longer returns with EINTR.
///
template<class function,
         class... args>
long
ircd::util::syscall_nointr(function&& f,
                           args&&... a)
{
	long ret; do
	{
		ret = f(std::forward<args>(a)...);
	}
	while(unlikely(ret == -1 && errno == EINTR));

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

/// Uninterruptible posix system call template to check for returned error
/// value and throw the approps errno in the proper std::system_error
/// exception. This template requires a system call number in the parameters.
/// The arguments are only the actual arguments passed to the syscall because
/// the number is given in the template.
///
/// The syscall is restarted until it no longer returns with EINTR.
///
template<long number,
         class... args>
long
ircd::util::syscall_nointr(args&&... a)
{
	long ret; do
	{
		ret = ::syscall(number, std::forward<args>(a)...);
	}
	while(unlikely(ret == -1 && errno == EINTR));

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}
