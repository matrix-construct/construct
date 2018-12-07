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
#define HAVE_IRCD_RFC3986_H

/// Universal Resource Indicator (URI) grammars & tools
namespace ircd::rfc3986
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, coding_error)
	IRCD_EXCEPTION(coding_error, encoding_error)
	IRCD_EXCEPTION(coding_error, decoding_error)

	struct parser extern const parser;
	struct encoder extern const encoder;
	struct decoder extern const decoder;

	void valid_hostname(const string_view &); // name part
	bool valid_hostname(std::nothrow_t, const string_view &);
	void valid_domain(const string_view &); // dot delimited hostnames
	bool valid_domain(std::nothrow_t, const string_view &);
	void valid_host(const string_view &); // domain | ip4 | ip6
	bool valid_host(std::nothrow_t, const string_view &);
	void valid_remote(const string_view &); // host + optional :port
	bool valid_remote(std::nothrow_t, const string_view &);

	string_view encode(const mutable_buffer &, const string_view &url);
	string_view encode(const mutable_buffer &, const json::members &);

	string_view decode(const mutable_buffer &, const string_view &url);
}

namespace ircd
{
	namespace url = rfc3986;
}
