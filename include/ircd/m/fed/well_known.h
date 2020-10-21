// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_WELL_KNOWN_H

/// well-known for server name resolution.
///
/// This is a future-based interface. It performs local caching in the !dns
/// room as well as conducting network requests. The cache is queried on the
/// callers ircd::ctx and valid results cheaply return an already-satisfied
/// future. In the case of expired or missing results, a request structure is
/// allocated and managed internally and an unsatisfied future is returned;
/// an internal worker will resolve the promise asynchronously.
///
namespace ircd::m::fed::well_known
{
	struct request;
	struct opts;

	// Primary query interface
	ctx::future<string_view>
	get(const mutable_buffer &out, const string_view &name, const opts &);

	extern conf::item<seconds> cache_max;
	extern conf::item<seconds> cache_error;
	extern conf::item<seconds> cache_default;
}

/// Options used for resolving well-known.
struct ircd::m::fed::well_known::opts
{
	/// Whether to check the cache before making any request.
	bool cache_check {true};

	/// Allow expired cache results to be returned without making any refresh.
	bool expired {false};

	/// Allow a query to be made to a remote.
	bool request {true};

	/// Whether to cache the result of any request.
	bool cache_result {true};
};

/// Internal request structure; do not instantiate or manage manually. The
/// request::list allows traversal of all requests and observation of their
/// state.
struct ircd::m::fed::well_known::request
:instance_list<request>
{
	static const string_view path, type;
	static const server::request::opts sopts;
	static conf::item<size_t> redirects_max;
	static conf::item<seconds> timeout;
	static ctx::mutex mutex;
	static uint64_t id_ctr;

	mutable_buffer out;        // only safe if promise valid
	string_view target;
	well_known::opts opts;
	uint64_t id {++id_ctr};
	system_point expires;
	ctx::promise<string_view> promise;
	unique_mutable_buffer carry;
	rfc3986::uri uri;
	server::request req;
	http::code code {0};
	http::response::head head;
	string_view location;
	size_t redirects {0};
	json::object response;
	json::string m_server;
	char tgtbuf[2][rfc3986::REMOTE_BUFSIZE];
	char buf[15_KiB];
};
