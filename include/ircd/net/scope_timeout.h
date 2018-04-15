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
#define HAVE_IRCD_NET_SCOPE_TIMEOUT_H

namespace ircd::net
{
	struct scope_timeout;
}

class ircd::net::scope_timeout
{
	socket *s {nullptr};

  public:
	using handler = std::function<void (const bool &)>; // true = TO; false = canceled

	bool cancel() noexcept;   // invoke timer.cancel() before dtor
	bool release();           // cancels the cancel;

	scope_timeout(socket &, const milliseconds &timeout, handler callback);
	scope_timeout(socket &, const milliseconds &timeout);
	scope_timeout() = default;
	scope_timeout(scope_timeout &&) noexcept;
	scope_timeout(const scope_timeout &) = delete;
	scope_timeout &operator=(scope_timeout &&) noexcept;
	scope_timeout &operator=(const scope_timeout &) = delete;
	~scope_timeout() noexcept;
};
