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
#define HAVE_IRCD_CLIENT_H

namespace ircd
{
	struct client;

	const ipport &remote(const client &);
	ipport local(const client &);

	string_view loghead(const mutable_buffer &buf, const client &);
	string_view loghead(const client &);
}

/// Remote party connecting to our daemon to make requests.
struct ircd::client
:std::enable_shared_from_this<client>
,ircd::instance_multimap<net::ipport, client, net::ipport::cmp_ip>
{
	struct init;
	struct conf;
	struct settings;
	struct request;

	static log::log log;
	static struct settings settings;
	static struct conf default_conf;
	static ctx::pool::opts pool_opts;
	static ctx::pool pool;
	static ctx::dock dock;
	static uint64_t ctr;              // monotonic

	struct conf *conf {&default_conf};
	unique_buffer<mutable_buffer> head_buffer;
	unique_buffer<mutable_buffer> content_buffer;
	std::shared_ptr<socket> sock;
	uint64_t id {++ctr};
	uint64_t ready_count {0};
	uint64_t request_count {0};
	ctx::ctx *reqctx {nullptr};
	ircd::timer timer;
	size_t head_length {0};
	size_t content_consumed {0};
	resource::request request;

	size_t write_all(const net::const_buffers &);
	size_t write_all(const const_buffer &);
	void close(const net::close_opts &, net::close_callback);
	void close(const net::dc, net::close_callback);
	ctx::future<void> close(const net::close_opts &);
	ctx::future<void> close(const net::dc);

  private:
	void discard_unconsumed(const http::request::head &);
	bool resource_request(const http::request::head &);
	bool handle_request(parse::capstan &pc);
	bool main();

	static char *read(client &, char *&start, char *const &stop); //TODO: XXX
	static parse::read_closure read_closure(client &); //TODO: XXX
	static void handle_requests(std::shared_ptr<client>);
	static void handle_ready(std::shared_ptr<client>, const error_code &ec);
	bool async();

  public:
	client(std::shared_ptr<socket>);
	client(client &&) = delete;
	client(const client &) = delete;
	client &operator=(client &&) = delete;
	client &operator=(const client &) = delete;
	~client() noexcept;

	static void create(net::acceptor &, const std::shared_ptr<socket> &);
	static size_t count(const net::ipport &remote); // cmp is by IP only, not port
	static void terminate_all();
	static void interrupt_all();
	static void close_all();
	static void wait_all();
	static void spawn();

	friend const ipport &remote(const client &);
	friend ipport local(const client &);
};

template<>
decltype(ircd::client::map)
ircd::instance_multimap<ircd::net::ipport, ircd::client, ircd::net::ipport::cmp_ip>::map;

/// Confs can be attached to individual clients to change their behavior
struct ircd::client::conf
{
	template<class T> using item = ircd::conf::item<T>;

	static item<seconds> async_timeout_default;
	static item<seconds> request_timeout_default;
	static item<size_t> header_max_size_default;

	/// Default time limit for how long a client connection can be in "async mode"
	/// (or idle mode) after which it is disconnected.
	seconds async_timeout {async_timeout_default};

	/// Time limit for how long a connected client can be sending its request.
	/// This is meaningful before the resource being sought is known (while
	/// receiving headers), after which its own specific timeout specified by
	/// its options takes over.
	seconds request_timeout {request_timeout_default};

	/// Number of bytes allocated to receive HTTP request headers for client.
	size_t header_max_size {header_max_size_default};
};

/// Settings apply to all clients and cannot be configured per-client
struct ircd::client::settings
{
	template<class T> using item = ircd::conf::item<T>;

	static item<size_t> stack_size;
	static item<size_t> pool_size;
	static item<size_t> pool_disp;
	static item<size_t> max_client;
	static item<size_t> max_client_per_peer;
};

struct [[gnu::visibility("hidden")]]
ircd::client::init
{
	init();
	~init() noexcept;
};

inline size_t
ircd::client::write_all(const const_buffer &buf)
{
	const const_buffer bufs[]
	{
		buf
	};

	return write_all(bufs);
}
