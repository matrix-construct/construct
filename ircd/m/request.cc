// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

//
// request
//

ircd::m::request::request(const string_view &method,
                          const string_view &uri,
                          const mutable_buffer &body_buf,
                          const json::members &body)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	json::stringify(mutable_buffer{body_buf}, body)
}
{}

ircd::m::request::request(const string_view &method,
                          const string_view &uri)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	json::object{}
}
{}

ircd::m::request::request(const string_view &method,
                          const string_view &uri,
                          const json::object &content)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	content
}
{}

ircd::m::request::request(const string_view &origin,
                          const string_view &destination,
                          const string_view &method,
                          const string_view &uri,
                          const json::object &content)
{
	json::get<"origin"_>(*this) = origin;
	json::get<"destination"_>(*this) = destination;
	json::get<"method"_>(*this) = method;
	json::get<"uri"_>(*this) = uri;
	json::get<"content"_>(*this) = content;
}

ircd::string_view
ircd::m::request::operator()(const mutable_buffer &out,
                             const vector_view<const http::header> &addl_headers)
const
{
	const size_t addl_headers_size
	{
		std::min(addl_headers.size(), size_t(64UL))
	};

	size_t headers{1};
	http::header header[headers + addl_headers_size]
	{
		{ "User-Agent", info::user_agent }
	};

	for(size_t i(0); i < addl_headers_size; ++i)
		header[headers++] = addl_headers.at(i);

	thread_local char x_matrix[1_KiB];
	if(startswith(at<"uri"_>(*this), "/_matrix/federation"))
	{
		const auto &sk{self::secret_key};
		const auto &pkid{self::public_key_id};
		header[headers++] =
		{
			"Authorization", generate(x_matrix, sk, pkid)
		};
	}

	static const string_view content_type
	{
		"application/json; charset=utf-8"_sv
	};

	const auto content_length
	{
		string_view(json::get<"content"_>(*this)).size()
	};

	window_buffer sb{out};
	http::request
	{
		sb,
		at<"destination"_>(*this),
		at<"method"_>(*this),
		at<"uri"_>(*this),
		content_length,
		content_type,
		{ header, headers }
	};

	return sb.completed();
}

ircd::string_view
ircd::m::request::generate(const mutable_buffer &out,
                           const ed25519::sk &sk,
                           const string_view &pkid)
const
{
	const json::strung object
	{
		*this,
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(const_buffer{object})
	};

	const auto &origin
	{
		unquote(string_view{at<"origin"_>(*this)})
	};

	thread_local char sigb64[1_KiB];
	return fmt::sprintf
	{
		out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\"",
		origin,
		pkid,
		b64encode_unpadded(sigb64, sig)
	};
}

bool
ircd::m::request::verify(const string_view &key,
                         const string_view &sig)
const
{
	const ed25519::sig _sig
	{
		[&sig](auto &buf)
		{
			b64decode(buf, sig);
		}
	};

	const ed25519::pk pk
	{
		[this, &key](auto &buf)
		{
			const auto &origin
			{
				unquote(at<"origin"_>(*this))
			};

			m::keys::get(origin, key, [&buf]
			(const string_view &key)
			{
				b64decode(buf, unquote(key));
			});
		}
	};

	return verify(pk, _sig);
}

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig)
const
{
	const json::strung object
	{
		*this
	};

	return verify(pk, sig, object);
}

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig,
                         const json::object &object)
{
	return pk.verify(object, sig);
}

//
// x_matrix
//

ircd::m::request::x_matrix::x_matrix(const string_view &input)
{
	string_view tokens[3];
	if(ircd::tokens(split(input, ' ').second, ',', tokens) != 3)
		throw std::out_of_range{"The x_matrix header is malformed"};

	for(const auto &token : tokens)
	{
		const auto &kv{split(token, '=')};
		const auto &val{unquote(kv.second)};
		switch(hash(kv.first))
		{
		    case hash("origin"):  origin = val;  break;
		    case hash("key"):     key = val;     break;
		    case hash("sig"):     sig = val;     break;
		}
	}

	if(empty(origin))
		throw std::out_of_range{"The x_matrix header is missing 'origin='"};

	if(empty(key))
		throw std::out_of_range{"The x_matrix header is missing 'key='"};

	if(empty(sig))
		throw std::out_of_range{"The x_matrix header is missing 'sig='"};
}
