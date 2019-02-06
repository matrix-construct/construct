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
#define HAVE_IRCD_M_REQUEST_H

namespace ircd::m
{
	struct request;
}

/// Composes a matrix protocol request. This is a JSON tuple because the
/// protocol uses a JSON authorization object to create the X-Matrix
/// authorization header on federation requests.
///
struct ircd::m::request
:json::tuple
<
	json::property<name::content, json::object>,
	json::property<name::destination, string_view>,
	json::property<name::method, string_view>,
	json::property<name::origin, string_view>,
	json::property<name::uri, string_view>
>
{
	struct x_matrix;

	static const size_t headers_max;
	static conf::item<size_t> generate_content_max;
	static conf::item<size_t> verify_content_max;

	static bool verify(const ed25519::pk &, const ed25519::sig &, const json::object &);
	bool verify(const ed25519::pk &, const ed25519::sig &) const;
	bool verify(const string_view &key, const string_view &sig) const;
	string_view generate(const mutable_buffer &out, const ed25519::sk &, const string_view &pkid) const;

	string_view operator()(const mutable_buffer &out, const vector_view<const http::header> & = {}) const;

	request(const string_view &origin,
	        const string_view &destination,
	        const string_view &method,
	        const string_view &uri,
	        const json::object &content);

	request(const string_view &method,
	        const string_view &uri,
	        const json::object &content);

	request(const string_view &method,
	        const string_view &uri);

	// stringifies content members into buf
	request(const string_view &method,
	        const string_view &uri,
	        const mutable_buffer &body_buf,
	        const json::members &body);

	using super_type::tuple;
	request() = default;
};

struct ircd::m::request::x_matrix
{
	string_view origin;
	string_view key;
	string_view sig;

	x_matrix(const string_view &);
	x_matrix() = default;
};
