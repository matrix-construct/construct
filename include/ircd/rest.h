// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_REST_H

/// Simple highlevel interface for web/http requests.
///
/// Prior to this it was too difficult to orchestrate all the objects and
/// buffers and low-level non-ergonomic procedures split between ircd::http
/// and ircd::server. This should instead have some familiarity to the
/// browser-js environment which developers can easily commit to their memory.
namespace ircd::rest
{
	struct opts;
	struct request;

	struct get;
	struct put;
	struct post;
}

struct ircd::rest::request
:returns<string_view>
{
	unique_const_buffer out;

	request(const mutable_buffer &out, const rfc3986::uri &, opts);
	request(const rfc3986::uri &, opts);
};

struct ircd::rest::get
:request
{
	get(const mutable_buffer &out, const rfc3986::uri &, opts);
	get(const rfc3986::uri &, opts);
};

struct ircd::rest::put
:request
{
	put(const mutable_buffer &out, const rfc3986::uri &, const string_view &content, opts);
	put(const rfc3986::uri &, const string_view &content, opts);
};

struct ircd::rest::post
:request
{
	post(const mutable_buffer &out, const rfc3986::uri &, const string_view &content, opts);
	post(const mutable_buffer &out, const rfc3986::uri &, opts);
	post(const rfc3986::uri &, const string_view &content, opts);
	post(const rfc3986::uri &, opts);
};

struct ircd::rest::opts
{
	/// The HTTP method to use. This is overridden and should not be set unless
	/// using the generic rest::request() call where it must be set.
	string_view method;

	/// The HTTP request body. This is overridden and should not be set unless
	/// using the generic rest::request() call where it's set as needed.
	string_view content;

	/// The HTTP request body content-type. It is a good idea to set this when
	/// there is request body content.
	string_view content_type;

	/// Additional request headers to send. These are pairs of string_views.
	vector_view<const http::header> headers;

	/// This is set automatically from the URI argument's domain and scheme
	/// (service) by default. Setting it here will override.
	net::hostport remote;

	/// Managed internally by default and passed to server::request. Setting
	/// things here will override.
	server::out sout;

	/// Managed internally by default and passed to server::request. Setting
	/// things here will override.
	server::in sin;

	/// Passed to server::request. The http_exceptions option is useful here
	/// to prevent this suite from throwing on non-2xx codes.
	server::request::opts sopts;

	/// Allows the HTTP response code to be returned to the caller. This may
	/// not be initialized if the call throws any error first.
	http::code *code {nullptr};

	/// Allows the user to override the request::out with their own for
	/// receiving dynamic content. Supply an empty unique_buffer instance.
	unique_const_buffer *out {nullptr};

	/// Optionally supply the temporary buffer for headers in/out in lieu of
	/// any internally allocated.
	mutable_buffer buf;

	/// Timeout for the yielding/synchronous calls of this interface.
	seconds timeout {20s};

	/// Internal use
	opts &&set(const string_view &method, const string_view &content = {});
};

inline
ircd::rest::post::post(const rfc3986::uri &uri,
                       opts opts)
:request
{
	uri, opts.set("POST")
}
{}

inline
ircd::rest::post::post(const rfc3986::uri &uri,
                       const string_view &content,
                       opts opts)
:request
{
	uri, opts.set("POST", content)
}
{}

inline
ircd::rest::post::post(const mutable_buffer &out,
                       const rfc3986::uri &uri,
                       opts opts)
:request
{
	out, uri, opts.set("POST")
}
{}

inline
ircd::rest::post::post(const mutable_buffer &out,
                       const rfc3986::uri &uri,
                       const string_view &content,
                       opts opts)
:request
{
	out, uri, opts.set("POST", content)
}
{}

inline
ircd::rest::put::put(const rfc3986::uri &uri,
                     const string_view &content,
                     opts opts)
:request
{
	uri, opts.set("PUT", content)
}
{}

inline
ircd::rest::put::put(const mutable_buffer &out,
                     const rfc3986::uri &uri,
                     const string_view &content,
                     opts opts)
:request
{
	out, uri, opts.set("PUT", content)
}
{}

inline
ircd::rest::get::get(const rfc3986::uri &uri,
                     opts opts)
:request
{
	uri, opts.set("GET")
}
{}

inline
ircd::rest::get::get(const mutable_buffer &out,
                     const rfc3986::uri &uri,
                     opts opts)
:request
{
	out, uri, opts.set("GET")
}
{}

inline ircd::rest::opts &&
ircd::rest::opts::set(const string_view &method,
                      const string_view &content)
{
	this->method = method;
	this->content = content?: this->content;
	return std::move(*this);
}
