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

	static conf::item<seconds> default_timeout;
	static conf::item<size_t> default_payload_max;
	static ctx::dock idle_dock;

	struct resource *resource;
	string_view name;
	handler function;
	std::unique_ptr<const struct opts> opts;
	std::unique_ptr<struct stats> stats;
	unique_const_iterator<decltype(resource::methods)> methods_it;

	bool content_length_acceptable(const http::request::head &) const;
	bool mime_type_acceptable(const http::request::head &) const;

	void handle_timeout(client &) const;
	response call_handler(client &, request &);

  public:
	response operator()(client &, const http::request::head &, const string_view &content_partial);

	method(struct resource &, const string_view &name, handler, struct opts);
	method(struct resource &, const string_view &name, handler);
	~method() noexcept;
};

enum ircd::resource::method::flag
:uint
{
	REQUIRES_AUTH         = 0x01,   //TODO: matrix abstraction bleed.
	RATE_LIMITED          = 0x02,
	VERIFY_ORIGIN         = 0x04,   //TODO: matrix abstraction bleed.
	CONTENT_DISCRETION    = 0x08,
	DELAYED_ACK           = 0x10,
	DELAYED_RESPONSE      = 0x20,
};

struct ircd::resource::method::opts
{
	flag flags {(flag)0};

	/// Timeout specific to this resource; 0 is automatic
	seconds timeout {0};

	/// The maximum size of the Content-Length for this method. Anything
	/// larger will be summarily rejected with a 413. -1 is automatic.
	size_t payload_max {-1UL};

	/// MIME type; first part is the Registry (i.e application) and second
	/// part is the format (i.e json). Empty value means nothing rejected.
	std::pair<string_view, string_view> mime;
};

struct ircd::resource::method::stats
{
	using item = ircd::stats::item<uint64_t>;

	item pending;             ///< Clients currently inside the method
	item requests;            ///< The method was found and called.
	item timeouts;            ///< The method's timeout was exceeded.
	item completions;         ///< The handler returned without throwing.
	item internal_errors;     ///< The handler threw a very bad exception.

	stats(method &);
};
