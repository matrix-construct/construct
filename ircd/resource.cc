/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
		throw http::error(http::code::NOT_FOUND);
	}

	// Exact file or directory match
	if(path == rstrip(it->first, '/'))
		return *it->second;

	// Directories handle all paths under them.
	if(!startswith(path, rstrip(it->first, '/')))
	{
		// Walk the iterator back to find if there is a directory prefixing this path.
		if(it == begin(resources))
			throw http::error(http::code::NOT_FOUND);

		--it;
		if(!startswith(path, rstrip(it->first, '/')))
			throw http::error(http::code::NOT_FOUND);
	}

	// Check if the resource is a directory; if not, it can only
	// handle exact path matches.
	if(~it->second->flags & it->second->DIRECTORY && path != rstrip(it->first, '/'))
		throw http::error(http::code::NOT_FOUND);

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
	const auto iit(resources.emplace(this->path, this));
	if(!iit.second)
		throw error("resource \"%s\" already registered", path);

	return unique_const_iterator<decltype(resources)>
	{
		resources, iit.first
	};
}()}
{
	log::info("Registered resource \"%s\"", path.empty()? string_view{"/"} : path);
}

ircd::resource::~resource()
noexcept
{
	log::info("Unregistered resource \"%s\"", path.empty()? string_view{"/"} : path);
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
	const string_view &access_token
	{
		request.query.at("access_token")
	};

	// Sets up the query to find the access_token in the sessions room
	const m::vm::query<m::vm::where::equal> query
	{
		{ "type",        "ircd.access_token"         },
		{ "state_key",   access_token                },
		{ "room_id",     m::user::sessions.room_id   },
	};

	const bool result
	{
		m::vm::test(query, [&request, &access_token](const m::event &event)
		{
			// Checks if the access token has expired. Tokens are expired when
			// an m.room.redaction event is issued for the ircd.access_token
			// event. Instead of making another query here for the redaction
			// we expect the original event to be updated with the following
			// key which must be part of the redaction process.
			const json::object &unsigned_
			{
				json::get<"unsigned"_>(event)
			};

			if(unsigned_.has("redacted_because"))
				return false;

			assert(at<"state_key"_>(event) == access_token);
			request.user_id = m::user::id{at<"sender"_>(event)};
			return true;
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
	const fmt::bsprintf<1024> uri
	{
		"%s%s%s",
		request.head.path,
		request.query? "?" : "",
		request.query
	};

	const m::request::x_matrix x_matrix
	{
		request.head.authorization
	};

	const m::request object
	{
		x_matrix.origin, my_host(), method.name, uri, request.content
	};

	const auto verified
	{
		object.verify(x_matrix.key, x_matrix.sig)
	};

	if(verified)
		return;

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
	log::error("X-Matrix Authorization from %s: %s",
	           string(remote(client)),
	           e.what());

	throw m::error
	{
		http::UNAUTHORIZED, "M_UNKNOWN_ERROR",
		"An error has prevented authorization: %s",
		e.what()
	};
}

void
ircd::resource::operator()(client &client,
                           struct client::request &request,
                           const http::request::head &head)
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

	const size_t content_remain
	{
		head.content_length - request.content_consumed
	};

	unique_buffer<mutable_buffer> content_buffer;
	string_view content{request.content_partial};
	if(content_remain)
	{
		// Copy any partial content to the final contiguous allocated buffer;
		content_buffer = unique_buffer<mutable_buffer>{head.content_length};
		memcpy(data(content_buffer), data(request.content_partial), size(request.content_partial));

		// Setup a window inside the buffer for the remaining socket read.
		const mutable_buffer content_remain_buffer
		{
			data(content_buffer) + size(request.content_partial), content_remain
		};

		//TODO: more discretion from the method.
		// Read the remaining content off the socket.
		request.content_consumed += read_all(*client.sock, content_remain_buffer);
		assert(request.content_consumed == head.content_length);
		content = string_view
		{
			data(content_buffer), head.content_length
		};
	}

	const auto pathparm
	{
		lstrip(head.path, this->path)
	};

	string_view param[8];
	const vector_view<string_view> parv
	{
		param, tokens(pathparm, '/', param)
	};

	resource::request resource_request
	{
		head, content, head.query, parv
	};

	if(method.opts.flags & method.REQUIRES_AUTH)
		authenticate(client, method, resource_request);

	if(method.opts.flags & method.VERIFY_ORIGIN)
		verify_origin(client, method, resource_request);

	handle_request(client, method, resource_request);
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
	const auto iit(this->resource->methods.emplace(this->name, this));
	if(!iit.second)
		throw error("resource \"%s\" already registered", name);

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

ircd::resource::request::request(const http::request::head &head,
                                 const string_view &content,
                                 http::query::string query,
                                 const vector_view<string_view> &parv)
:json::object{content}
,head{head}
,content{content}
,query{std::move(query)}
,parv{parv}
{
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
	static const auto content_type
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
	static const auto content_type
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
	thread_local char buffer[2_KiB];
	stream_buffer sb{buffer};
	{
		const critical_assertion ca;
		http::write(sb, headers);
	}

	response
	{
		client, content, content_type, code, sb.completed()
	};
}

ircd::resource::response::response(client &client,
                                   const string_view &content,
                                   const string_view &content_type,
                                   const http::code &code,
                                   const string_view &headers)
{
	assert(client.request);
	const auto request_time
	{
		client.request->timer.at<microseconds>().count()
	};

	const fmt::bsprintf<64> rtime
	{
		"%zd$us", request_time
	};

	const string_view cache_control
	{
		(code >= 200 && code < 300) ||
		(code >= 403 && code <= 405) ||
		(code >= 300 && code < 400)? "no-cache":
		""
	};

	// This buffer will be passed to the socket and sent out;
	// cannot be static/tls.
	char head_buf[2_KiB];
	stream_buffer head{head_buf};
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

	log::debug("socket(%p) local[%s] remote[%s] HTTP %d %s in %ld$us; response in %ld$us (%s) content-length:%zu",
	           client.sock.get(),
	           string(local(client)),
	           string(remote(client)),
	           int(code),
	           http::status(code),
	           request_time,
	           (client.request->timer.at<microseconds>().count() - request_time),
	           content_type,
	           content.size());
}
