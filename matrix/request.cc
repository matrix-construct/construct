// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static unique_mutable_buffer request_content_buf;
}

decltype(ircd::m::request::headers_max)
ircd::m::request::headers_max
{
	32UL
};

decltype(ircd::m::request::content_max)
ircd::m::request::content_max
{
	{ "name",    "ircd.m.request.content.max" },
	{ "default",  long(4_MiB)                 },
};

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

	if(unlikely(origin && !rfc3986::valid_remote(std::nothrow, origin)))
		throw m::error
		{
			http::BAD_REQUEST, "M_REQUEST_INVALID_ORIGIN",
			"This origin string '%s' is not a valid remote.",
			origin
		};

	if(unlikely(destination && !rfc3986::valid_remote(std::nothrow, destination)))
		throw m::error
		{
			http::BAD_REQUEST, "M_REQUEST_INVALID_DESTINATION",
			"This destination string '%s' is not a valid remote.",
			destination
		};
}

ircd::string_view
ircd::m::request::operator()(const mutable_buffer &out,
                             const vector_view<const http::header> &addl_headers)
const
{
	thread_local http::header header[headers_max];
	thread_local char authorization[2_KiB];
	const ctx::critical_assertion ca;

	size_t headers{0};
	header[headers++] =
	{
		"User-Agent", info::user_agent
	};

	if(startswith(json::at<"uri"_>(*this), "/_matrix/federation"))
	{
		const json::string &origin
		{
			json::at<"origin"_>(*this)
		};

		const auto &my
		{
			m::my(origin)
		};

		const auto &secret_key
		{
			m::secret_key(my)
		};

		const auto &public_key_id
		{
			m::public_key_id(my)
		};

		header[headers++] =
		{
			"Authorization", generate(authorization, secret_key, public_key_id)
		};
	}

	assert(headers <= headers_max);
	assert(headers + addl_headers.size() <= headers_max);
	for(size_t i(0); i < addl_headers.size() && headers < headers_max; ++i)
		header[headers++] = addl_headers.at(i);

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
		json::at<"destination"_>(*this),
		json::at<"method"_>(*this),
		json::at<"uri"_>(*this),
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
	const ctx::critical_assertion ca;
	if(unlikely(buffer::size(request_content_buf) != size_t(content_max)))
		request_content_buf = unique_mutable_buffer
		{
			size_t(content_max), info::page_size
		};

	assert(!empty(request_content_buf));
	const mutable_buffer buf
	{
		request_content_buf
	};

	const auto serial_size
	{
		json::serialized(*this)
	};

	if(unlikely(serial_size > buffer::size(buf)))
		throw m::error
		{
			"M_REQUEST_TOO_LARGE", "This server generated a request of %zu bytes; limit is %zu",
			serial_size,
			buffer::size(buf)
		};

	const json::object object
	{
		stringify(mutable_buffer{buf}, *this)
	};

	const json::string &origin
	{
		json::at<"origin"_>(*this)
	};

	const json::string &destination
	{
		json::at<"destination"_>(*this)
	};

	const auto &secret_key
	{
		m::secret_key(my(origin))
	};

	const ed25519::sig sig
	{
		secret_key.sign(object)
	};

	assert(pkid);
	assert(origin);
	assert(destination);
	char sigb64[128];
	return fmt::sprintf
	{
		out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\",destination=\"%s\"",
		origin,
		pkid,
		b64::encode_unpadded(sigb64, sig),
		destination,
	};
}

bool
ircd::m::request::verify(const string_view &key,
                         const string_view &sig_)
const
{
	const ed25519::sig sig
	{
		[&sig_](auto&& buf)
		{
			b64::decode(buf, sig_);
		}
	};

	const json::string &origin
	{
		json::at<"origin"_>(*this)
	};

	const m::node::keys node_keys
	{
		origin
	};

	bool attempted{false}, verified{false};
	node_keys.get(key, [this, &attempted, &verified, &sig]
	(const ed25519::pk &pk)
	{
		attempted = true;
		verified = verify(pk, sig);
	});

	if(!verified && !attempted)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNVERIFIABLE_SIGNATURE",
			"The X-Matrix Authorization key '%s' for '%s' could not be obtained.",
			key,
			origin,
		};

	return verified;
}

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig)
const
{
	const ctx::critical_assertion ca;
	if(unlikely(buffer::size(request_content_buf) != size_t(content_max)))
		request_content_buf = unique_mutable_buffer
		{
			size_t(content_max), info::page_size
		};

	assert(!empty(request_content_buf));
	mutable_buffer buf
	{
		request_content_buf
	};

	// Matrix spec sez that an empty content object {} is excluded entirely
	// from the verification. Our JSON only excludes members if they evaluate
	// to undefined i.e json::object{}/string_view{} but not json::object{"{}"}
	// or even json::object{""}; rather than burdening the caller with ensuring
	// their assignment conforms perfectly, we ensure correctness manually.
	auto _this(*this);
	if(empty(json::get<"content"_>(*this)))
		json::get<"content"_>(_this) = json::object{};

	const size_t request_size
	{
		json::serialized(_this)
	};

	if(unlikely(request_size > buffer::size(buf)))
		throw m::error
		{
			http::PAYLOAD_TOO_LARGE, "M_REQUEST_TOO_LARGE",
			"The request size %zu bytes exceeds maximum of %zu bytes",
			request_size,
			buffer::size(buf)
		};

	const json::object object
	{
		json::stringify(buf, _this)
	};

	return verify(pk, sig, object);
}

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig,
                         const json::object &object)
{
	assert(object.has("destination"));
	assert(object.has("method"));
	assert(object.has("origin"));
	assert(object.has("uri"));

	return pk.verify(object, sig);
}

//
// x_matrix
//

ircd::m::request::x_matrix::x_matrix(const string_view &authorization)
:x_matrix
{
	split(authorization, ' ')
}
{
}

ircd::m::request::x_matrix::x_matrix(const pair<string_view> &authorization)
:x_matrix
{
	authorization.first,
	authorization.second
}
{
}

ircd::m::request::x_matrix::x_matrix(const string_view &type,
                                     const string_view &values)
{
	static const size_t tokens_max {8}, tokens_min {3};

	// Assumes caller determined this is X-Matrix lest they suffer exception.
	assert(iequals(type, "X-Matrix"_sv));

	string_view tokens[tokens_max];
	if(unlikely(ircd::tokens(values, ',', tokens) < tokens_min))
		throw std::out_of_range
		{
			"The x_matrix header is malformed."
		};

	for(const auto &token : tokens)
	{
		const auto &[key, val_]
		{
			split(token, '=')
		};

		const auto &val
		{
			unquote(val_)
		};

		switch(hash(key))
		{
			case hash("origin"):       this->origin = val;       break;
			case hash("key"):          this->key = val;          break;
			case hash("sig"):          this->sig = val;          break;
			case hash("destination"):  this->destination = val;  break;
		}
	}

	if(unlikely(empty(origin)))
		throw std::out_of_range
		{
			"The x_matrix header is missing 'origin='"
		};

	if(unlikely(empty(key)))
		throw std::out_of_range
		{
			"The x_matrix header is missing 'key='"
		};

	if(unlikely(empty(sig)))
		throw std::out_of_range
		{
			"The x_matrix header is missing 'sig='"
		};
}
