// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>

namespace ircd::net::dns::cache
{
	struct waiter;

	bool operator==(const waiter &, const waiter &);
	bool operator!=(const waiter &, const waiter &);

	bool call_waiter(const string_view &, const string_view &, const json::array &, waiter &);
	size_t call_waiters(const string_view &, const string_view &, const json::array &);
	void handle(const m::event &, m::vm::eval &);

	bool put(const string_view &type, const string_view &state_key, const records &rrs);
	bool put(const string_view &type, const string_view &state_key, const uint &code, const string_view &msg);

	void fini();
	void init();

	extern const m::room::id::buf room_id;
	extern m::hookfn<m::vm::eval &> hook;
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
