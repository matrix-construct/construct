// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::resource::log)
ircd::resource::log
{
	"resource", 'r'
};

template<>
decltype(ircd::resource::allocator)
ircd::util::instance_map<ircd::string_view, ircd::resource, ircd::iless>::allocator
{};

template<>
decltype(ircd::resource::map)
ircd::util::instance_map<ircd::string_view, ircd::resource, ircd::iless>::map
{
	allocator
};

ircd::resource &
ircd::resource::find(const string_view &path_)
{
	const auto &resources{instance_map::map};
	auto it
	{
		resources.begin()
	};

	if(unlikely(it == resources.end()))
		throw http::error
		{
			http::code::NOT_FOUND
		};

	auto &resource
	{
		it->second
	};

	const string_view path
	{
		rstrip(path_, '/')
	};

	return path && path != "/"?
		resource->route(path):
		*resource;
}

//
// resource::resource
//

ircd::resource::resource(const string_view &path)
:resource
{
	path, {}
}
{
}

ircd::resource::resource(const string_view &path,
                         struct opts opts)
:instance_map
{
	rstrip(path, '/')
}
,path
{
	instance_map::it->first
}
,opts
{
	std::make_unique<const struct opts>(std::move(opts))
}
,default_method_head{[this]() -> std::unique_ptr<method>
{
	if(this->opts->flags & flag::OVERRIDE_HEAD)
		return {};

	auto handler
	{
		std::bind(&resource::handle_head, this, ph::_1, ph::_2)
	};

	return std::make_unique<method>(*this, "HEAD", std::move(handler));
}()}
,default_method_options{[this]() -> std::unique_ptr<method>
{
	if(this->opts->flags & flag::OVERRIDE_OPTIONS)
		return {};

	auto handler
	{
		std::bind(&resource::handle_options, this, ph::_1, ph::_2)
	};

	return std::make_unique<method>(*this, "OPTIONS", std::move(handler));
}()}
{
	log::debug
	{
		log, "Registered resource \"%s\"",
		path.empty()?
			string_view{"/"}:
			this->path
	};
}

ircd::resource::~resource()
noexcept
{
	log::debug
	{
		log, "Unregistered resource \"%s\"",
		path.empty()?
			string_view{"/"}:
			path
	};
}

ircd::string_view
ircd::resource::params(const string_view &path)
const
{
	const auto prefix_tokens
	{
		token_count(this->path, '/')
	};

	const auto params_after
	{
		prefix_tokens? prefix_tokens - 1: 0
	};

	const auto ret
	{
		tokens_after(path, '/', params_after)
	};

	return ret;
}

ircd::resource &
ircd::resource::route(const string_view &path)
const
{
	if(this->path != "/" && startswith(path, this->path))
		if(path == this->path || opts->flags & flag::DIRECTORY)
			return mutable_cast(*this);

	const auto &resources{instance_map::map};
	assert(!resources.empty());
	auto it
	{
		resources.lower_bound(path)
	};

	if(it == end(resources) || it->first > path)
		--it;

	if(it->second != this)
		return it->second->route(path);

	it = begin(resources);
	assert(it != end(resources));
	return mutable_cast(*it->second);
}

ircd::resource::method &
ircd::resource::operator[](const string_view &name)
const try
{
	return *methods.at(name);
}
catch(const std::out_of_range &e)
{
	thread_local char buf[512];
	const http::header headers[]
	{
		{ "Allow", method_list(buf) }
	};

	throw http::error
	{
		http::METHOD_NOT_ALLOWED, {}, headers
	};
}

ircd::resource::response
ircd::resource::handle_head(client &client,
                            const request &request)
const
{
	return response
	{
		client, http::METHOD_NOT_ALLOWED
	};
}

ircd::resource::response
ircd::resource::handle_options(client &client,
                               const request &request)
const
{
	const http::headers headers
	{
		request.head.headers
	};

	const string_view &request_origin
	{
		headers["origin"]
	};

	const string_view &allow_origin
	{
		resource::response::access_control_allow_origin
	};

	const string_view &request_headers
	{
		headers["access-control-request-headers"]
	};

	const string_view &allow_headers
	{
		request_headers
	};

	const string_view &request_method
	{
		headers["access-control-request-method"]
	};

	char allow_methods_buf[48];
	const string_view &allow_methods
	{
		method_list(allow_methods_buf, [&]
		(const method &method) noexcept
		{
			return true;
		})
	};

	const http::header response_headers[]
	{
		// ACAO sent further up stack
		//{ "Access-Control-Allow-Origin",   allow_origin   },
		{ "Access-Control-Allow-Methods",  allow_methods  },
		{ "Access-Control-Allow-Headers",  allow_headers  },
	};

	return response
	{
		client, {}, {}, http::OK, response_headers
	};
}

ircd::string_view
ircd::resource::method_list(const mutable_buffer &buf)
const
{
	return method_list(buf, []
	(const method &) noexcept
	{
		return true;
	});
}

ircd::string_view
ircd::resource::method_list(const mutable_buffer &buf,
                            const method_closure &closure)
const
{
	size_t len(0);
	if(likely(size(buf)))
		buf[len] = '\0';

	auto it(begin(methods));
	if(it != end(methods))
	{
		assert(it->second);
		if(closure(*it->second))
			len = strlcat(buf, it->first);

		for(++it; it != end(methods); ++it)
		{
			assert(it->second);
			if(!closure(*it->second))
				continue;

			len = strlcat(buf, ", ");
			len = strlcat(buf, it->first);
		}
	}

	return { data(buf), len };
}

///////////////////////////////////////////////////////////////////////////////
//
// resource/method.h
//

namespace ircd
{
	extern conf::item<seconds> cache_warmup_time;
	static void cache_warm_origin(const string_view &origin);
}

decltype(ircd::resource::method::idle_dock)
ircd::resource::method::idle_dock;

//
// method::opts
//

decltype(ircd::resource::method::default_timeout)
ircd::resource::method::default_timeout
{
	{ "name",    "ircd.resource.method.default.timeout" },
	{ "default", 30L                                    },
};

decltype(ircd::resource::method::default_payload_max)
ircd::resource::method::default_payload_max
{
	{ "name",    "ircd.resource.method.default.payload_max" },
	{ "default", long(128_KiB)                              },
};

//
// method::stats
//

namespace ircd
{
	static thread_local char method_stats_name_buf[128];
	static string_view method_stats_name(resource::method &, const string_view &);
}

ircd::resource::method::stats::stats(method &m)
:pending
{
	{ "name", method_stats_name(m, "pending") }
}
,requests
{
	{ "name", method_stats_name(m, "requests") }
}
,timeouts
{
	{ "name", method_stats_name(m, "timeouts") }
}
,completions
{
	{ "name", method_stats_name(m, "completed") }
}
,internal_errors
{
	{ "name", method_stats_name(m, "internal_errors") }
}
{
}

ircd::string_view
ircd::method_stats_name(resource::method &m,
                        const string_view &key)
{
	assert(m.resource);
	assert(m.resource->path);
	assert(m.name);
	assert(key);
	return fmt::sprintf
	{
		method_stats_name_buf, "ircd.resource.%s.%s.%s",
		m.resource->path,
		m.name,
		key,
	};
}
//
// method::method
//

ircd::resource::method::method(struct resource &resource,
                               const string_view &name,
                               handler function)
:method
{
	resource, name, std::move(function), {}
}
{
}

ircd::resource::method::method(struct resource &resource,
                               const string_view &name,
                               handler function,
                               struct opts opts)
:resource
{
	&resource
}
,name
{
	name
}
,function
{
	std::move(function)
}
,opts
{
	std::make_unique<const struct opts>(std::move(opts))
}
,stats
{
	std::make_unique<struct stats>(*this)
}
,methods_it{[this, &name]
{
	const auto iit
	{
		this->resource->methods.emplace(this->name, this)
	};

	if(!iit.second)
		throw error
		{
			"Resource method '%s' already registered to '%s'",
			name,
			this->resource->path
		};

	return unique_const_iterator<decltype(resource::methods)>
	{
		this->resource->methods, iit.first
	};
}()}
{
}

ircd::resource::method::~method()
noexcept
{
	assert(resource);
	if(stats && stats->pending)
		log::dwarning
		{
			"Resource '%s' method '%s' still waiting for %zu pending requests",
			resource->path,
			name,
			uint64_t(stats->pending),
		};

	// No point in waiting without a context...
	if(unlikely(!ctx::current))
		return;

	// Wait until the method has completed requests in progress.
	const ctx::uninterruptible::nothrow ui;
	idle_dock.wait([this]() noexcept
	{
		return !stats || stats->pending == 0;
	});
}

ircd::resource::response
ircd::resource::method::operator()(client &client,
                                   const http::request::head &head,
                                   const string_view &content_partial)
try
{
	const unwind on_idle{[this]
	{
		if(stats->pending == 0)
			idle_dock.notify_all();
	}};

	++stats->requests;
	const scope_count pending
	{
		static_cast<uint64_t &>(stats->pending)
	};

	// Bail out if the method limited the amount of content and it was exceeded.
	if(!content_length_acceptable(head))
		throw http::error
		{
			http::PAYLOAD_TOO_LARGE
		};

	// Check if the resource method wants a specific MIME type. If no option
	// is given by the resource then any Content-Type by the client will pass.
	if(!mime_type_acceptable(head))
		throw http::error
		{
			http::UNSUPPORTED_MEDIA_TYPE
		};

	// This timer will keep the request from hanging forever for whatever
	// reason. The resource method may want to do its own timing and can
	// disable this in its options structure.
	const auto &method_timeout
	{
		opts->timeout != 0s?
			opts->timeout:
			seconds(default_timeout)
	};

	const net::scope_timeout timeout
	{
		*client.sock, method_timeout, [this, &client]
		(const bool &timed_out)
		{
			if(timed_out)
				this->handle_timeout(client);
		}
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

	// When we have incomplete content it's a good time to TCP_QUICKACK to
	// coax the client into sending more as soon as possible. If we don't do
	// this we risk waiting for our own kernel's delayed-acknowledgment timer
	// in the subsequent reads for content below (or in the handler). We don't
	// QUICKACK when we've received all content since we might be able to make
	// an actual response all in one shot.
	if(content_remain && ~opts->flags & DELAYED_ACK)
		net::quickack(*client.sock, true);

	// Branch taken to receive any remaining content in the common case where
	// the resource handler does not perform its own consumption of content.
	if(content_remain && ~opts->flags & CONTENT_DISCRETION)
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

	// We take the extra step here to clear the assignment to client.request
	// when this request stack has finished for two reasons:
	// - It allows other ctxs to peep at the client::list to see what this
	//   client/ctx/request is currently working on with some more safety.
	// - It prevents an easy source for stale refs wrt the longpoll thing.
	const scope_restore client_request
	{
		client.request, resource::request
		{
			head, content
		}
	};

	// The path components after the resource->path become the parameter
	// vector (or parv[]) passed to the resource as its arguments.
	client.request.params = resource->params(head.path);
	client.request.params = strip(client.request.params, '/');
	client.request.parv =
	{
		client.request.param,
		tokens(client.request.params, '/', client.request.param)
	};

	// Start the TCP cork if the method has this option set.
	if(opts->flags & RESPONSE_NOPUSH)
	{
		assert(client.sock);
		net::nopush(*client.sock, true);
	}

	// Finally handle the request.
	const auto &ret
	{
		call_handler(client, client.request)
	};

	// Increment the successful completion counter for the handler.
	++stats->completions;

	// Stop the TCP cork if the method has this option set.
	if(opts->flags & RESPONSE_NOPUSH)
	{
		assert(client.sock);
		net::nopush(*client.sock, false);
	}

	// This branch flips TCP_NODELAY to force transmission here. This is a
	// good place because the request has finished writing everything; the
	// socket doesn't know that, but we do, and this is the place. The action
	// can be disabled by using the flag in the method's options.
	if(likely(~opts->flags & RESPONSE_NOFLUSH))
	{
		assert(client.sock);
		net::flush(*client.sock);
	}

	return ret;
}
catch(const http::error &e)
{
	if(unlikely(e.code == http::INTERNAL_SERVER_ERROR))
		++stats->internal_errors;

	throw;
}
catch(const std::system_error &)
{
	throw;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(...)
{
	++stats->internal_errors;
	throw;
}

ircd::resource::response
ircd::resource::method::call_handler(client &client,
                                     resource::request &request)
try
{
	return function(client, request);
}
catch(const ctx::timeout &e)
{
	throw http::error
	{
		"%s", http::REQUEST_TIMEOUT, e.what()
	};
}
catch(const mods::unavailable &e)
{
	throw http::error
	{
		"%s", http::SERVICE_UNAVAILABLE, e.what()
	};
}
catch(const std::bad_function_call &e)
{
	throw http::error
	{
		"%s", http::SERVICE_UNAVAILABLE, e.what()
	};
}
catch(const std::out_of_range &e)
{
	throw http::error
	{
		"%s", http::NOT_FOUND, e.what()
	};
}

void
ircd::resource::method::handle_timeout(client &client)
const
{
	log::derror
	{
		log, "%s Timed out in %s `%s'",
		loghead(client),
		name,
		resource->path
	};

	stats->timeouts++;

	// The interrupt is effective when the socket has already been
	// closed and/or the client is still stuck in a request for
	// some reason.
	if(client.reqctx)
		ctx::interrupt(*client.reqctx);

	//TODO: If we know that no response has been sent yet
	//TODO: we can respond with http::REQUEST_TIMEOUT instead.
	client.close(net::dc::RST, net::close_ignore);
}

bool
ircd::resource::method::mime_type_acceptable(const http::request::head &head)
const
{
	assert(opts);

	const auto &[required_registry, required_format]
	{
		opts->mime
	};

	const auto &[supplied, charset]
	{
		split(head.content_type, ';')
	};

	const auto &[supplied_registry, supplied_format]
	{
		split(supplied, '/')
	};

	const bool match[]
	{
		!required_registry || iequals(required_registry, supplied_registry),
		!required_format || iequals(required_format, supplied_format),
	};

	return all(match);
}

bool
ircd::resource::method::content_length_acceptable(const http::request::head &head)
const
{
	assert(opts);
	assert(opts->payload_max != 0UL);

	const auto &payload_max
	{
		opts->payload_max != -1UL?
			opts->payload_max:
			size_t(default_payload_max)
	};

	return head.content_length <= payload_max;
}

///////////////////////////////////////////////////////////////////////////////
//
// resource/request.h
//

ircd::resource::request::request(const http::request::head &head,
                                 const string_view &content)
noexcept
:json::object
{
	content
}
,head
{
	head
}
,content
{
	content
}
,query
{
	this->head.query
}
,agent
{
	parse_agent(this->head)
}
{
}

[[gnu::visibility("internal")]]
ircd::pair<ircd::string_view>
ircd::resource::request::parse_agent(const http::request::head &head)
noexcept
{
	const auto &user_agent
	{
		head.user_agent
	};

	const auto &[primary, info]
	{
		split(user_agent, ' ')
	};

	const auto &[name, agent]
	{
		split(primary, '/')
	};

	return
	{
		name, agent
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// resource/response.h
//

//
// resource::response::chunked
//

ircd::resource::response::chunked::chunked(client &client,
                                           const http::code &code,
                                           const size_t &buffer_size,
                                           const mutable_buffer &buf)
:chunked
{
	client,
	code,
	"application/json; charset=utf-8"_sv,
	string_view{},
	buffer_size,
	buf,
}
{
}

ircd::resource::response::chunked::chunked(client &client,
                                           const http::code &code,
                                           const vector_view<const http::header> &headers,
                                           const size_t &buffer_size,
                                           const mutable_buffer &buf)
:chunked
{
	client,
	code,
	"application/json; charset=utf-8"_sv,
	headers,
	buffer_size,
	buf,
}
{
}

ircd::resource::response::chunked::chunked(client &client,
                                           const http::code &code,
                                           const string_view &content_type,
                                           const size_t &buffer_size,
                                           const mutable_buffer &buf)
:chunked
{
	client,
	code,
	content_type,
	string_view{}, // headers
	buffer_size,
	buf,
}
{
}

ircd::resource::response::chunked::chunked(client &client,
                                           const http::code &code,
                                           const string_view &content_type,
                                           const vector_view<const http::header> &headers,
                                           const size_t &buffer_size,
                                           const mutable_buffer &buf)
:chunked
{
	client,
	code,
	content_type,
	[&headers]
	{
		// Note that the headers which are composed into this buffer are
		// copied again before the response goes out from resource::response.
		// There must not be any context switch between now and that copy so
		// we can return a string_view of this TLS buffer.
		const critical_assertion ca;

		thread_local char buffer[4_KiB];
		window_buffer sb{buffer};
		http::write(sb, headers);
		return string_view{sb.completed()};
	}(),
	buffer_size,
	buf,
}
{
}

decltype(ircd::resource::response::chunked::default_buffer_size)
ircd::resource::response::chunked::default_buffer_size
{
	{ "name",    "ircd.resource.response.chunked.buffer_size" },
	{ "default", long(128_KiB)                                },
};

ircd::resource::response::chunked::chunked(client &client,
                                           const http::code &code,
                                           const string_view &content_type,
                                           const string_view &headers,
                                           const size_t &buffer_size,
                                           const mutable_buffer &buf)
:response
{
	client,
	code,
	content_type,
	size_t(-1),
	headers
}
,c
{
	&client
}
,_buf
{
	buffer_size
}
,buf
{
	buffer_size? _buf: buf
}
{
	assert(!empty(content_type));
	assert(buffer_size > 0 || empty(_buf));
	assert(buffer_size > 0 || !empty(buf));
	assert(buffer_size == 0 || empty(buf));
	assert(buffer_size == 0 || !empty(_buf));
}

ircd::resource::response::chunked::~chunked()
noexcept try
{
	if(!c)
		return;

	if(std::uncaught_exceptions())
	{
		log::derror
		{
			log, "%s HTTP response chunks:%u wrote:%zu flushed:%zu :stream interrupted...",
			loghead(*c),
			count,
			wrote,
			flushed,
		};

		c->close(net::dc::SSL_NOTIFY, net::close_ignore);
		return;
	}

	finish();
}
catch(...)
{
	return;
}

std::function<ircd::const_buffer (const ircd::const_buffer &)>
ircd::resource::response::chunked::flusher()
{
	return std::bind(&chunked::flush, this, ph::_1);
}

bool
ircd::resource::response::chunked::finish(const bool psh)
{
	if(!c)
		return false;

	write(const_buffer{}, false);
	assert(finished);

	if(psh)
		net::flush(*c->sock);

	assert(count > 0);
	char tmbuf[32];
	log::debug
	{
		log, "%s HTTP --- `%s' in %s wrote:%zu flushed:%zu chunks:%u psh:%b",
		loghead(*c),
		c->request.head.path,
		pretty(tmbuf, c->timer.at<microseconds>(), true),
		wrote,
		flushed,
		count - 1,    // no count terminator chunk
		psh,
	};

	c = nullptr;
	return true;
}

ircd::const_buffer
ircd::resource::response::chunked::flush(const const_buffer &buf)
{
	assert(size(buf) <= size(this->buf) || empty(this->buf));
	const size_t wrote
	{
		write(buf, true)
	};

	assert(wrote > 0 || empty(buf));
	const size_t flushed
	{
		std::min(size(buf), wrote)
	};

	assert(flushed <= size(buf));
	this->flushed += flushed;
	assert(this->flushed <= this->wrote);
	return const_buffer
	{
		data(buf), flushed
	};
}

size_t
ircd::resource::response::chunked::write(const const_buffer &chunk,
                                         const bool &ignore_empty)
try
{
	assert(size(chunk) <= size(this->buf) || empty(this->buf));
	assert(!finished);
	if(!c)
		return 0UL;

	if(empty(chunk) && ignore_empty)
		return 0UL;

	char headbuf[32];
	const const_buffer iov[]
	{
		// head
		http::writechunk(headbuf, size(chunk)),

		// body
		chunk,

		// terminator,
		http::response::chunk::terminator,
	};

	const size_t wrote
	{
		this->wrote
	};

	this->wrote += c->write_all(iov);
	finished |= empty(chunk);
	count++;

	assert(this->wrote >= wrote);
	assert(this->wrote >= 2 || !finished);
	return this->wrote - wrote;
}
catch(...)
{
	this->c = nullptr;
	throw;
}

//
// resource::response
//

// A buffer of this size will be passed to the socket and sent
// out and must be on stack.
decltype(ircd::resource::response::HEAD_BUF_SZ)
ircd::resource::response::HEAD_BUF_SZ
{
	4_KiB
};

ircd::resource::response::response(client &client,
                                   const http::code &code)
:response
{
	client,
	json::object{json::empty_object},
	code
}
{
}

ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const json::iov &members)
:response
{
	client,
	members,
	code
}
{
}

ircd::resource::response::response(client &client,
                                   const json::members &members,
                                   const http::code &code)
:response
{
	client,
	code,
	members
}
{
}

ircd::resource::response::response(client &client,
                                   const json::value &value,
                                   const http::code &code)
:response
{
	client,
	code,
	value
}
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

	const string_view str
	{
		stringify(mutable_buffer{buffer}, value)
	};

	switch(type(value))
	{
		case json::ARRAY:
		{
			response
			{
				client,
				json::array{str},
				code,
			};

			return;
		}

		case json::OBJECT:
		{
			response
			{
				client,
				json::object{str},
				code,
			};

			return;
		}

		[[unlikely]]
		default:
			throw http::error
			{
				"Cannot send json::%s as response content",
				http::INTERNAL_SERVER_ERROR,
				type(value),
			};
	}
}
catch(const json::error &e)
{
	throw http::error
	{
		"Generator Protection: %s",
		http::INTERNAL_SERVER_ERROR,
		e.what(),
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

	response
	{
		client,
		object,
		code,
	};
}
catch(const json::error &e)
{
	throw http::error
	{
		"Generator Protection: %s",
		http::INTERNAL_SERVER_ERROR,
		e.what()
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

	response
	{
		client,
		object,
		code,
	};
}
catch(const json::error &e)
{
	throw http::error
	{
		"Generator Protection: %s",
		http::INTERNAL_SERVER_ERROR,
		e.what(),
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
	response
	{
		client,
		string_view{object},
		content_type,
		code,
	};
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
	response
	{
		client,
		string_view{array},
		content_type,
		code,
	};
}

ircd::resource::response::response(client &client,
                                   const string_view &content,
                                   const string_view &content_type,
                                   const http::code &code,
                                   const vector_view<const http::header> &headers)
{
	assert(empty(content) || !empty(content_type));

	// contents of this buffer get copied again when further passed to
	// response{}; we can get this off the stack if that remains true.
	thread_local char buffer[4_KiB];
	window_buffer sb{buffer};
	{
		const critical_assertion ca;
		http::write(sb, headers);
	}

	response
	{
		client,
		content,
		content_type,
		code,
		string_view{sb.completed()},
	};
}

ircd::resource::response::response(client &client,
                                   const string_view &content,
                                   const string_view &content_type,
                                   const http::code &code,
                                   const string_view &headers)
{
	assert(empty(content) || !empty(content_type));

	// Head gets sent
	response
	{
		client,
		code,
		content_type,
		size(content),
		headers,
		content,
	};
}

decltype(ircd::resource::response::access_control_allow_origin)
ircd::resource::response::access_control_allow_origin
{
	{ "name",      "ircd.resource.access_control.allow_origin" },
	{ "default",   "*"                                         }
};

__attribute__((stack_protect))
ircd::resource::response::response(client &client,
                                   const http::code &code,
                                   const string_view &content_type,
                                   const size_t &content_length,
                                   const string_view &headers,
                                   const string_view &content)
{
	// content may be empty if the caller wants to send it themselves, but
	// either way the type and length must still be passed by caller.
	assert(!content || content_length);
	assert(!content_length || !empty(content_type));

	const auto request_time
	{
		client.timer.at<microseconds>()
	};

	char rtime_buf[32];
	const string_view rtime
	{
		pretty(rtime_buf, request_time, true)
	};

	const http::header headers_addl[]
	{
		{ "X-IRCd-Request-Timer",         rtime                                    },
		{ "Access-Control-Allow-Origin",  string_view(access_control_allow_origin) },
	};

	char head_buf[HEAD_BUF_SZ];
	window_buffer head{head_buf};
	http::response
	{
		head,
		code,
		content_length,
		content_type,
		headers,
		headers_addl,
	};

	// Maximum size is realistically ok but ideally a small
	// maximum; this exception should hit the developer in testing.
	if(unlikely(!head.remaining()))
		throw panic
		{
			"HTTP headers too large for buffer of %zu",
			sizeof(head_buf),
		};

	const const_buffer iov[]
	{
		head.completed(),
		content,
	};

	size_t wrote {0};
	std::exception_ptr eptr; try
	{
		wrote += client.write_all(iov);
	}
	catch(...)
	{
		eptr = std::current_exception();
	}

	if constexpr(RB_DEBUG_LEVEL)
	{
		const log::level level
		{
			http::severity(http::category(code))
		};

		log::logf
		{
			log, level,
			"%s HTTP %u `%s' %s in %s; %s head:%zu content:%s %s%s",
			loghead(client),
			uint(code),
			client.request.head.path,
			http::status(code),
			rtime,
			content_type,
			size(iov[0]),
			ssize_t(content_length) >= 0?
				lex_cast(content_length):
				"chunked"_sv,
			eptr?
				"error:"_sv:
				string_view{},
			what(eptr)
		};
	}

	if(unlikely(eptr))
		std::rethrow_exception(eptr);

	assert(wrote == buffers::size(vector_view(iov)));
}

///////////////////////////////////////////////////////////////////////////////
//
// resource/redirect.h
//

//
// redirect::permanent::permanent
//

ircd::resource::redirect::permanent::permanent(const string_view &old_path,
                                               const string_view &new_path,
                                               struct opts opts)
:resource
{
	old_path, std::move(opts)
}
,new_path
{
	new_path
}
,_options
{
	*this, "OPTIONS", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_trace
{
	*this, "TRACE", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_head
{
	*this, "HEAD", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_get
{
	*this, "GET", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_put
{
	*this, "PUT", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_post
{
	*this, "POST", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_patch
{
	*this, "PATCH", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
,_delete
{
	*this, "DELETE", std::bind(&permanent::handler, this, ph::_1, ph::_2)
}
{
}

ircd::resource::response
ircd::resource::redirect::permanent::handler(client &client,
                                             const request &request)
{
	thread_local char buf[response::HEAD_BUF_SZ];

	const string_view postfix
	{
		lstrip(request.head.uri, this->path)
	};

	const string_view location{fmt::sprintf
	{
		buf, "%s/%s",
		rstrip(this->new_path, '/'),
		lstrip(postfix, '/')
	}};

	return response
	{
		client, {}, {}, http::PERMANENT_REDIRECT,
		{
			http::header { "Location", location }
		}
	};
}
