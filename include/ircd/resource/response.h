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
#define HAVE_IRCD_RESOURCE_RESPONSE_H

struct ircd::resource::response
{
	struct chunked;

	static const size_t HEAD_BUF_SZ;
	static conf::item<std::string> access_control_allow_origin;

	response(client &, const http::code &, const string_view &content_type, const size_t &content_length, const string_view &headers = {});
	response(client &, const string_view &str, const string_view &content_type, const http::code &, const vector_view<const http::header> &);
	response(client &, const string_view &str, const string_view &content_type, const http::code & = http::OK, const string_view &headers = {});
	response(client &, const json::object &str, const http::code & = http::OK);
	response(client &, const json::array &str, const http::code & = http::OK);
	response(client &, const json::members & = {}, const http::code & = http::OK);
	response(client &, const json::value &, const http::code & = http::OK);
	response(client &, const json::iov &, const http::code & = http::OK);
	response(client &, const http::code &, const json::members &);
	response(client &, const http::code &, const json::value &);
	response(client &, const http::code &, const json::iov &);
	response(client &, const http::code &);
	response() = default;
};

struct ircd::resource::response::chunked
:resource::response
{
	static conf::item<size_t> default_buffer_size;

	client *c {nullptr};
	unique_buffer<mutable_buffer> buf;

	size_t write(const const_buffer &chunk, const bool &ignore_empty = true);
	const_buffer flush(const const_buffer &);
	bool finish();

	std::function<const_buffer (const const_buffer &)> flusher();

	chunked(client &, const http::code &, const string_view &content_type, const string_view &headers = {}, const size_t &buffer_size = default_buffer_size);
	chunked(client &, const http::code &, const string_view &content_type, const vector_view<const http::header> &, const size_t &buffer_size = default_buffer_size);
	chunked(client &, const http::code &, const vector_view<const http::header> &, const size_t &buffer_size = default_buffer_size);
	chunked(client &, const http::code &, const size_t &buffer_size = default_buffer_size);
	chunked(const chunked &) = delete;
	chunked(chunked &&) = delete;
	chunked() = default;
	~chunked() noexcept;
};
