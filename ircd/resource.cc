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

namespace ircd
{
	void handle_request(client &client, parse::capstan &pc, const http::request::head &head);
}

IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(ircd::resource::resources)
ircd::resource::resources
{};

/// Handle a single request within the client main() loop.
///
/// This function returns false if the main() loop should exit
/// and thus disconnect the client. It should return true in most
/// cases even for lightly erroneous requests that won't affect
/// the next requests on the tape.
///
/// This function is timed. The timeout will prevent a client from
/// sending a partial request and leave us waiting for the rest.
/// As of right now this timeout extends to our handling of the
/// request too.
bool
ircd::handle_request(client &client,
                     parse::capstan &pc)
try
{
	bool ret{true};
	http::request
	{
		pc, nullptr, [&client, &pc, &ret]
		(const auto &head)
		{
			handle_request(client, pc, head);
			ret = !iequals(head.connection, "close"s);
		}
	};

	return ret;
}
catch(const http::error &e)
{
	resource::response
	{
		client, e.content, "text/html; charset=utf8", e.code, e.headers
	};

	switch(e.code)
	{
		case http::BAD_REQUEST:
		case http::REQUEST_TIMEOUT:
			return false;

		case http::INTERNAL_SERVER_ERROR:
			throw;

		default:
			return true;
	}
}
catch(const std::exception &e)
{
	log::error("client[%s]: in %ld$us: %s",
	           string(remote(client)),
	           client.request_timer.at<microseconds>().count(),
	           e.what());

	resource::response
	{
		client, e.what(), {}, http::INTERNAL_SERVER_ERROR
	};

	throw;
}

void
ircd::handle_request(client &client,
                     parse::capstan &pc,
                     const http::request::head &head)
{
	log::debug("client[%s] HTTP %s `%s' (content-length: %zu)",
	           string(remote(client)),
	           head.method,
	           head.path,
	           head.content_length);

	auto &resource
	{
		ircd::resource::find(head.path)
	};

	resource(client, pc, head);
}

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
{
	const auto &authorization
	{
		request.head.authorization
	};

	const fmt::bsprintf<1024> uri
	{
		"%s%s%s", request.head.path, request.query? "?" : "", request.query
	};

	const auto verified
	{
		m::io::verify_x_matrix_authorization(authorization, method.name, uri, request.content)
	};

	if(!verified)
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE", "The X-Matrix Authorization is invalid."
		};
}

void
ircd::resource::operator()(client &client,
                           parse::capstan &pc,
                           const http::request::head &head)
{
	auto &method(operator[](head.method));
	http::request::content content{pc, head};

	const auto pathparm
	{
		lstrip(head.path, this->path)
	};

	string_view param[8];
	const vector_view<string_view> parv
	{
		param, tokens(pathparm, '/', param)
	};

	resource::request request
	{
		head, content, head.query, parv
	};

	if(method.flags & method.REQUIRES_AUTH)
		authenticate(client, method, request);

	if(method.flags & method.VERIFY_ORIGIN)
		verify_origin(client, method, request);

	handle_request(client, method, request);
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
                               const handler &handler,
                               opts opts)
:name{name}
,resource{&resource}
,function{handler}
,flags{opts.flags}
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
                                 http::request::content &content,
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
	char buf[serialized(headers)];
	const mutable_buffer mb{buf, sizeof(buf)};
	stream_buffer sb{mb};
	write(sb, headers);
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
	const auto request_time
	{
		client.request_timer.at<microseconds>().count()
	};

	const fmt::bsprintf<64> rtime
	{
		"%zdus", request_time
	};

	const string_view cache_control
	{
		(code >= 200 && code < 300) ||
		(code >= 403 && code <= 405) ||
		(code >= 300 && code < 400)? "no-cache":
		""
	};

	char head_buf[2048];
	stream_buffer head{head_buf};
	http::response
	{
		head,
		code,
		content.size(),
		content_type,
		headers,
		{
			{ "Access-Control-Allow-Origin",   "*"            }, //TODO: XXX
			{ "Cache-Control",                 cache_control  },
			{ "X-IRCd-Request-Timer",          rtime,         },
		},
	};

	const ilist<const_buffer> vector
	{
		head.completed(),
		content
	};

	write_closure(client)(vector);

	log::debug("client[%s] HTTP %d %s in %ld$us; response in %ld$us (%s) content-length: %zu",
	           string(remote(client)),
	           int(code),
	           http::status(code),
	           request_time,
	           (client.request_timer.at<microseconds>().count() - request_time),
	           content_type,
	           content.size());
}
