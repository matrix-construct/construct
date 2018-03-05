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

decltype(ircd::resource::resources)
ircd::resource::resources
{};

ircd::resource &
ircd::resource::find(string_view path)
{
	path = rstrip(path, '/');
	auto it(resources.lower_bound(path));
	if(it == end(resources)) try
	{
		--it;
		if(it == begin(resources) || !startswith(path, rstrip(it->first, '/')))
			return *resources.at("/");
	}
	catch(const std::out_of_range &e)
	{
		throw http::error
		{
			http::code::NOT_FOUND
		};
	}

	// Exact file or directory match
	if(path == rstrip(it->first, '/'))
		return *it->second;

	// Directories handle all paths under them.
	if(!startswith(path, rstrip(it->first, '/')))
	{
		// Walk the iterator back to find if there is a directory prefixing this path.
		if(it == begin(resources))
			throw http::error
			{
				http::code::NOT_FOUND
			};

		--it;
		if(!startswith(path, rstrip(it->first, '/')))
			throw http::error
			{
				http::code::NOT_FOUND
			};
	}

	// Check if the resource is a directory; if not, it can only
	// handle exact path matches.
	if(~it->second->flags & it->second->DIRECTORY && path != rstrip(it->first, '/'))
		throw http::error
		{
			http::code::NOT_FOUND
		};

	return *it->second;
}

//
// resource
//

ircd::resource::resource(const string_view &path)
:resource
{
	path, opts{}
}
{
}

ircd::resource::resource(const string_view &path,
                         const opts &opts)
:path{path}
,description{opts.description}
,flags{opts.flags}
,resources_it{[this, &path]
{
	const auto iit
	{
		resources.emplace(this->path, this)
	};

	if(!iit.second)
		throw error
		{
			"resource \"%s\" already registered", path
		};

	return unique_const_iterator<decltype(resources)>
	{
		resources, iit.first
	};
}()}
{
	log::debug
	{
		"Registered resource \"%s\"", path.empty()? string_view{"/"} : path
	};
}

ircd::resource::~resource()
noexcept
{
	log::debug
	{
		"Unregistered resource \"%s\"", path.empty()? string_view{"/"} : path
	};
}

namespace ircd
{
	static void verify_origin(client &client, resource::method &method, resource::request &request);
	static void authenticate(client &client, resource::method &method, resource::request &request);
}

void
ircd::authenticate(client &client,
                   resource::method &method,
                   resource::request &request)
try
{
	request.access_token =
	{
		request.query["access_token"]
	};

	if(empty(request.access_token))
	{
		const auto authorization
		{
			split(request.head.authorization, ' ')
		};

		if(iequals(authorization.first, "bearer"_sv))
			request.access_token = authorization.second;
	}

	const bool result
	{
		request.access_token &&
		m::user::tokens.get(std::nothrow, "ircd.access_token"_sv, request.access_token, [&request]
		(const m::event &event)
		{
			// The user sent this access token to the tokens room
			request.user_id = m::user::id{at<"sender"_>(event)};
		})
	};

	if(!result)
		throw m::error
		{
			// When credentials are required but missing or invalid, the HTTP call will return with
			// a status of 401 and the error code, M_MISSING_TOKEN or M_UNKNOWN_TOKEN respectively.
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN", "Credentials for this method are required but invalid."
		};
}
catch(const std::out_of_range &e)
{
	throw m::error
	{
		// When credentials are required but missing or invalid, the HTTP call will return with
		// a status of 401 and the error code, M_MISSING_TOKEN or M_UNKNOWN_TOKEN respectively.
		http::UNAUTHORIZED, "M_MISSING_TOKEN", "Credentials for this method are required but missing."
	};
}

void
ircd::verify_origin(client &client,
                    resource::method &method,
                    resource::request &request)
try
{
	const m::request::x_matrix x_matrix
	{
		request.head.authorization
	};

	const m::request object
	{
		x_matrix.origin, my_host(), method.name, request.head.uri, request.content
	};

	const auto verified
	{
		object.verify(x_matrix.key, x_matrix.sig)
	};

	if(!verified)
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"The X-Matrix Authorization is invalid."
		};
}
catch(const m::error &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		"X-Matrix Authorization from %s: %s",
		string(remote(client)),
		e.what()
	};

	throw m::error
	{
		http::UNAUTHORIZED, "M_UNKNOWN_ERROR",
		"An error has prevented authorization: %s",
		e.what()
	};
}

void
ircd::resource::operator()(client &client,
                           const http::request::head &head,
                           const string_view &content_partial)
{
	// Find the method or METHOD_NOT_ALLOWED
	auto &method
	{
		operator[](head.method)
	};

	// Bail out if the method limited the amount of content and it was exceeded.
	if(head.content_length > method.opts.payload_max)
		throw http::error
		{
			http::PAYLOAD_TOO_LARGE
		};

	// Content that hasn't yet arrived is remaining
	const size_t content_remain
	{
		head.content_length - client.content_consumed
	};

	// View of the content that will be passed to the resource handler. Starts
	// with the content received so far which is actually in the head's buffer.
	// One of three things can happen now:
	//
	// - There is no more content so we pass this as-is right to the resource.
	// - There is more content, so we allocate a content buffer, copy what we
	// have to it, read the rest off the socket, and then reassign this view.
	// - There is more content, but the resource wants to read it off the
	// socket on its own terms, so we pass this as-is.
	string_view content
	{
		content_partial
	};

	if(content_remain && ~method.opts.flags & method.CONTENT_DISCRETION)
	{
		// Copy any partial content to the final contiguous allocated buffer;
		client.content_buffer = unique_buffer<mutable_buffer>{head.content_length};
		memcpy(data(client.content_buffer), data(content_partial), size(content_partial));

		// Setup a window inside the buffer for the remaining socket read.
		const mutable_buffer content_remain_buffer
		{
			data(client.content_buffer) + size(content_partial), content_remain
		};

		// Read the remaining content off the socket.
		client.content_consumed += read_all(*client.sock, content_remain_buffer);
		assert(client.content_consumed == head.content_length);
		content = string_view
		{
			data(client.content_buffer), head.content_length
		};
	}

	client.request = resource::request
	{
		head, content
	};

	const auto pathparm
	{
		lstrip(head.path, this->path)
	};

	client.request.parv =
	{
		client.request.param, tokens(pathparm, '/', client.request.param)
	};

	if(method.opts.flags & method.REQUIRES_AUTH)
		authenticate(client, method, client.request);

	if(method.opts.flags & method.VERIFY_ORIGIN)
		verify_origin(client, method, client.request);

	handle_request(client, method, client.request);
}

void
ircd::resource::handle_request(client &client,
                               method &method,
                               resource::request &request)
try
{
	const auto response
	{
		method(client, request)
	};
}
catch(const json::not_found &e)
{
	throw m::error
	{
		http::BAD_REQUEST, "M_BAD_JSON", "Required JSON field: %s", e.what()
	};
}
catch(const json::print_error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Generator Protection: %s", e.what()
	};
}
catch(const json::error &e)
{
	throw m::error
	{
		http::BAD_REQUEST, "M_NOT_JSON", "%s", e.what()
	};
}
catch(const std::out_of_range &e)
{
	throw m::error
	{
		http::NOT_FOUND, "M_NOT_FOUND", "%s", e.what()
	};
}

ircd::resource::method &
ircd::resource::operator[](const string_view &name)
try
{
	return *methods.at(name);
}
catch(const std::out_of_range &e)
{
	size_t len(0);
	char buf[128]; buf[0] = '\0';
	auto it(begin(methods));
	if(it != end(methods))
	{
		len = strlcat(buf, it->first);
		for(++it; it != end(methods); ++it)
		{
			len = strlcat(buf, " ");
			len = strlcat(buf, it->first);
		}
	}

	throw http::error
	{
		http::METHOD_NOT_ALLOWED, {},
		{
			{ "Allow", string_view{buf, len} }
		}
	};
}

ircd::resource::method::method(struct resource &resource,
                               const string_view &name,
                               const handler &handler)
:method
{
	resource, name, handler, {}
}
{
}

ircd::resource::method::method(struct resource &resource,
                               const string_view &name,
                               const handler &handler,
                               const struct opts &opts)
:name{name}
,resource{&resource}
,function{handler}
,opts{opts}
,methods_it{[this, &name]
{
	const auto iit
	{
		this->resource->methods.emplace(this->name, this)
	};

	if(!iit.second)
		throw error
		{
			"resource \"%s\" already registered", name
		};

	return unique_const_iterator<decltype(resource::methods)>
	{
		this->resource->methods,
		iit.first
	};
}()}
{
}

ircd::resource::method::~method()
noexcept
{
}

ircd::resource::response
ircd::resource::method::operator()(client &client,
                                   request &request)
try
{
	return function(client, request);
}
catch(const std::bad_function_call &e)
{
	throw http::error
	{
		http::SERVICE_UNAVAILABLE
	};
}

ircd::resource::response::response(client &client,
                                   const http::code &code)
:response{client, json::object{"{}"}, code}
{
}

ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const json::iov &members)
:response{client, members, code}
{
}

ircd::resource::response::response(client &client,
                                   const json::members &members,
                                   const http::code &code)
:response{client, code, members}
{
}

ircd::resource::response::response(client &client,
                                   const json::value &value,
                                   const http::code &code)
:response{client, code, value}
{
}

ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const json::value &value)
try
{
	const auto size
	{
		serialized(value)
	};

	const unique_buffer<mutable_buffer> buffer
	{
		size
	};

	switch(type(value))
	{
		case json::ARRAY:
		{
			response(client, json::array{stringify(mutable_buffer{buffer}, value)}, code);
			return;
		}

		case json::OBJECT:
		{
			response(client, json::object{stringify(mutable_buffer{buffer}, value)}, code);
			return;
		}

		default: throw m::error
		{
			http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Cannot send json::%s as response content", type(value)
		};
	}
}
catch(const json::error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Generator Protection: %s", e.what()
	};
}

ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const json::members &members)
try
{
	const auto size
	{
		serialized(members)
	};

	const unique_buffer<mutable_buffer> buffer
	{
		size
	};

	const json::object object
	{
		stringify(mutable_buffer{buffer}, members)
	};

	response(client, object, code);
}
catch(const json::error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Generator Protection: %s", e.what()
	};
}

ircd::resource::response::response(client &client,
                                   const json::iov &members,
                                   const http::code &code)
try
{
	const auto size
	{
		serialized(members)
	};

	const unique_buffer<mutable_buffer> buffer
	{
		size
	};

	const json::object object
	{
		stringify(mutable_buffer{buffer}, members)
	};

	response(client, object, code);
}
catch(const json::error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Generator Protection: %s", e.what()
	};
}

ircd::resource::response::response(client &client,
                                   const json::object &object,
                                   const http::code &code)
{
	static const string_view content_type
	{
		"application/json; charset=utf-8"
	};

	assert(json::valid(object, std::nothrow));
	response(client, object, content_type, code);
}

ircd::resource::response::response(client &client,
                                   const json::array &array,
                                   const http::code &code)
{
	static const string_view content_type
	{
		"application/json; charset=utf-8"
	};

	assert(json::valid(array, std::nothrow));
	response(client, array, content_type, code);
}

ircd::resource::response::response(client &client,
                                   const string_view &content,
                                   const string_view &content_type,
                                   const http::code &code,
                                   const vector_view<const http::header> &headers)
{
	// contents of this buffer get copied again when further passed to
	// response{}; we can get this off the stack if that remains true.
	thread_local char buffer[4_KiB];
	window_buffer sb{buffer};
	{
		const critical_assertion ca;
		http::write(sb, headers);
	}

	new (this) response
	{
		client, content, content_type, code, string_view{sb.completed()}
	};
}

ircd::resource::response::response(client &client,
                                   const string_view &content,
                                   const string_view &content_type,
                                   const http::code &code,
                                   const string_view &headers)
{
	const auto request_time
	{
		client.timer.at<microseconds>().count()
	};

	const fmt::bsprintf<64> rtime
	{
		"%zd$us", request_time
	};

	const string_view cache_control
	{
		(code >= 200 && code < 300) ||
		(code >= 403 && code < 405) ||
		(code >= 300 && code < 400)? "no-cache":
		""
	};

	// This buffer will be passed to the socket and sent out;
	// cannot be static/tls.
	char head_buf[4_KiB];
	window_buffer head{head_buf};
	http::response
	{
		head,
		code,
		content.size(),
		content_type,
		headers,
		{
			{ "Access-Control-Allow-Origin",   "*"                  }, //TODO: XXX
			{ "Cache-Control",                 cache_control        },
			{ "X-IRCd-Request-Timer",          rtime,               },
		},
	};

	// Maximum size is 2_KiB which is realistically ok but ideally a small
	// maximum; this exception should hit the developer in testing.
	if(unlikely(!head.remaining()))
		throw assertive
		{
			"HTTP headers too large for buffer of %zu", sizeof(head_buf)
		};

	const ilist<const const_buffer> vector
	{
		head.completed(),
		content
	};

	write_closure(client)(vector);

	log::debug
	{
		"socket(%p) local[%s] remote[%s] HTTP %d %s in %ld$us; response in %ld$us (%s) content-length:%zu",
		client.sock.get(),
		string(local(client)),
		string(remote(client)),
		int(code),
		http::status(code),
		request_time,
		(client.timer.at<microseconds>().count() - request_time),
		content_type,
		content.size()
	};
}
