// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// This file is not included in any include group. It is used when
// implementing the dns::cache by modules and extensions.
//

namespace ircd::net::dns::cache
{
	struct waiter;

	bool operator==(const waiter &, const waiter &) noexcept;
	bool operator!=(const waiter &, const waiter &) noexcept;

	extern std::list<waiter> waiting;
	extern ctx::mutex mutex;
	extern ctx::dock dock;
}

struct ircd::net::dns::cache::waiter
{
	dns::callback callback;
	dns::opts opts;
	uint16_t port;
	string_view key;
	char keybuf[rfc1035::NAME_BUFSIZE*2];

	waiter(const hostport &hp, const dns::opts &opts, dns::callback &&callback);
	waiter(waiter &&) = delete;
	waiter(const waiter &) = delete;
	waiter &operator=(waiter &&) = delete;
	waiter &operator=(const waiter &) = delete;
};
