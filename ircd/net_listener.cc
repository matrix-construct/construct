// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Option to indicate if any listener sockets should be allowed to bind. If
/// false then no listeners should bind. This is only effective on startup
/// unless a conf item updated function is implemented here.
decltype(ircd::net::listen)
ircd::net::listen
{
	{ "name",     "ircd.net.listen" },
	{ "default",  true              },
	{ "persist",  false             },
};

bool
ircd::net::stop(acceptor &a)
{
	a.close();
	return true;
}

bool
ircd::net::start(acceptor &a)
{
	if(!a.a.is_open())
		a.open();

	allow(a);
	return true;
}

bool
ircd::net::allow(acceptor &a)
{
	if(unlikely(!a.a.is_open()))
		return false;

	if(a.accepting > 0)
		return false;

	a.set_handle();
	return true;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const acceptor &a)
{
	s << loghead(a);
	return s;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const listener &a)
{
	s << *a.acceptor;
	return s;
}

ircd::string_view
ircd::net::loghead(const acceptor &a)
{
	thread_local char buf[512];
	return loghead(buf, a);
}

ircd::string_view
ircd::net::loghead(const mutable_buffer &out,
                   const acceptor &a)
{
	char addrbuf[128];
	return fmt::sprintf
	{
		out, "[%s] @ [%s]:%u",
		name(a),
		string(addrbuf, a.ep.address()),
		a.ep.port(),
	};
}

size_t
ircd::net::accepting_count(const acceptor &a)
{
	return a.accepting;
}

size_t
ircd::net::handshaking_count(const acceptor &a)
{
	return a.handshaking.size();
}

size_t
ircd::net::handshaking_count(const acceptor &a,
                             const ipaddr &ipaddr)
{
	return std::count_if(begin(a.handshaking), end(a.handshaking), [&ipaddr]
	(const auto &socket_p)
	{
		return remote_ipport(*socket_p) == ipaddr;
	});
}

ircd::net::ipport
ircd::net::local(const acceptor &a)
{
	return make_ipport(a.a.local_endpoint());
}

ircd::net::ipport
ircd::net::binder(const acceptor &a)
{
	return make_ipport(a.ep);
}

ircd::string_view
ircd::net::name(const acceptor &a)
{
	return a.name;
}

ircd::json::object
ircd::net::config(const acceptor &a)
{
	return a.opts;
}

std::string
ircd::net::cipher_list(const acceptor &a)
{
	auto &ssl(mutable_cast(a).ssl);
	return openssl::cipher_list(*ssl.native_handle());
}

//
// listener::listener
//

ircd::net::listener::listener(const string_view &name,
                              const std::string &opts,
                              callback cb,
                              proffer pcb)
:listener
{
	name,
	json::object{opts},
	std::move(cb),
	std::move(pcb)
}
{
}

ircd::net::listener::listener(const string_view &name,
                              const json::object &opts,
                              callback cb,
                              proffer pcb)
:acceptor
{
	std::make_shared<struct acceptor>
	(
		*this,
		name,
		opts,
		std::move(cb),
		std::move(pcb)
	)
}
{
}

/// Cancels all pending accepts and handshakes and waits (yields ircd::ctx)
/// until report.
///
ircd::net::listener::~listener()
noexcept
{
	if(acceptor)
		acceptor->close();
}

ircd::string_view
ircd::net::listener::name()
const
{
	const net::acceptor &a(*this);
	return net::name(a);
}

ircd::net::listener::operator
ircd::json::object()
const
{
	const net::acceptor &a(*this);
	return net::config(a);
}

ircd::net::listener::operator
net::acceptor &()
{
	assert(acceptor);
	return *acceptor;
}

ircd::net::listener::operator
const net::acceptor &()
const
{
	assert(acceptor);
	return *acceptor;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/acceptor.h
//

decltype(ircd::net::acceptor::log)
ircd::net::acceptor::log
{
	"net.listen"
};

decltype(ircd::net::acceptor::accept_desc)
ircd::net::acceptor::accept_desc
{
	"ircd.net.acceptor.accept"
};

decltype(ircd::net::acceptor::handshake_desc)
ircd::net::acceptor::handshake_desc
{
	"ircd.net.acceptor.handshake"
};

decltype(ircd::net::acceptor::timeout)
ircd::net::acceptor::timeout
{
	{ "name",     "ircd.net.acceptor.timeout" },
	{ "default",  12000L                      },
};

/// The number of simultaneous handshakes we conduct across all clients.
decltype(ircd::net::acceptor::handshaking_max)
ircd::net::acceptor::handshaking_max
{
	{ "name",     "ircd.net.acceptor.handshaking.max" },
	{ "default",  1024L                               },
};

/// The number of simultaneous handshakes we conduct for a single peer (which
/// is an IP without a port in this context). This prevents a peer from
/// reaching the handshaking.max limit to DoS out other peers.
decltype(ircd::net::acceptor::handshaking_max_per_peer)
ircd::net::acceptor::handshaking_max_per_peer
{
	{ "name",     "ircd.net.acceptor.handshaking.max_per_peer" },
	{ "default",  16L                                          },
};

decltype(ircd::net::acceptor::ssl_curve_list)
ircd::net::acceptor::ssl_curve_list
{
	{ "name",     "ircd.net.acceptor.ssl.curve.list"     },
	{ "default",  string_view{ircd::net::ssl_curve_list} },
};

decltype(ircd::net::acceptor::ssl_cipher_list)
ircd::net::acceptor::ssl_cipher_list
{
	{ "name",     "ircd.net.acceptor.ssl.cipher.list"     },
	{ "default",  string_view{ircd::net::ssl_cipher_list} },
};

decltype(ircd::net::acceptor::ssl_cipher_blacklist)
ircd::net::acceptor::ssl_cipher_blacklist
{
	{ "name",     "ircd.net.acceptor.ssl.cipher.blacklist"     },
	{ "default",  string_view{ircd::net::ssl_cipher_blacklist} },
};

//
// acceptor::acceptor
//

ircd::net::acceptor::acceptor(net::listener &listener,
                              const string_view &name,
                              const json::object &opts,
                              listener::callback cb,
                              listener::proffer pcb)
try
:listener_
{
	&listener
}
,name
{
	name
}
,opts
{
	opts
}
,backlog
{
	//TODO: XXX
	//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
	std::min(opts.get<uint>("backlog", SOMAXCONN), uint(SOMAXCONN))
}
,cb
{
	std::move(cb)
}
,pcb
{
	pcb?
		std::move(pcb):
		proffer_default
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,ep
{
	make_address(unquote(opts.get("host", "*"_sv))),
	opts.at<uint16_t>("port")
}
,a
{
	ios::get()
}
{
	configure(opts);

	log::debug
	{
		log, "%s configured listener SSL",
		loghead(*this)
	};

	open();
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

ircd::net::acceptor::~acceptor()
noexcept
{
	if(accepting || !handshaking.empty())
		log::critical
		{
			"The acceptor must not have clients during destruction!"
			" (accepting:%zu handshaking:%zu)",
			accepting,
			handshaking.size(),
		};
}

void
ircd::net::acceptor::open()
{
	static const auto &max_connections
	{
		//TODO: XXX
		//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
		std::min(json::object(opts).get<uint>("max_connections", SOMAXCONN), uint(SOMAXCONN))
	};

	static const ip::tcp::acceptor::reuse_address reuse_address
	{
		true
	};

	assert(!interrupting);
	interrupting = false;
	a.open(ep.protocol());
	a.set_option(reuse_address);
	a.non_blocking(true);
	log::debug
	{
		log, "%s opened listener socket",
		loghead(*this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket",
		loghead(*this)
	};

	a.listen(backlog);
	log::debug
	{
		log, "%s listening (backlog: %lu, max connections: %zu)",
		loghead(*this),
		backlog,
		max_connections
	};
}

void
ircd::net::acceptor::close()
{
	if(!interrupting)
		interrupt();

	if(a.is_open())
		a.close();

	for(const auto &sock : handshaking)
		sock->cancel();

	join();
	log::debug
	{
		log, "%s listener finished",
		loghead(*this)
	};
}

void
ircd::net::acceptor::join()
noexcept try
{
	if(!interrupting)
		interrupt();

	if(!ctx::current)
		return;

	joining.wait([this]
	{
		return !accepting && handshaking.empty();
	});

	interrupting = false;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "acceptor(%p) join :%s",
		this,
		e.what()
	};
}

bool
ircd::net::acceptor::interrupt()
noexcept try
{
	if(interrupting)
		return false;

	if(!a.is_open())
		return false;

	interrupting = true;
	a.cancel();
	return true;
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "acceptor(%p) interrupt :%s",
		this,
		string(e)
	};

	return false;
}

/// Sets the next asynchronous handler to start the next accept sequence.
/// Each call to next() sets one handler which handles the connect for one
/// socket. After the connect, an asynchronous SSL handshake handler is set
/// for the socket.
bool
ircd::net::acceptor::set_handle()
try
{
	const auto &sock
	{
		std::make_shared<ircd::socket>(ssl)
	};

	auto handler
	{
		std::bind(&acceptor::accept, this, ph::_1, sock)
	};

	ip::tcp::socket &sd(*sock);
	a.async_accept(sd, ios::handle(accept_desc, std::move(handler)));
	++accepting;
	return true;
}
catch(const std::exception &e)
{
	throw panic
	{
		"%s :%s",
		loghead(*this),
		e.what()
	};
}

/// Callback for a socket connected. This handler then invokes the
/// asynchronous SSL handshake sequence.
///
void
ircd::net::acceptor::accept(const error_code &ec,
                            const std::shared_ptr<socket> sock)
noexcept try
{
	assert(bool(sock));
	assert(accepting > 0);
	assert(accepting == 1); // for now
	char ecbuf[64];
	log::debug
	{
		log, "%s %s accepted(%zu) %s",
		loghead(*sock),
		loghead(*this),
		accepting,
		string(ecbuf, ec)
	};

	--accepting;
	if(unlikely(!check_accept_error(ec, *sock)))
	{
		allow(*this);
		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	const auto &remote
	{
		remote_ipport(*sock)
	};

	if(unlikely(!check_handshake_limit(*sock, remote)))
	{
		allow(*this);
		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	// Call the proffer-callback. This allows the application to check whether
	// to allow or deny this remote before the handshake, as well as setting
	// the next accept to shape the kernel's queue.
	if(!pcb(*listener_, remote))
	{
		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	static const socket::handshake_type handshake_type
	{
		socket::handshake_type::server
	};

	const auto it
	{
		handshaking.emplace(end(handshaking), sock)
	};

	auto handshake
	{
		std::bind(&acceptor::handshake, this, ph::_1, sock, it)
	};

	sock->set_timeout(milliseconds(timeout));
	sock->ssl.async_handshake(handshake_type, ios::handle(handshake_desc, std::move(handshake)));
	assert(!openssl::get_app_data(*sock));
	openssl::set_app_data(*sock, sock.get());
}
catch(const ctx::interrupted &e)
{
	assert(bool(sock));

	char ecbuf[64];
	log::debug
	{
		log, "%s acceptor interrupted %s :%s",
		loghead(*this),
		loghead(*sock),
		string(ecbuf, ec)
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
	joining.notify_all();
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s acceptor error in accept() %s :%s",
		loghead(*this),
		loghead(*sock),
		e.what()
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
	joining.notify_all();
}

/// Error handler for the accept socket callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::acceptor::check_accept_error(const error_code &ec,
                                        socket &sock)
const
{
	using std::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(!ec))
		return true;

	if(system_category(ec)) switch(ec.value())
	{
		case int(errc::operation_canceled):
			throw ctx::interrupted();

		default:
			break;
	}

	char ecbuf[64];
	log::derror
	{
		log, "%s in accept %s :%s",
		loghead(*this),
		loghead(sock),
		string(ecbuf, ec),
	};

	return false;
}

/// Checks performed for whether handshaking limits have been reached before
/// allowing a handshake.
bool
ircd::net::acceptor::check_handshake_limit(socket &sock,
                                           const ipport &remote)
const
{
	if(unlikely(handshaking_count(*this) >= size_t(handshaking_max)))
	{
		log::warning
		{
			log, "%s refusing to handshake %s; exceeds maximum of %zu handshakes.",
			loghead(sock),
			loghead(*this),
			size_t(handshaking_max),
		};

		return false;
	}

	if(unlikely(handshaking_count(*this, remote) >= size_t(handshaking_max_per_peer)))
	{
		log::dwarning
		{
			log, "%s refusing to handshake %s; exceeds maximum of %zu handshakes to them.",
			loghead(sock),
			loghead(*this),
			size_t(handshaking_max_per_peer),
		};

		return false;
	}

	return true;
}

/// Default proffer callback which accepts this connection and allows the
/// next accept to take place as well. This is generally overriden by a
/// user callback to control this behavior.
bool
ircd::net::acceptor::proffer_default(listener &listener,
                                     const ipport &ipport)
{
	allow(listener);
	return true;
}

void
ircd::net::acceptor::handshake(const error_code &ec,
                               const std::shared_ptr<socket> sock,
                               const decltype(handshaking)::const_iterator it)
noexcept try
{
	assert(bool(sock));
	assert(!handshaking.empty());
	assert(it != end(handshaking));
	assert(openssl::get_app_data(*sock) == sock.get());

	#ifdef RB_DEBUG
	const auto *const current_cipher
	{
		!ec?
			openssl::current_cipher(*sock):
			nullptr
	};

	char ecbuf[64];
	log::debug
	{
		log, "%s %s handshook(%zd:%zu) cipher:%s %s",
		loghead(*sock),
		loghead(*this),
		std::distance(cbegin(handshaking), it),
		handshaking.size(),
		current_cipher?
			openssl::name(*current_cipher):
			"<NO CIPHER>"_sv,
		string(ecbuf, ec)
	};
	#endif

	handshaking.erase(it);
	openssl::set_app_data(*sock, nullptr);
	check_handshake_error(ec, *sock);
	sock->cancel_timeout();
	assert(bool(cb));

	// Toggles the behavior of non-async functions; see func comment
	blocking(*sock, false);
	cb(*listener_, sock);
}
catch(const ctx::interrupted &e)
{
	assert(bool(sock));
	char ecbuf[64];
	log::debug
	{
		log, "%s SSL handshake interrupted %s %s",
		loghead(*sock),
		loghead(*this),
		string(ecbuf, ec)
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}
catch(const std::system_error &e)
{
	assert(bool(sock));
	log::derror
	{
		log, "%s %s in handshake() :%s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s %s in handshake() :%s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}

/// Error handler for the SSL handshake callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
void
ircd::net::acceptor::check_handshake_error(const error_code &ec,
                                           socket &sock)
const
{
	using std::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(system_category(ec))) switch(ec.value())
	{
		case 0:
			return;

		case int(errc::operation_canceled):
			if(sock.timedout)
				throw_system_error(errc::timed_out);
			else
				break;

		default:
			break;
	}

	throw_system_error(ec);
	__builtin_unreachable();
}

ircd::string_view
ircd::net::acceptor::handle_alpn(socket &socket,
                                 const vector_view<const string_view> &in)
{
	if(empty(in))
		return {};

	log::debug
	{
		log, "%s %s offered %zu ALPN protocols",
		loghead(socket),
		loghead(*this),
		size(in),
	};

	#ifdef IRCD_NET_ACCEPTOR_DEBUG_ALPN
	for(size_t i(0); i < size(in); ++i)
	{
		log::debug
		{
			log, "%s ALPN protocol %zu of %zu: '%s'",
			loghead(socket),
			i,
			size(in),
			in[i],
		};
	}
	#endif IRCD_NET_ACCEPTOR_DEBUG_ALPN

	//NOTE: proto == "h2" condition goes here
	for(const auto &proto : in)
		if(proto == "http/1.1")
		{
			strlcpy(socket.alpn, proto);
			return proto;
		}

	return {};
}

static int
ircd_net_acceptor_handle_alpn(SSL *const s,
                              const unsigned char **out,
                              unsigned char *const outlen,
                              const unsigned char *const in,
                              unsigned int inlen,
                              void *const arg)
noexcept try
{
	static const size_t PROTOS_MAX
	{
		8
	};

	auto &acceptor
	{
		*reinterpret_cast<ircd::net::acceptor *>(arg)
	};

	size_t p(0), i(0);
	ircd::string_view protos[PROTOS_MAX];
	while(i < inlen && p < PROTOS_MAX)
	{
		const uint8_t &len(in[i++]);
		if(unlikely(!len || i + len >= inlen))
			break;

		protos[p++] = ircd::string_view
		{
			reinterpret_cast<const char *>(in + i), len
		};

		i += len;
	}

	const ircd::vector_view<const ircd::string_view> vec
	{
		protos, p
	};

	assert(s);
	assert(ircd::openssl::get_app_data(*s));
	if(unlikely(!ircd::openssl::get_app_data(*s)))
		return SSL_TLSEXT_ERR_ALERT_FATAL;

	auto &socket
	{
		*static_cast<ircd::net::socket *>(ircd::openssl::get_app_data(*s))
	};

	const ircd::string_view sel
	{
		acceptor.handle_alpn(socket, vec)
	};

	if(!sel)
		return SSL_TLSEXT_ERR_NOACK;

	*out = reinterpret_cast<const unsigned char *>(data(sel));
	*outlen = size(sel);
	return SSL_TLSEXT_ERR_OK;
}
catch(const std::exception &)
{
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}
catch(...)
{
	ircd::log::critical
	{
		ircd::net::acceptor::log,
		"Acceptor ALPN callback unhandled."
	};

	ircd::terminate();
	__builtin_unreachable();
}

bool
ircd::net::acceptor::handle_sni(socket &socket,
                                int &client_server)
try
{
	const string_view &name
	{
		openssl::server_name(socket)
	};

	if(!name)
		return true;

	const string_view accept[]
	{
		this->cname,
	};

	const bool accepts
	{
		std::find(begin(accept), end(accept), name) != end(accept)
	};

	if(!accepts)
	{
		log::dwarning
		{
			log, "%s %s unrecognized SNI '%s' offered.",
			loghead(socket),
			loghead(*this),
			name,
		};

		return false;
	}

	log::debug
	{
		log, "%s %s offered SNI '%s'",
		loghead(socket),
		loghead(*this),
		name,
	};

	return true;
}
catch(const sni_warning &e)
{
	log::warning
	{
		log, "%s %s during SNI :%s",
		loghead(socket),
		loghead(*this),
		e.what(),
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s %s during SNI :%s",
		loghead(socket),
		loghead(*this),
		e.what(),
	};

	throw;
}

static int
ircd_net_acceptor_handle_sni(SSL *const s,
                             int *const i,
                             void *const a)
noexcept try
{
	if(unlikely(!s || !i || !a))
		throw ircd::panic
		{
			"Missing arguments to callback s:%p i:%p a:%p", s, i, a
		};

	auto &acceptor
	{
		*reinterpret_cast<ircd::net::acceptor *>(a)
	};

	assert(s);
	assert(ircd::openssl::get_app_data(*s));
	if(unlikely(!ircd::openssl::get_app_data(*s)))
		return SSL_TLSEXT_ERR_ALERT_FATAL;

	auto &socket
	{
		*static_cast<ircd::net::socket *>(ircd::openssl::get_app_data(*s))
	};

	return acceptor.handle_sni(socket, *i)?
		SSL_TLSEXT_ERR_OK:
		SSL_TLSEXT_ERR_NOACK;
}
catch(const ircd::net::acceptor::sni_warning &)
{
	return SSL_TLSEXT_ERR_ALERT_WARNING;
}
catch(const std::exception &)
{
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}
catch(...)
{
	ircd::log::critical
	{
		ircd::net::acceptor::log,
		"Acceptor SNI callback unhandled."
	};

	ircd::terminate();
	__builtin_unreachable();
}

void
ircd::net::acceptor::configure(const json::object &opts)
{
	log::debug
	{
		log, "%s preparing listener socket configuration...",
		loghead(*this)
	};

	configure_password(opts);
	configure_flags(opts);
	configure_ciphers(opts);
	configure_curves(opts);
	configure_certs(opts);

	SSL_CTX_set_alpn_select_cb(ssl.native_handle(), ircd_net_acceptor_handle_alpn, this);
	SSL_CTX_set_tlsext_servername_callback(ssl.native_handle(), ircd_net_acceptor_handle_sni);
	SSL_CTX_set_tlsext_servername_arg(ssl.native_handle(), this);
}

void
ircd::net::acceptor::configure_flags(const json::object &opts)
{
	ulong flags(0);

	if(opts.get<bool>("ssl_default_workarounds", false))
		flags |= ssl.default_workarounds;

	if(opts.get<bool>("ssl_single_dh_use", false))
		flags |= ssl.single_dh_use;

	if(opts.get<bool>("ssl_no_sslv2", false))
		flags |= ssl.no_sslv2;

	if(opts.get<bool>("ssl_no_sslv3", false))
		flags |= ssl.no_sslv3;

	if(opts.get<bool>("ssl_no_tlsv1", false))
		flags |= ssl.no_tlsv1;

	if(opts.get<bool>("ssl_no_tlsv1_1", false))
		flags |= ssl.no_tlsv1_1;

	if(opts.get<bool>("ssl_no_tlsv1_2", false))
		flags |= ssl.no_tlsv1_2;

	ssl.set_options(flags);
}

void
ircd::net::acceptor::configure_ciphers(const json::object &opts)
{
	if(!empty(unquote(opts["ssl_cipher_list"])))
	{
		const json::string &list
		{
			opts["ssl_cipher_list"]
		};

		assert(ssl.native_handle());
		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_cipher_list)))
	{
		assert(ssl.native_handle());
		const string_view &list
		{
			ssl_cipher_list
		};

		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_cipher_blacklist)))
	{
		assert(ssl.native_handle());

		std::stringstream res;
		const string_view &blacklist
		{
			ssl_cipher_blacklist
		};

		const auto ciphers
		{
			openssl::cipher_list(*ssl.native_handle(), 0)
		};

		ircd::tokens(ciphers, ':', [&res, &blacklist]
		(const string_view &cipher)
		{
			assert(cipher);
			if(!has(blacklist, cipher))
				res << cipher << ':';
		});

		std::string list(res.str());
		assert(list.empty() || list.back() == ':');
		list.pop_back();

		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
}

void
ircd::net::acceptor::configure_curves(const json::object &opts)
{
	if(!empty(unquote(opts["ssl_curve_list"])))
	{
		const json::string &list
		{
			opts["ssl_curve_list"]
		};

		assert(ssl.native_handle());
		openssl::set_curves(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_curve_list)))
	{
		const string_view &list
		{
			ssl_curve_list
		};

		assert(ssl.native_handle());
		openssl::set_curves(*ssl.native_handle(), list);
	}
}

void
ircd::net::acceptor::configure_certs(const json::object &opts)
{
	if(!empty(unquote(opts["certificate_chain_path"])))
	{
		const json::string filename
		{
			opts["certificate_chain_path"]
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL certificate chain file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_certificate_chain_file(filename);
		log::info
		{
			log, "%s using certificate chain file '%s'",
			loghead(*this),
			filename
		};
	}

	if(!empty(unquote(opts["certificate_pem_path"])))
	{
		const std::string filename
		{
			unquote(opts.get("certificate_pem_path", name + ".crt"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL certificate pem file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);

		const auto *const x509
		{
			SSL_CTX_get0_certificate(ssl.native_handle())
		};

		this->cname = ircd::string(rfc3986::DOMAIN_BUFSIZE | SHRINK_TO_FIT, [&x509]
		(const mutable_buffer &buf)
		{
			return x509?
				openssl::subject_common_name(buf, *x509):
				string_view{};
		});

		log::info
		{
			log, "%s using file '%s' with certificate for '%s'",
			loghead(*this),
			filename,
			this->cname,
		};
	}

	if(!empty(unquote(opts["private_key_pem_path"])))
	{
		const std::string filename
		{
			unquote(opts.get("private_key_pem_path", name + ".crt.key"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL private key file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log::info
		{
			log, "%s using private key file '%s'",
			loghead(*this),
			filename
		};
	}
}

void
ircd::net::acceptor::configure_dh(const json::object &opts)
{
	if(!empty(unquote(opts["tmp_dh_path"])))
	{
		const json::string filename
		{
			opts.at("tmp_dh_path")
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL tmp dh file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_tmp_dh_file(filename);
		log::info
		{
			log, "%s using tmp dh file '%s'",
			loghead(*this),
			filename,
		};

		return;
	}

	assert(ssl.native_handle());
	openssl::set_ecdh_auto(*ssl.native_handle(), true);
}

void
ircd::net::acceptor::configure_password(const json::object &opts)
{
	//TODO: XXX
	ssl.set_password_callback([this]
	(const auto &size, const auto &purpose)
	{
		log::notice
		{
			log, "%s asking for password with purpose '%s' (size: %zu)",
			loghead(*this),
			purpose,
			size
		};

		//XXX: TODO
		assert(0);
		return "foobar";
	});
}
