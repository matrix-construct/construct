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

/// Construction of a resource::response transmits result data to the client.
///
/// A resource::response is required for every request, which is why the
/// return value of every resource method handler is a resource::response type.
/// This return value requirement has no other significance, and the response
/// object has no useful semantics.
///
/// The construction of a response object will send the response head and
/// content to the client. The call will probably yield the ircd::ctx. When the
/// construction is complete the response has been sent to the client (or copied
/// entirely to the kernel).
///
/// The lowest level ctors without a content argument allow for sending just the
/// response HTTP head to the client. The developer has the option to manually
/// write the content to the client's socket following the transmission of the
/// head. It is still advised for semantic reasons that the resource::response
/// object which transmitted the head still be returned from the handler.
///
/// Note that handlers can always throw an exception, and the resource
/// framework will facilitate the response there.
///
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

/// This device streams a chunked encoded response to a request. This is
/// preferred rather than conducting chunked encoding manually with the above
/// resource::response (that's what this is for).
///
/// Basic usage of this device involves construction of a named instance,
/// upon which headers are immediately sent to the client opening the chunked
/// encoding session. First know that if a handler throws an exception
/// during a chunked encoding session, the client connection is immediately
/// terminated as hard as possible (disrupting any pipelining, etc).
///
/// Once the instance is constructed the developer calls write() to write a
/// chunk to the socket. Each call to write() directly sends a chunk and
/// yields the ctx until it is transmitted.
///
/// The direct use of this object is rare, instead it is generally paired with
/// something like json::stack, which streams chunks of JSON. To facilitate
/// this type of pairing and real world use, instances of this object contain
/// a simple buffered flush-callback system.
//
/// By default this object allocates a buffer to facilitate the chunked
/// response and to satisfy the majority pattern of allocating this same
/// buffer immediately preceding construction. A function pointer can also
/// be passed on construction to act as a "flusher." These features are
/// best suited for use by json::stack. A developer wishing to conduct chunked
/// encoding with some other content has the option of setting a zero buffer
/// size on construction.
///
struct ircd::resource::response::chunked
:resource::response
{
	struct json;

	static conf::item<size_t> default_buffer_size;

	client *c {nullptr};
	unique_mutable_buffer _buf;
	mutable_buffer buf;
	size_t flushed {0};
	size_t wrote {0};
	uint count {0};
	bool finished {false};

	size_t write(const const_buffer &chunk, const bool &ignore_empty = true);
	const_buffer flush(const const_buffer &);
	bool finish(const bool psh = false);

	std::function<const_buffer (const const_buffer &)> flusher();

	chunked(client &, const http::code &, const string_view &content_type, const string_view &headers = {}, const size_t &buffer_size = default_buffer_size, const mutable_buffer & = {});
	chunked(client &, const http::code &, const string_view &content_type, const vector_view<const http::header> &, const size_t &buffer_size = default_buffer_size, const mutable_buffer & = {});
	chunked(client &, const http::code &, const vector_view<const http::header> &, const size_t &buffer_size = default_buffer_size, const mutable_buffer & = {});
	chunked(client &, const http::code &, const size_t &buffer_size = default_buffer_size, const mutable_buffer & = {});
	chunked() = default;
	chunked(chunked &&) = delete;
	chunked(const chunked &) = delete;
	chunked &operator=(chunked &&) = delete;
	chunked &operator=(const chunked &&) = delete;
	~chunked() noexcept;
};

/// Convenience amalgam. This class reduces a common pattern of objects
/// constructed in a response handler using chunked encoding to stream
/// json::object content.
///
struct ircd::resource::response::chunked::json
:resource::response::chunked
{
	ircd::json::stack out;
	ircd::json::stack::object top;

	operator ircd::json::stack &()
	{
		return out;
	}

	template<class... args>
	json(args&&... a);
};

template<class... args>
inline
ircd::resource::response::chunked::json::json(args&&... a)
:resource::response::chunked
{
	std::forward<args>(a)...
}
,out
{
	this->buf, this->flusher()
}
,top
{
	this->out
}
{}
