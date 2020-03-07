// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// (internal) DNS cache
namespace ircd::net::dns::cache
{
	using closure = std::function<bool (const string_view &, const json::object &)>;

	extern conf::item<seconds> min_ttl;
	extern conf::item<seconds> error_ttl;
	extern conf::item<seconds> nxdomain_ttl;

	string_view make_type(const mutable_buffer &out, const string_view &);
	string_view make_type(const mutable_buffer &out, const uint16_t &);

	bool for_each(const string_view &type, const closure &); // do not make_type() here
	bool for_each(const hostport &, const opts &, const closure &);
	bool get(const hostport &, const opts &, const callback &);
	bool put(const hostport &, const opts &, const records &);
	bool put(const hostport &, const opts &, const uint &code, const string_view &msg = {});
}

/// (internal) DNS cache
namespace ircd::net::dns::cache
{
	struct waiter;

	bool operator==(const waiter &, const waiter &) noexcept;
	bool operator!=(const waiter &, const waiter &) noexcept;

	extern std::list<waiter> waiting;
	extern ctx::mutex mutex;
	extern ctx::dock dock;
}

/// DNS cache result waiter
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
