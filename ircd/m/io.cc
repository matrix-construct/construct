/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

namespace ircd::m::io
{
	bool acquire_local(event::fetch &);
	bool acquire_local(room::fetch &);
	bool acquire_local(room::state::fetch &);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/io.h
//

struct ircd::m::io::fetch::opts
const ircd::m::io::fetch::defaults
{};

struct ircd::m::io::sync::opts
const ircd::m::io::sync::defaults
{};

ircd::json::object
ircd::m::io::get(const id::event &event_id,
                 const mutable_buffer &buf)
{
	event::fetch tab
	{
		event_id, buf
	};

	return acquire(tab);
}

void
ircd::m::io::release(event::sync &tab)
{
	release({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);
}

ircd::json::array
ircd::m::io::acquire(room::state::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

ircd::json::array
ircd::m::io::acquire(room::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

ircd::json::object
ircd::m::io::acquire(event::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdu;
}

//
// mass release suite
//

//TODO: XXX
namespace ircd::m::io
{
	net::remote
	destination_remote(const string_view &destination)
	{
		return net::remote{destination};
	}
}

size_t
ircd::m::io::release(vector_view<event::sync> tab)
{
	const auto count
	{
		tab.size()
	};

	size_t out(0);
	std::string url[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].destination || !tab[i].buf)
			continue;

		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/send/%zu/", tab[i].txnid
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : destination_remote(tab[i].destination)
		};

		request[i] =
		{
			"PUT", url[i], {}, json::object{tab[i].buf}
		};

		request[i].destination = tab[i].destination;
		request[i](session[i].server);
		++out;
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("sync txn %ld will not succeed: %s",
		            tab[i].txnid,
		            e.what());
	}

	static const size_t rbuf_size{4_KiB};
	const unique_buffer<mutable_buffer> rbuf
	{
		out * rbuf_size
	};

	size_t in(0), ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].destination)
			continue;

		if(bool(tab[i].error))
			continue;

		const mutable_buffer buf
		{
			data(rbuf) + (in * rbuf_size), rbuf_size
		};

		ircd::parse::buffer pb{buf};
		response[i] = m::io::response{session[i].server, pb};
		ret += !tab[i].error;
		++in;

		log.debug("%s received txn %ld (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          tab[i].txnid,
		          size(tab[i].buf),
		          bool(tab[i].error));
	}
	catch(const http::error &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.error("request to sync txn %ld failed: %s: %s",
		          tab[i].txnid,
		          e.what(),
		          e.content);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.error("request to sync txn %ld failed: %s",
		          tab[i].txnid,
		          e.what());
	}

	return ret;
}

//
// mass acquire suite
//

size_t
ircd::m::io::acquire(vector_view<room::state::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/state/%s/", url::encode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "event_id=%s", url::encode(tab[i].event_id, tmp)
		};

		request[i] =
		{
			"GET", url[i], query[i], json::object{}
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

size_t
ircd::m::io::acquire(vector_view<room::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/backfill/%s/", url::encode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "limit=%zu&v=%s", tab[i].opts->limit, url::encode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], query[i], {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

size_t
ircd::m::io::acquire(vector_view<event::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/event/%s/", url::encode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], {}, {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response{session[i].server, pb};
		tab[i].pdu = json::array{response[i]["pdus"]}[0];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

//
// acquire_local suite.
//

bool
ircd::m::io::acquire_local(room::state::fetch &tab)
try
{
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}

bool
ircd::m::io::acquire_local(room::fetch &tab)
try
{
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}

bool
ircd::m::io::acquire_local(event::fetch &tab)
try
{
/*
	const m::vm::query<m::vm::where::equal> query
	{
		{ "event_id", tab.event_id },
	};

	const auto test{[&tab](const auto &event)
	{
		tab.pdu = stringify(mutable_buffer{tab.buf}, event);
		return true;
	}};

	return m::vm::test(query, test);
*/
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//
// m/session.h
//

ircd::m::io::session::session(const net::remote &remote)
:server{remote}
,destination{remote.hostname}
{
}

ircd::json::object
ircd::m::io::session::operator()(parse::buffer &pb,
                                 request &request)
{
	request.destination = destination;
	request(server);
	return response
	{
		server, pb
	};
}

ircd::m::io::response::response(server &server,
                                parse::buffer &pb)
{
	http::code status;
	json::object &object
	{
		static_cast<json::object &>(*this)
	};

	parse::capstan pc
	{
		pb, read_closure(server)
	};

	http::response
	{
		pc,
		nullptr,
		[&pc, &status, &object](const http::response::head &head)
		{
			status = http::status(head.status);
			object = http::response::content{pc, head};
		},
		[](const auto &header)
		{
			//std::cout << header.first << " :" << header.second << std::endl;
		}
	};

	if(status < 200 || status >= 300)
		throw m::error(status, object);
}

void
ircd::m::io::request::operator()(const vector_view<const http::header> &addl_headers)
const
{
}

void
ircd::m::io::request::operator()(server &server,
                                 const vector_view<const http::header> &addl_headers)
const
{
	const size_t addl_headers_size
	{
		std::min(addl_headers.size(), size_t(64UL))
	};

	size_t headers{2 + addl_headers_size};
	http::line::header header[headers + 1]
	{
		{ "User-Agent",    BRANDING_NAME " (IRCd " BRANDING_VERSION ")" },
		{ "Content-Type",  "application/json"                           },
	};

	for(size_t i(0); i < addl_headers_size; ++i)
		header[headers++] = addl_headers.at(i);

	char x_matrix[1024];
	if(startswith(path, "_matrix/federation"))
		header[headers++] =
		{
			"Authorization",  generate_authorization(x_matrix)
		};

	http::request
	{
		destination,
		method,
		path,
		query,
		content,
		write_closure(server),
		{ header, headers }
	};
}

namespace ircd::m::name
{
//	constexpr const char *const content {"content"};
	constexpr const char *const destination {"destination"};
	constexpr const char *const method {"method"};
//	constexpr const char *const origin {"origin"};
	constexpr const char *const uri {"uri"};
}

struct ircd::m::io::request::authorization
:json::tuple
<
	json::property<name::content, string_view>,
	json::property<name::destination, string_view>,
	json::property<name::method, string_view>,
	json::property<name::origin, string_view>,
	json::property<name::uri, string_view>
>
{
	string_view generate(const mutable_buffer &out);

	using super_type::tuple;
};

ircd::string_view
ircd::m::io::request::generate_authorization(const mutable_buffer &out)
const
{
	const fmt::bsprintf<2048> uri
	{
		"/%s%s%s", lstrip(path, '/'), query? "?" : "", query
	};

	request::authorization authorization
	{
		json::members
		{
			{ "destination",  destination  },
			{ "method",       method       },
			{ "origin",       my_host()    },
			{ "uri",          uri          },
		}
	};

	if(string_view{content}.size() > 2)
		json::get<"content"_>(authorization) = content;

	return authorization.generate(out);
}

ircd::string_view
ircd::m::io::request::authorization::generate(const mutable_buffer &out)
{
	// Any buffers here can be comfortably large if they're not on a stack and
	// nothing in this procedure has a yield which risks decohering static
	// buffers; the assertion is tripped if so.
	ctx::critical_assertion ca;

	static fixed_buffer<mutable_buffer, 131072> request_object_buf;
	const auto request_object
	{
		json::stringify(request_object_buf, *this)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(request_object)
	};

	static fixed_buffer<mutable_buffer, 128> signature_buf;
	const auto x_matrix_len
	{
		fmt::sprintf(out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\"",
		             unquote(string_view{at<"origin"_>(*this)}),
		             self::public_key_id,
		             b64encode_unpadded(signature_buf, sig))
	};

	return
	{
		data(out), size_t(x_matrix_len)
	};
}

bool
ircd::m::io::verify_x_matrix_authorization(const string_view &x_matrix,
                                           const string_view &method,
                                           const string_view &uri,
                                           const string_view &content)
{
	string_view tokens[3], origin, key, sig;
	if(ircd::tokens(split(x_matrix, ' ').second, ',', tokens) != 3)
		return false;

	for(const auto &token : tokens)
	{
		const auto &kv{split(token, '=')};
		const auto &val{unquote(kv.second)};
		switch(hash(kv.first))
		{
			case hash("origin"):  origin = val;  break;
			case hash("key"):     key = val;     break;
			case hash("sig"):     sig = val;     break;
		}
	}

	request::authorization authorization
	{
		json::members
		{
			{ "destination",  my_host() },
			{ "method",       method    },
			{ "origin",       origin    },
			{ "uri",          uri       },
		}
	};

	if(content.size() > 2)
		json::get<"content"_>(authorization) = content;

	//TODO: XXX
	const json::strung request_object
	{
		authorization
	};

	const ed25519::sig _sig
	{
		[&sig](auto &buf)
		{
			b64decode(buf, sig);
		}
	};

	const ed25519::pk pk
	{
		[&origin, &key](auto &buf)
		{
			m::keys::get(origin, key, [&buf]
			(const string_view &key)
			{
				b64decode(buf, unquote(key));
			});
		}
	};

	return pk.verify(const_raw_buffer{request_object}, _sig);
}
