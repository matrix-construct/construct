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

	const char *write(client &, const char *&start, const char *const &stop);
	char *read(client &, char *&start, char *const &stop);

	http::response::write_closure write_closure(client &);
	parse::read_closure read_closure(client &);

	void close_all_clients();

	std::shared_ptr<client> add_client(std::shared_ptr<socket>);  // Creates a client.
}

struct ircd::client
:std::enable_shared_from_this<client>
,ircd::instance_list<client>
{
	struct init;
	struct conf;
	struct settings;
	struct request;

	static constexpr const size_t &HEAD_MAX
	{
		4_KiB
	};

	static struct settings settings;
	static struct conf default_conf;
	static ctx::pool context;

	struct conf *conf {&default_conf};
	unique_buffer<mutable_buffer> head_buffer;
	std::shared_ptr<socket> sock;
	ircd::timer timer;
	http::request::head head;
	size_t head_length {0};
	unique_buffer<mutable_buffer> content_buffer;
	size_t content_consumed {0};
	bool longpoll {false};

	void close(const net::close_opts &, net::close_callback);
	ctx::future<void> close(const net::close_opts & = {});

	void discard_unconsumed();
	bool resource_request();
	bool handle_request(parse::capstan &pc);
	bool main() noexcept;
	void async();

  public:
	client(std::shared_ptr<socket>);
	client();
	client(client &&) = delete;
	client(const client &) = delete;
	client &operator=(client &&) = delete;
	client &operator=(const client &) = delete;
	~client() noexcept;

	friend ipport remote(const client &);
	friend ipport local(const client &);
};

/// Confs can be attached to individual clients to change their behavior
struct ircd::client::conf
{
	/// Default time limit for how long a client connection can be in "async mode"
	/// (or idle mode) after which it is disconnected.
	seconds async_timeout {35s};

	/// Time limit for how long a connected client can be in "request mode." This
	/// should never be hit unless there's an error in the handling code.
	seconds request_timeout {15s};
};

/// Settings apply to all clients and cannot be configured per-client
struct ircd::client::settings
{
	/// TODO
	size_t stack_size
	{
		1_MiB
	};

	/// TODO
	size_t pool_size
	{
		128
	};
};

struct ircd::client::init
{
	void interrupt();

	init();
	~init() noexcept;
};
