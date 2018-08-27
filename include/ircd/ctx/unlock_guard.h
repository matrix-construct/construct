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
#define HAVE_IRCD_CTX_UNLOCK_GUARD_H

namespace ircd::ctx
{
	template<class lockable> struct unlock_guard;
}

/// Inverse of std::lock_guard<>
template<class lockable>
struct ircd::ctx::unlock_guard
{
	lockable &l;

	unlock_guard(lockable &l);
	~unlock_guard() noexcept;
};

namespace ircd
{
	using ctx::unlock_guard;
}

template<class lockable>
ircd::ctx::unlock_guard<lockable>::unlock_guard(lockable &l)
:l{l}
{
	l.unlock();
}

template<class lockable>
ircd::ctx::unlock_guard<lockable>::~unlock_guard()
noexcept
{
	l.lock();
}
