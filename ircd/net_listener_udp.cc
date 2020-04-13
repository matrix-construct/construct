// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::ostream &
ircd::net::operator<<(std::ostream &s, const listener_udp &a)
{
	s << *a.acceptor;
	return s;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const acceptor_udp &a)
{
	s << loghead(a);
	return s;
}

ircd::string_view
ircd::net::loghead(const acceptor_udp &a)
{
	thread_local char buf[512];
	return loghead(buf, a);
}

ircd::string_view
ircd::net::loghead(const mutable_buffer &out,
                   const acceptor_udp &a)
{
	thread_local char addrbuf[128];
	return fmt::sprintf
	{
		out, "[%s] @ [%s]:%u",
		a.name,
		string(addrbuf, a.ep.address()),
		a.ep.port(),
	};
}

//
// listener_udp::listener_udp
//

ircd::net::listener_udp::listener_udp(const string_view &name,
                                      const std::string &opts)
:listener_udp
{
	name, json::object{opts}
}
{
}

ircd::net::listener_udp::listener_udp(const string_view &name,
                                      const json::object &opts)
:acceptor
{
	std::make_unique<net::acceptor_udp>(name, opts)
}
{
}

ircd::net::listener_udp::~listener_udp()
noexcept
{
	if(acceptor)
		acceptor->join();
}

ircd::net::listener_udp::datagram &
ircd::net::listener_udp::operator()(datagram &datagram)
{
	assert(acceptor);
	return acceptor->operator()(datagram);
}

ircd::string_view
ircd::net::listener_udp::name()
const
{
	assert(acceptor);
	return acceptor->name;
}

ircd::net::listener_udp::operator
ircd::json::object()
const
{
	assert(acceptor);
	return acceptor->opts;
}

//
// acceptor_udp::acceptor
//

ircd::net::acceptor_udp::acceptor_udp(const string_view &name,
                                      const json::object &opts)
try
:name
{
	name
}
,opts
{
	opts
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
	static const ip::udp::socket::reuse_address reuse_address
	{
		true
	};

	a.open(ep.protocol());
	a.set_option(reuse_address);
	log::debug
	{
		log, "%s opened listener socket", loghead(*this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket", loghead(*this)
	};
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

ircd::net::acceptor_udp::~acceptor_udp()
noexcept
{
}

void
ircd::net::acceptor_udp::join()
noexcept try
{
	interrupt();
	joining.wait([this]
	{
		return waiting == 0;
	});
}
catch(const std::exception &e)
{
	log::error
	{
		log, "acceptor(%p) join :%s", this, e.what()
	};
}

bool
ircd::net::acceptor_udp::interrupt()
noexcept try
{
	a.cancel();
	return true;
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "acceptor(%p) interrupt :%s", this, string(e)
	};

	return false;
}

ircd::net::listener_udp::datagram &
ircd::net::acceptor_udp::operator()(datagram &datagram)
{
	assert(ctx::current);

	const auto flags
	{
		this->flags(datagram.flag)
	};

	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->interrupt();
	}};

	const scope_count waiting
	{
		this->waiting
	};

	ip::udp::endpoint ep;
	size_t rlen; continuation
	{
		continuation::asio_predicate, interruption, [this, &rlen, &datagram, &ep, &flags]
		(auto &yield)
		{
			rlen = a.async_receive_from(datagram.mbufs, ep, flags, yield);
		}
	};

	datagram.remote = make_ipport(ep);
	datagram.mbuf = {data(datagram.mbuf), rlen};
	return datagram;
}

boost::asio::ip::udp::socket::message_flags
ircd::net::acceptor_udp::flags(const flag &flag)
{
	ip::udp::socket::message_flags ret{0};

	if(flag & flag::PEEK)
		ret |= ip::udp::socket::message_peek;

	return ret;
}

//
// listener_udp::datagram
//

ircd::net::listener_udp::datagram::datagram(const const_buffer &buf,
                                            const ipport &remote,
                                            const enum flag &flag)
:remote{remote}
,flag{flag}
{
	cbuf = buf;
	cbufs =
	{
		&this->cbuf, 1
	};
}

ircd::net::listener_udp::datagram::datagram(const mutable_buffer &buf,
                                            const enum flag &flag)
:flag{flag}
{
	mbuf = buf;
	mbufs =
	{
		&this->mbuf, 1
	};
}
