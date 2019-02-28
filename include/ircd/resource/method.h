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
#define HAVE_IRCD_RESOURCE_METHOD_H

struct ircd::resource::method
{
	enum flag :uint;
	struct opts;
	struct stats;
	using handler = std::function<response (client &, request &)>;

	static conf::item<bool> x_matrix_verify_origin;
	static conf::item<bool> x_matrix_verify_destination;
	static ctx::dock idle_dock;

	struct resource *resource;
	string_view name;
	handler function;
	std::unique_ptr<const struct opts> opts;
	std::unique_ptr<struct stats> stats;
	unique_const_iterator<decltype(resource::methods)> methods_it;

	string_view verify_origin(client &, request &) const;
	string_view authenticate(client &, request &) const;
	void handle_timeout(client &) const;
	void call_handler(client &, request &);

  public:
	void operator()(client &, const http::request::head &, const string_view &content_partial);

	method(struct resource &, const string_view &name, handler, struct opts);
	method(struct resource &, const string_view &name, handler);
	~method() noexcept;
};

enum ircd::resource::method::flag
:uint
{
	REQUIRES_AUTH         = 0x01,
	RATE_LIMITED          = 0x02,
	VERIFY_ORIGIN         = 0x04,
	CONTENT_DISCRETION    = 0x08,
};

struct ircd::resource::method::opts
{
	flag flags {(flag)0};

	/// Timeout specific to this resource.
	seconds timeout {30s};

	/// The maximum size of the Content-Length for this method. Anything
	/// larger will be summarily rejected with a 413.
	size_t payload_max {128_KiB};

	/// MIME type; first part is the Registry (i.e application) and second
	/// part is the format (i.e json). Empty value means nothing rejected.
	std::pair<string_view, string_view> mime;
};

struct ircd::resource::method::stats
{
	uint64_t pending {0};             // Clients currently inside the method
	uint64_t requests {0};            // The method was found and called.
	uint64_t timeouts {0};            // The method's timeout was exceeded.
	uint64_t completions {0};         // The handler returned without throwing.
	uint64_t internal_errors {0};     // The handler threw a very bad exception.
};
