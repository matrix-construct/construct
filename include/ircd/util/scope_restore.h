// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_SCOPE_RESTORE_H

namespace ircd {
inline namespace util
{
	template<class T> struct scope_restore;
}}

template<class T>
struct ircd::util::scope_restore
{
	T *restore {nullptr};
	T theirs;

	scope_restore(T &restore);
	scope_restore(T &restore, T&& ours);
	template<class... args> scope_restore(T &restore, args&&... ours);
	scope_restore(const scope_restore &) = delete;
	scope_restore(scope_restore &&) noexcept = delete;
	~scope_restore() noexcept;
};

template<class T>
ircd::util::scope_restore<T>::scope_restore(T &restore)
:restore{&restore}
,theirs{std::move(restore)}
{
}

template<class T>
ircd::util::scope_restore<T>::scope_restore(T &restore,
                                            T&& ours)
:restore{&restore}
,theirs{std::move(restore)}
{
	restore = std::forward<T>(ours);
}

template<class T>
template<class... args>
ircd::util::scope_restore<T>::scope_restore(T &restore,
                                            args&&... ours)
:restore{&restore}
,theirs{std::move(restore)}
{
	new (&restore) T(std::forward<args>(ours)...);
}

template<class T>
ircd::util::scope_restore<T>::~scope_restore()
noexcept
{
	assert(restore);
	*restore = std::move(theirs);
}
