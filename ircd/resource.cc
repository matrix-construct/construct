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

namespace ircd {

IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(resource::resources)
resource::resources
{};

} // namespace ircd

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

ircd::resource::resource(const string_view &path,
                         opts opts)
:resource
{
	path, opts.description, opts
}
{
}

ircd::resource::resource(const string_view &path,
                         const string_view &description,
                         opts opts)
:path{path}
,description{description}
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
			request.user_id = at<"sender"_>(event);
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
	throw http::error
	{
		http::METHOD_NOT_ALLOWED
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
	const auto content_type
	{
		"application/json; charset=utf-8"
	};

	response(client, object, content_type, code);
}

ircd::resource::response::response(client &client,
                                   const json::array &array,
                                   const http::code &code)
{
	const auto content_type
	{
		"application/json; charset=utf-8"
	};

	response(client, array, content_type, code);
}

ircd::resource::response::response(client &client,
                                   const string_view &str,
                                   const string_view &content_type,
                                   const http::code &code)
{
	const auto request_time
	{
		client.request_timer.at<microseconds>().count()
	};

	char rtime[64]; const auto rtime_len
	{
		snprintf(rtime, sizeof(rtime), "%zdus",
		         request_time)
	};

	http::response
	{
		code, str, write_closure(client),
		{
			{ "Content-Type", content_type },
			{ "Access-Control-Allow-Origin", "*" }, //TODO: XXX
			{ "X-IRCd-Request-Timer", string_view{rtime, size_t(rtime_len)} }
		}
	};

	log::debug("client[%s] HTTP %d %s in %ld$us; response in %ld$us (%s) content-length: %zu",
	           string(remote(client)),
	           int(code),
	           http::status(code),
	           request_time,
	           (client.request_timer.at<microseconds>().count() - request_time),
	           content_type,
	           str.size());
}
