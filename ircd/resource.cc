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

#include <ircd/m.h>

namespace ircd {

IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(resource::resources)
resource::resources
{};

IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(resource::tokens)
resource::tokens
{};

} // namespace ircd

ircd::resource &
ircd::resource::find(string_view path)
{
    path = lstrip(path, '/');
    path = strip(path, '/');
	auto it(resources.lower_bound(path));
	if(it == end(resources))
		return *resources.at(string_view{});

	// Exact file or directory match
	if(path == it->first)
		return *it->second;

	// Directories handle all paths under them.
	if(!startswith(path, it->first))
	{
		// Walk the iterator back to find if there is a directory prefixing this path.
		if(it == begin(resources))
			throw http::error(http::code::NOT_FOUND);

		--it;
		if(!startswith(path, it->first))
			throw http::error(http::code::NOT_FOUND);
	}

	// Check if the resource is a directory; if not, it can only
	// handle exact path matches.
	if(~it->second->flags & it->second->DIRECTORY && path != it->first)
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
	log::info("Registered resource \"%s\" by handler @ %p", path, this);
}

ircd::resource::~resource()
noexcept
{
	log::info("Unregistered resource \"%s\" by handler @ %p", path, this);
}

namespace ircd {

static void
authenticate(client &client,
             resource::method &method,
             resource::request &request)
try
{
	const auto &access_token(request.query.at("access_token"));
	if(access_token == "charybdisLETMEIN")
		return;

	const auto it(resource::tokens.find(access_token));
	if(it == end(resource::tokens) || it->second != &client)
	{
		throw m::error
		{
			// "When credentials are required but missing or invalid, the HTTP call will "
			// "return with a status of 401 and the error code, M_MISSING_TOKEN or "
			// "M_UNKNOWN_TOKEN respectively."
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN", "Credentials for this method are required but invalid."
		};
	}
}
catch(const std::out_of_range &e)
{
	throw m::error
	{
		// "When credentials are required but missing or invalid, the HTTP call will return "
		// "with a status of 401 and the error code, M_MISSING_TOKEN or M_UNKNOWN_TOKEN "
		// "respectively."
		http::UNAUTHORIZED, "M_MISSING_TOKEN", "Credentials for this method are required but missing."
	};
}

} // namespace ircd

void
ircd::resource::operator()(client &client,
                           parse::capstan &pc,
                           const http::request::head &head)
{
	auto &method(operator[](head.method));
	http::request::content content{pc, head};
	resource::request request
	{
		head, content, head.query
	};

	if(method.flags & method.REQUIRES_AUTH)
		authenticate(client, method, request);

	handle_request(client, method, request);
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

void
ircd::resource::handle_request(client &client,
                               method &method,
                               resource::request &request)
try
{
	method(client, request);
}
catch(const json::error &e)
{
	throw m::error
	{
		"M_BAD_JSON", "Required JSON field: %s", e.what()
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
                                 http::query::string query)
:json::doc{content}
,head{head}
,content{content}
,query{std::move(query)}
{
}

ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const json::obj &obj)
:response{client, obj, code}
{
}

ircd::resource::response::response(client &client,
                                   const json::obj &obj,
                                   const http::code &code)
try
{
	char cbuf[4096], *out(cbuf);
	const auto doc(serialize(obj, out, cbuf + sizeof(cbuf)));
	response(client, doc, code);
}
catch(const json::error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON", "Generator Protection: %s", e.what()
	};
}

ircd::resource::response::response(client &client,
                                   const json::doc &doc,
                                   const http::code &code)
{
	const auto content_type
	{
		"application/json; charset=utf-8"
	};

	response(client, doc, content_type, code);
}

ircd::resource::response::response(client &client,
                                   const string_view &str,
                                   const string_view &content_type,
                                   const http::code &code)
{
	http::response
	{
		code, str, write_closure(client),
		{
			{ "Content-Type", content_type },
			{ "Access-Control-Allow-Origin", "*" }
		}
	};

	log::debug("client[%s] HTTP %d %s (%s) content-length: %zu %s...",
	           string(remote_addr(client)),
	           int(code),
               http::reason[code],
               content_type,
               str.size(),
               startswith(content_type, "text") ||
               content_type == "application/json" ||
               content_type == "application/javascript"? str.substr(0, 96) : string_view{});
}
