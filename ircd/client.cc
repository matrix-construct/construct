// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>

namespace ircd
{
	ctx::dock dock;

	template<class... args> std::shared_ptr<client> make_client(args&&...);
}

//
// client::settings conf::item's
//

ircd::conf::item<size_t>
ircd::client::settings::stack_size
{
	{ "name",     "ircd.client.stack_size"  },
	{ "default",  ssize_t(1_MiB)            },
};

ircd::conf::item<size_t>
ircd::client::settings::pool_size
{
	{
		{ "name",     "ircd.client.pool_size " },
		{ "default",  64L                      },
	}, []
	{
		using client = ircd::client;
		client::pool.set(client::settings::pool_size);
	}
};

/// Linkage for the default settings
decltype(ircd::client::settings)
ircd::client::settings
{};

//
// client::conf conf::item's
//

ircd::conf::item<ircd::seconds>
ircd::client::conf::async_timeout_default
{
	{ "name",     "ircd.client.conf.async_timeout" },
	{ "default",  60L                              },
};

ircd::conf::item<ircd::seconds>
ircd::client::conf::request_timeout_default
{
	{ "name",     "ircd.client.conf.request_timeout" },
	{ "default",  30L                                },
};

ircd::conf::item<size_t>
ircd::client::conf::header_max_size_default
{
	{ "name",     "ircd.client.conf.header_max_size" },
	{ "default",  ssize_t(8_KiB)                     },
};

/// Linkage for the default conf
decltype(ircd::client::default_conf)
ircd::client::default_conf
{};

//
// linkages
//

/// The pool of request contexts. When a client makes a request it does so by acquiring
/// a stack from this pool. The request handling and response logic can then be written
/// in a synchronous manner as if each connection had its own thread.
ircd::ctx::pool
ircd::client::pool
{
	"client", size_t(settings.stack_size)
};

decltype(ircd::client::ctr)
ircd::client::ctr
{};

// Linkage for the container of all active clients for iteration purposes.
template<>
decltype(ircd::util::instance_multimap<ircd::net::ipport, ircd::client>::map)
ircd::util::instance_multimap<ircd::net::ipport, ircd::client>::map
{};

//
// init
//

ircd::client::init::init()
{
	spawn();
}

ircd::client::init::~init()
noexcept
{
	const ctx::uninterruptible::nothrow ui;

	interrupt();
	close();
	wait();

	log::debug
	{
		"All client contexts, connections, and requests are clear.",
	};

	assert(client::map.empty());
}

void
ircd::client::init::interrupt()
{
	interrupt_all();
}

void
ircd::client::init::close()
{
	close_all();
}

void
ircd::client::init::wait()
{
	wait_all();
}

//
// util
//

void
ircd::client::spawn()
{
	pool.add(size_t(settings.pool_size));
}

void
ircd::client::interrupt_all()
{
	if(pool.active())
		log::warning
		{
			"Terminating %zu active of %zu client request contexts; %zu pending; %zu queued",
			pool.active(),
			pool.size(),
			pool.pending(),
			pool.queued()
		};

	pool.terminate();
}

void
ircd::client::close_all()
{
	if(!client::map.empty())
		log::debug
		{
			"Closing %zu clients", client::map.size()
		};

	auto it(begin(client::map));
	while(it != end(client::map))
	{
		auto c(shared_from(*it->second)); ++it; try
		{
			c->close(net::dc::RST, [c](const auto &e)
			{
				dock.notify_one();
			});
		}
		catch(const std::exception &e)
		{
			log::derror
			{
				"Error disconnecting client @%p: %s", c.get(), e.what()
			};
		}
	}
}

void
ircd::client::wait_all()
{
	if(pool.active())
		log::dwarning
		{
			"Waiting on %zu active of %zu client request contexts; %zu pending; %zu queued.",
			pool.active(),
			pool.size(),
			pool.pending(),
			pool.queued()
		};

	while(!client::map.empty())
		if(!dock.wait_for(seconds(2)) && !client::map.empty())
			log::warning
			{
				"Waiting for %zu clients to close...", client::map.size()
			};

	log::debug
	{
		"Joining %zu active of %zu client request contexts; %zu pending; %zu queued",
		pool.active(),
		pool.size(),
		pool.pending(),
		pool.queued()
	};

	pool.join();
}

ircd::parse::read_closure
ircd::read_closure(client &client)
{
	// Returns a function the parser can call when it wants more data
	return [&client](char *&start, char *const &stop)
	{
		char *const got(start);
		read(client, start, stop);
		//std::cout << ">>>> " << std::distance(got, start) << std::endl;
		//std::cout << string_view{got, start} << std::endl;
		//std::cout << "----" << std::endl;
    };
}

char *
ircd::read(client &client,
           char *&start,
           char *const &stop)
{
	assert(client.sock);
	auto &sock(*client.sock);
	const mutable_buffer buf
	{
		start, stop
	};

	char *const base(start);
	start += net::read(sock, buf);
	return base;
}

std::shared_ptr<ircd::client>
ircd::add_client(std::shared_ptr<socket> s)
{
	if(unlikely(ircd::runlevel != ircd::runlevel::RUN))
	{
		log::dwarning
		{
			"Refusing to add new client from %s in runlevel %s",
			string(remote_ipport(*s)),
			reflect(ircd::runlevel)
		};

		net::close(*s, net::dc::RST, net::close_ignore);
		return {};
	}

	const auto client
	{
		make_client(std::move(s))
	};

	client->async();
	return client;
}

template<class... args>
std::shared_ptr<ircd::client>
ircd::make_client(args&&... a)
{
	return std::make_shared<client>(std::forward<args>(a)...);
}

ircd::ipport
ircd::local(const client &client)
{
	if(!client.sock)
		return {};

	return net::local_ipport(*client.sock);
}

ircd::ipport
ircd::remote(const client &client)
{
	if(!client.sock)
		return {};

	return net::remote_ipport(*client.sock);
}

//
// async loop
//

namespace ircd
{
	using error_code = boost::system::error_code;

	static bool handle_ec_default(client &, const error_code &);
	static bool handle_ec_timeout(client &);
	static bool handle_ec_short_read(client &);
	static bool handle_ec_eof(client &);
	static bool handle_ec(client &, const error_code &);

	static void handle_client_request(std::shared_ptr<client>);
	static void handle_client_ready(std::shared_ptr<client>, const error_code &ec);
}

/// This function is the basis for the client's request loop. We still use
/// an asynchronous pattern until there is activity on the socket (a request)
/// in which case the switch to synchronous mode is made by jumping into an
/// ircd::context drawn from the request pool. When the request is finished,
/// the client exits back into asynchronous mode until the next request is
/// received and rinse and repeat.
//
/// This sequence exists to avoid any possible c10k-style limitation imposed by
/// dedicating a context and its stack space to the lifetime of a connection.
/// This is similar to the thread-per-request pattern before async was in vogue.
///
/// This call returns immediately so we no longer block the current context and
/// its stack while waiting for activity on idle connections between requests.
bool
ircd::client::async()
{
	assert(bool(this->sock));
	assert(bool(this->conf));
	auto &sock(*this->sock);
	const auto &timeout
	{
		conf->async_timeout
	};

	const net::wait_opts opts
	{
		net::ready::READ, timeout
	};

	auto handler
	{
		std::bind(ircd::handle_client_ready, shared_from(*this), ph::_1)
	};

	sock(opts, std::move(handler));
	return true;
}

/// The client's socket is ready for reading. This intermediate handler
/// intercepts any errors otherwise dispatches the client to the request
/// pool to be married with a stack. Right here this handler is executing on
/// the main stack (not in any ircd::context).
///
/// The context the closure ends up getting is the next available from the
/// request pool, which may not be available immediately so this handler might
/// be queued for some time after this call returns.
void
ircd::handle_client_ready(std::shared_ptr<client> client,
                          const error_code &ec)
{
	if(!handle_ec(*client, ec))
		return;

	auto handler
	{
		std::bind(ircd::handle_client_request, std::move(client))
	};

	if(client::pool.avail() == 0)
		log::dwarning
		{
			"Client context pool exhausted. %zu requests queued.",
			client::pool.queued()
		};

	client::pool(std::move(handler));
}

/// A request context has been dispatched and is now handling this client.
/// This function is executing on that ircd::ctx stack. client::main() will
/// now be called and synchronous programming is possible. Afterward, the
/// client will release this ctx and its stack and fall back to async mode
/// or die.
void
ircd::handle_client_request(std::shared_ptr<client> client)
try
{
	// The ircd::ctx now handling this request is referenced and accessible
	// in client for the duration of this handling.
	assert(ctx::current);
	assert(!client->reqctx);
	client->reqctx = ctx::current;
	const unwind reset{[&client]
	{
		assert(bool(client));
		assert(client->reqctx);
		assert(client->reqctx == ctx::current);
		client->reqctx = nullptr;
	}};

	if(!client->main())
	{
		client->close(net::dc::SSL_NOTIFY).wait();
		return;
	}

	client->async();
}
catch(const std::exception &e)
{
	log::derror
	{
		"socket(%p) client(%p) (below main) :%s",
		client->sock.get(),
		client.get(),
		e.what()
	};
}

/// This error handling switch is one of two places client errors
/// are handled. This handles the errors when the client is in async
/// mode rather than during a request. This executes on the main/callback
/// stack, not in any ircd::ctx, and must be asynchronous.
///
bool
ircd::handle_ec(client &client,
                const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::system::system_category;
	using boost::asio::error::get_ssl_category;
	using boost::asio::error::get_misc_category;

	if(ec.category() == system_category()) switch(ec.value())
	{
		case success:                return true;
		case operation_canceled:     return false;
		case timed_out:              return handle_ec_timeout(client);
		default:                     return handle_ec_default(client, ec);
	}
	else if(ec.category() == get_misc_category()) switch(ec.value())
	{
		case asio::error::eof:       return handle_ec_eof(client);
		default:                     return handle_ec_default(client, ec);
	}
	else if(ec.category() == get_ssl_category()) switch(uint8_t(ec.value()))
	{
		case SSL_R_SHORT_READ:       return handle_ec_short_read(client);
		default:                     return handle_ec_default(client, ec);
	}
	else return handle_ec_default(client, ec);
}

/// The client indicated they will not be sending the data we have been
/// waiting for. The proper behavior now is to initiate a clean shutdown.
bool
ircd::handle_ec_eof(client &client)
try
{
	log::debug
	{
		"socket(%p) local[%s] remote[%s] end of file",
		client.sock.get(),
		string(local(client)),
		string(remote(client))
	};

	client.close(net::dc::SSL_NOTIFY, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::error
	{
		"socket(%p) EOF: %s",
		client.sock.get(),
		e.what()
	};

	return false;
}

/// The client terminated the connection, likely improperly, and SSL
/// is informing us with an opportunity to prevent truncation attacks.
/// Best behavior here is to just close the sd.
bool
ircd::handle_ec_short_read(client &client)
try
{
	log::dwarning
	{
		"socket(%p) local[%s] remote[%s] short_read",
		client.sock.get(),
		string(local(client)),
		string(remote(client))
	};

	client.close(net::dc::RST, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::error
	{
		"socket(%p) short_read: %s",
		client.sock.get(),
		e.what()
	};

	return false;
}

/// The net:: system determined the client timed out because we set a timer
/// on the socket waiting for data which never arrived. The client may very
/// well still be there, so the best thing to do is to attempt a clean
/// disconnect.
bool
ircd::handle_ec_timeout(client &client)
try
{
	assert(bool(client.sock));
	log::debug
	{
		"socket(%p) local[%s] remote[%s] disconnecting after inactivity timeout",
		client.sock.get(),
		string(local(client)),
		string(remote(client))
	};

	client.close(net::dc::SSL_NOTIFY, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		"socket(%p) timeout: %s",
		client.sock.get(),
		e.what()
	};

	return false;
}

/// Unknown/untreated error. Probably not worth attempting a clean shutdown
/// so a hard / immediate disconnect given instead.
bool
ircd::handle_ec_default(client &client,
                        const error_code &ec)
{
	log::dwarning
	{
		"socket(%p) local[%s] remote[%s] %s",
		client.sock.get(),
		string(local(client)),
		string(remote(client)),
		string(ec)
	};

	client.close(net::dc::RST, net::close_ignore);
	return false;
}

//
// client
//

ircd::client::client(std::shared_ptr<socket> sock)
:instance_multimap
{
	remote_ipport(*sock)
}
,head_buffer
{
	conf->header_max_size
}
,sock
{
	std::move(sock)
}
{
	assert(size(head_buffer) >= 8_KiB);
}

ircd::client::~client()
noexcept try
{
	//assert(!sock || !connected(*sock));
}
catch(const std::exception &e)
{
	log::critical
	{
		"socket(%p) ~client(%p): %s",
		sock.get(),
		this,
		e.what()
	};

	return;
}

/// Client main loop.
///
/// Before main(), the client had been sitting in async mode waiting for
/// socket activity. Once activity with data was detected indicating a request,
/// the client was dispatched to the request pool where it is paired to an
/// ircd::ctx with a stack. main() is then invoked on that ircd::ctx stack.
/// Nothing from the socket has been read into userspace before main().
///
/// This function parses requests off the socket in a loop until there are no
/// more requests or there is a fatal error. The ctx will "block" to wait for
/// more data off the socket during the middle of a request until the request
/// timeout is reached. main() will not "block" to wait for more data after a
/// request; it will simply `return true` which puts this client back into
/// async mode and relinquishes this stack. returning false will disconnect
/// the client rather than putting it back into async mode.
///
/// Normal exceptions do not pass below main() therefor anything unhandled is an
/// internal server error and the client is disconnected. The exception handler
/// here though is executing on a request ctx stack, and we can choose to take
/// advantage of that; in contrast to the handle_ec() switch which handles
/// errors on the main/callback stack and must be asynchronous.
///
bool
ircd::client::main()
try
{
	parse::buffer pb{head_buffer};
	parse::capstan pc{pb, read_closure(*this)}; do
	{
		if(!handle_request(pc))
			return false;

		// After the request, the head and content has been read off the socket
		// and the capstan has advanced to the end of the content. The catch is
		// that reading off the socket could have read too much, bleeding into
		// the next request. This is rare, but pb.remove() will memmove() the
		// bleed back to the beginning of the head buffer for the next loop.
		pb.remove();
	}
	while(pc.unparsed());

	return true;
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;
	using boost::system::system_category;
	using boost::asio::error::get_ssl_category;
	using boost::asio::error::get_misc_category;

	log::derror
	{
		"socket(%p) local[%s] remote[%s] error during request: %s",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		string(e.code())
	};

	const error_code &ec{e.code()};
	const int &value{ec.value()};
	if(ec.category() == system_category()) switch(value)
	{
		case success:
			assert(0);
			return true;

		case broken_pipe:
		case connection_reset:
		case not_connected:
			close(net::dc::RST, net::close_ignore);
			return false;

		case operation_canceled:
		case timed_out:
			return false;

		case bad_file_descriptor:
			return false;

		default:
			break;
	}
	else if(ec.category() == get_ssl_category()) switch(uint8_t(value))
	{
		case SSL_R_SHORT_READ:
			close(net::dc::RST, net::close_ignore);
			return false;

		case SSL_R_PROTOCOL_IS_SHUTDOWN:
			close(net::dc::RST, net::close_ignore);
			return false;

		default:
			break;
	}
	else if(ec.category() == get_misc_category()) switch(value)
	{
		case boost::asio::error::eof:
			return false;

		default:
			break;
	}

	log::error
	{
		"socket(%p) (unexpected) %s: (%d) %s",
		sock.get(),
		ec.category().name(),
		value,
		ec.message()
	};

	close(net::dc::RST, net::close_ignore);
	return false;
}
catch(const ctx::interrupted &e)
{
	log::warning
	{
		"socket(%p) local[%s] remote[%s] Request interrupted: %s",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		e.what()
	};

	close(net::dc::SSL_NOTIFY, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		"socket(%p) local[%s] remote[%s] %s",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		e.what()
	};

	return false;
}
catch(const ctx::terminated &)
{
	close(net::dc::RST, net::close_ignore);
	throw;
}

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
ircd::client::handle_request(parse::capstan &pc)
try
{
	net::scope_timeout timeout
	{
		*sock, conf->request_timeout
	};

	// This is the first read off the wire. The headers are entirely read and
	// the tape is advanced.
	timer = ircd::timer{};
	const http::request::head head{pc};
	head_length = pc.parsed - data(head_buffer);
	content_consumed = std::min(pc.unparsed(), head.content_length);
	pc.parsed += content_consumed;
	assert(pc.parsed <= pc.read);

	// The resource being sought will have its own specific timeout, or none
	// at all. This timeout is now canceled to not conflict. Note that the
	// time spent so far is still being accumulated by client.timer.
	timeout.cancel();

	log::debug
	{
		"socket(%p) local[%s] remote[%s] HTTP %s `%s' content-length:%zu have:%zu",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		head.method,
		head.path,
		head.content_length,
		content_consumed
	};

	bool ret
	{
		resource_request(head)
	};

	if(ret && iequals(head.connection, "close"_sv))
		ret = false;

	return ret;
}
catch(const boost::system::system_error &e)
{
	if(e.code().value() != boost::system::errc::operation_canceled)
		throw;

	if(!sock || sock->fini)
		return false;

	const ctx::exception_handler eh;
	resource::response
	{
		*this, http::REQUEST_TIMEOUT, {}, 0L, {}
	};

	return false;
}
catch(const std::exception &e)
{
	if(!sock || sock->fini)
		return false;

	log::error
	{
		"socket(%p) local[%s] remote[%s] HTTP 500 Internal Error: %s",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		e.what()
	};

	const ctx::exception_handler eh;
	resource::response
	{
		*this, e.what(), "text/html; charset=utf8", http::INTERNAL_SERVER_ERROR
	};

	return false;
}

bool
ircd::client::resource_request(const http::request::head &head)
try
{
	const string_view content_partial
	{
		data(head_buffer) + head_length, content_consumed
	};

	auto &resource
	{
		ircd::resource::find(head.path)
	};

	resource(*this, head, content_partial);
	discard_unconsumed(head);
	return true;
}
catch(const http::error &e)
{
	const ctx::exception_handler eh;

	log::derror
	{
		"socket(%p) local[%s] remote[%s] HTTP %u %s `%s' :%s",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		uint(e.code),
		http::status(e.code),
		head.uri,
		e.content
	};

	resource::response
	{
		*this, e.content, "text/html; charset=utf8", e.code, e.headers
	};

	switch(e.code)
	{
		// These codes are "unrecoverable" errors and no more HTTP can be
		// conducted with this tape. The client must be disconnected.
		case http::BAD_REQUEST:
		case http::REQUEST_TIMEOUT:
		case http::PAYLOAD_TOO_LARGE:
		case http::INTERNAL_SERVER_ERROR:
			return false;

		// These codes are "recoverable" and allow the next HTTP request in
		// a pipeline to take place.
		default:
			discard_unconsumed(head);
			return true;
	}
}

void
ircd::client::discard_unconsumed(const http::request::head &head)
{
	if(unlikely(!sock))
		return;

	const size_t unconsumed
	{
		head.content_length - content_consumed
	};

	if(!unconsumed)
		return;

	log::debug
	{
		"socket(%p) local[%s] remote[%s] discarding %zu unconsumed of %zu bytes content...",
		sock.get(),
		string(local(*this)),
		string(remote(*this)),
		unconsumed,
		head.content_length
	};

	content_consumed += net::discard_all(*sock, unconsumed);
	assert(content_consumed == head.content_length);
}

ircd::ctx::future<void>
ircd::client::close(const net::close_opts &opts)
{
	if(likely(sock) && !sock->fini)
		return net::close(*sock, opts);
	else
		return ctx::future<void>::already;
}

void
ircd::client::close(const net::close_opts &opts,
                    net::close_callback callback)
{
	if(!sock)
		return;

	if(sock->fini)
		return callback({});

	net::close(*sock, opts, std::move(callback));
}

size_t
ircd::client::write_all(const const_buffer &buf)
{
	if(unlikely(!sock))
		throw error{"No socket to client."};

	return net::write_all(*sock, buf);
}
