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
#define HAVE_IRCD_CTX_SCOPE_NOTIFY_H

namespace ircd::ctx
{
	struct scope_notify;
}

struct ircd::ctx::scope_notify
{
	IRCD_OVERLOAD(all)
	IRCD_OVERLOAD(one)

	dock *d {nullptr};
	bool _all {false};
	bool _one {false};

	scope_notify(dock &d);
	scope_notify(dock &d, one_t);
	scope_notify(dock &d, all_t);
	scope_notify(scope_notify &&) noexcept;
	scope_notify(const scope_notify &) = delete;
	scope_notify &operator=(scope_notify &&) noexcept;
	scope_notify &operator=(const scope_notify &) = delete;
	~scope_notify() noexcept;
};

namespace ircd
{
	using ctx::scope_notify;
}

/// Notify the dock at destruction time with dock::notify() (reminder, that
/// is a single notify with fairness).
inline
ircd::ctx::scope_notify::scope_notify(dock &d)
:d{&d}
{}

/// Notify the dock at destruction time with a notify_one() (reminder, that
/// is a single notify of the first ctx waiting in the dock only).
inline
ircd::ctx::scope_notify::scope_notify(dock &d,
                                      one_t)
:d{&d}
,_one{true}
{}

/// Notify the dock at destruction time with a notify_all().
inline
ircd::ctx::scope_notify::scope_notify(dock &d,
                                      all_t)
:d{&d}
,_all{true}
{}

inline
ircd::ctx::scope_notify::scope_notify(scope_notify &&other)
noexcept
:d{std::move(other.d)}
,_all{std::move(other._all)}
,_one{std::move(other._one)}
{
	other.d = nullptr;
}

inline
ircd::ctx::scope_notify &
ircd::ctx::scope_notify::operator=(scope_notify &&other)
noexcept
{
	this->~scope_notify();
	d = std::move(other.d);
	_all = std::move(other._all);
	_one = std::move(other._one);
	other.d = nullptr;
	return *this;
}

inline
ircd::ctx::scope_notify::~scope_notify()
noexcept
{
	if(!d)
		return;
	else if(_all)
		d->notify_all();
	else if(_one)
		d->notify_one();
	else
		d->notify();
}
