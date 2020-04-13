// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_NETDB_H

namespace ircd::net::dns
{
	static uint16_t _service_port(const string_view &name, const string_view &prot);
	static string_view _service_name(const uint16_t &port, const string_view &prot);

	extern const std::map<pair<string_view>, uint16_t> service_ports;
	extern const std::map<pair<uint16_t, string_view>, string_view> service_names;
}

/// Custom internal database. This translates a service name and protocol
/// into a port number. Note that a query to this table will only be made
/// after the system query does not return results (or cannot be made).
decltype(ircd::net::dns::service_ports)
ircd::net::dns::service_ports
{
	{ { "matrix", "tcp" },  8448 },
};

/// Custom internal database. This translates a service port and protocol
/// into a service name. Note that a query to this table will only be made
/// after the system query does not return results (or cannot be made).
decltype(ircd::net::dns::service_names)
ircd::net::dns::service_names
{
	{ { 8448, "tcp" },  "matrix" },
};

uint16_t
ircd::net::dns::service_port(const string_view &name,
                             const string_view &prot)
{
	const auto ret
	{
		service_port(std::nothrow, name, prot)
	};

	if(unlikely(!ret))
		throw error
		{
			"Port for service %s:%s not found",
			name,
			prot?: "*"_sv,
		};

	return ret;
}

#ifdef HAVE_NETDB_H
uint16_t
ircd::net::dns::service_port(std::nothrow_t,
                             const string_view &name,
                             const string_view &prot)
try
{
	thread_local struct ::servent res, *ent {nullptr};
	thread_local char _name[32], _prot[32], buf[2048];
	const prof::syscall_usage_warning timer
	{
		"net::dns::service_port(%s)", name
	};

	strlcpy(_name, name);
	strlcpy(_prot, prot);
	syscall
	(
		::getservbyname_r,
		_name,
		prot? _prot : nullptr,
		&res,
		buf,
		sizeof(buf),
		&ent
	);

	assert(!ent || ent->s_port != 0);
	assert(!ent || name == ent->s_name);
	assert(!ent || !prot || prot == ent->s_proto);
	if(!ent || !ent->s_port)
		if((res.s_port = _service_port(name, prot)))
			return res.s_port;

	if(unlikely(!ent || !ent->s_port))
		log::error
		{
			log, "Uknown service %s/%s; please add port number to /etc/services",
			name,
			prot?: "*"_sv
		};

	return ent?
		htons(ent->s_port):
		0U;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Failure when translating service %s:%s to port number :%s",
		name,
		prot?: "*"_sv,
		e.what(),
	};

	throw;
}
#else
uint16_t
ircd::net::dns::service_port(std::nothrow_t,
                             const string_view &name,
                             const string_view &prot)
{
	//TODO: XXX
	always_assert(false);
	return 0;
}
#endif

ircd::string_view
ircd::net::dns::service_name(const mutable_buffer &out,
                             const uint16_t &port,
                             const string_view &prot)
{
	const auto ret
	{
		service_name(std::nothrow, out, port, prot)
	};

	if(unlikely(!ret))
		throw error
		{
			"Name of service for port %u:%s not found",
			port,
			prot?: "*"_sv,
		};

	return ret;
}

#ifdef HAVE_NETDB_H
ircd::string_view
ircd::net::dns::service_name(std::nothrow_t,
                             const mutable_buffer &out,
                             const uint16_t &port,
                             const string_view &prot)
try
{


	thread_local struct ::servent res, *ent {nullptr};
	thread_local char _prot[32], buf[2048];
	const prof::syscall_usage_warning timer
	{
		"net::dns::service_name(%u)", port
	};

	strlcpy(_prot, prot);
	syscall
	(
		::getservbyport_r,
		ntohs(port),
		prot? _prot : nullptr,
		&res,
		buf,
		sizeof(buf),
		&ent
	);

	assert(!ent || ent->s_port == ntohs(port));
	assert(!ent || !prot || prot == ent->s_proto);
	return ent?
		strlcpy(out, ent->s_name):
		strlcpy(out, _service_name(port, prot));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Failure when translating port %u:%s to service name :%s",
		port,
		prot?: "*"_sv,
		e.what(),
	};

	throw;
}
#else
ircd::string_view
ircd::net::dns::service_name(std::nothrow_t,
                             const mutable_buffer &out,
                             const uint16_t &port,
                             const string_view &prot)
{
	//TODO: XXX
	always_assert(false);
	return {};
}
#endif

uint16_t
ircd::net::dns::_service_port(const string_view &name,
                              const string_view &prot)
{
	const pair<string_view> query
	{
		name, prot
	};

	const auto it
	{
		service_ports.find(query)
	};

	return it != end(service_ports)?
		it->second:
		0;
}

ircd::string_view
ircd::net::dns::_service_name(const uint16_t &port,
                              const string_view &prot)
{
	const pair<uint16_t, string_view> query
	{
		port, prot
	};

	const auto it
	{
		service_names.find(query)
	};

	return it != end(service_names)?
		it->second:
		string_view{};
}
