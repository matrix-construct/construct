/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

///////////////////////////////////////////////////////////////////////////////
//
// m/vm.h
//

namespace ircd::m::vm
{
}

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"vm", 'v'
};

decltype(ircd::m::vm::fronts)
ircd::m::vm::fronts
{
};

decltype(ircd::m::vm::pipe)
ircd::m::vm::pipe
{
};

void
ircd::m::vm::trace(const id::event &event_id,
                   const tracer &closure)
{
	std::multimap<uint64_t, room::branch> tree;

	room::branch root{event_id};
	event::fetch tab{root.event_id, root.buf};
	io::acquire(tab);
	root.pdu = tab.pdu;
	const event event{root.pdu};
	const auto &depth{at<"depth"_>(event)};
	tree.emplace(depth, std::move(root));

	for(int64_t d(depth); d > 0; --d)
	{
		const auto pit(tree.equal_range(d));
		if(pit.first == pit.second)
			if(tree.lower_bound(d - 1) == end(tree))
				break;

		for(auto it(pit.first); it != pit.second; ++it)
		{
			room::branch &b(it->second);
			const m::event event{b.pdu};
			const json::array &prev_events
			{
				json::get<"prev_events"_>(event)
			};

			const auto count
			{
				prev_events.count()
			};

			room::branch child[count];
			event::fetch tab[count];
			for(size_t i(0); i < count; ++i)
			{
				const json::array &prev_event{prev_events[i]};
				child[i] = room::branch { unquote(prev_event[0]) };
				tab[i] = { child[i].event_id, child[i].buf };
			}

			io::acquire({tab, count});

			for(size_t i(0); i < count; ++i)
			{
				child[i].pdu = tab[i].pdu;
				if(tab[i].error)
					continue;

				event::id::buf tmp;
				const m::event event{child[i].pdu};
				if(!closure(event, tmp))
					return;

				const auto &depth{at<"depth"_>(event)};
				tree.emplace(depth, std::move(child[i]));
			}
		}
	}

	for(const auto &pit : tree)
	{
		const auto &depth(pit.first);
		const auto &branch(pit.second);
		std::cout << pretty_oneline(m::event{branch.pdu}) << std::endl;
	}
}

void
ircd::m::vm::statefill(const room::id &room_id,
                       const event::id &event_id)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		32_MiB //TODO: XXX
	};

	room::state::fetch tab
	{
		event_id, room_id, buf
	};

	io::acquire(tab);

	const json::array &auth_chain
	{
		tab.auth_chain
	};

	const json::array &pdus
	{
		tab.pdus
	};

	std::vector<m::event> events(pdus.count());
	std::transform(begin(pdus), end(pdus), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	eval{events};
}
catch(const std::exception &e)
{
	log.error("Acquiring state for %s at %s: %s",
	          string_view{room_id},
	          string_view{event_id},
	          e.what());
	throw;
}

void
ircd::m::vm::backfill(const room::id &room_id,
                      const event::id &event_id,
                      const size_t &limit)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		32_MiB //TODO: XXX
	};

	room::fetch tab
	{
		event_id, room_id, buf
	};

	io::acquire(tab);

	const json::array &auth_chain
	{
		tab.auth_chain
	};

	const json::array &pdus
	{
		tab.pdus
	};

	std::vector<m::event> events(pdus.count());
	std::transform(begin(pdus), end(pdus), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	eval{events};
}
catch(const std::exception &e)
{
	log.error("Acquiring backfill for %s at %s: %s",
	          string_view{room_id},
	          string_view{event_id},
	          e.what());
	throw;
}

ircd::json::object
ircd::m::vm::acquire(const id::event &event_id,
                     const mutable_buffer &buf)
{
	const vector_view<const id::event> event_ids(&event_id, &event_id + 1);
	const vector_view<const mutable_buffer> bufs(&buf, &buf + 1);
	return acquire(event_ids, bufs)? json::object{buf} : json::object{};
}

size_t
ircd::m::vm::acquire(const vector_view<const id::event> &event_id,
                     const vector_view<const mutable_buffer> &buf)
{
	std::vector<event::fetch> tabs(event_id.size());
	for(size_t i(0); i < event_id.size(); ++i)
		if(!exists(event_id[i]))
			tabs[i] = event::fetch { event_id[i], buf[i] };

	size_t i(0);
	io::acquire(vector_view<event::fetch>(tabs));
	for(const auto &fetch : tabs)
		if(fetch.pdu)
		{
			i++;
			eval{m::event{fetch.pdu}};
		}

	return i;
}

/// Federation join user to room
///
ircd::m::event::id::buf
ircd::m::vm::join(const room::id &room_id,
                  json::iov &iov)
{
	const user::id user_id
	{
		iov.at("sender")
	};

	const auto &hostname{room_id.hostname()};
	const auto &hostport{room_id.hostport()};
	m::log.debug("%s make_join %s to %s from %s:%u",
	             my_host(),
	             string_view{user_id},
	             string_view{room_id},
	             hostname,
	             hostport);

	char room_id_urle_buf[768];
	const auto room_id_urle
	{
		urlencode(room_id, room_id_urle_buf),
	};

	char user_id_urle_buf[768];
	const auto user_id_urle
	{
		urlencode(user_id, user_id_urle_buf)
	};

	const fmt::snstringf url
	{
		1024, "_matrix/federation/v1/make_join/%s/%s", room_id_urle, user_id_urle
	};

	m::request request
	{
		"GET", url, {}, {}
	};

	struct session session
	{
		{ std::string(hostname), hostport }
	};

	unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	ircd::parse::buffer pb{buf};
	const json::object response
	{
		session(pb, request)
	};

	const m::event proto
	{
		response.at("event")
	};

	//TODO: hash prototype
	//at<"hashes"_>(proto)

	//TODO: verify prototype
	//at<"signatures"_>(proto)

	m::log.debug("%s make_join %s to %s responded. depth: %ld prev: %s auth: %s",
	             room_id.host(),
	             string_view{user_id},
	             string_view{room_id},
	             json::get<"depth"_>(proto),
	             json::get<"prev_events"_>(proto),
	             json::get<"auth_events"_>(proto));

	const json::strung content
	{
		json::members {{ "membership", "join" }}
	};

	json::iov event;
	const json::iov::push push[]
	{
		{ event, { "type",              "m.room.member"              }},
		{ event, { "membership",        "join"                       }},
		{ event, { "room_id",           room_id                      }},
		{ event, { "origin",            my_host()                    }},
		{ event, { "sender",            user_id                      }},
		{ event, { "state_key",         user_id                      }},
		{ event, { "origin_server_ts",  ircd::time<milliseconds>()   }},
		{ event, { "depth",             at<"depth"_>(proto)          }},
		{ event, { "content",           content                      }},
	//	{ event, { "auth_events",       at<"auth_events"_>(proto)    }},
	//	{ event, { "prev_events",       at<"prev_events"_>(proto)    }},
	//	{ event, { "prev_state",        at<"prev_state"_>(proto)     }},
	};

	//TODO: XXX
	auto replaced_auth_events{replace(at<"auth_events"_>(proto), '\\', "")};
	auto replaced_prev_events{replace(at<"prev_events"_>(proto), '\\', "")};
	auto replaced_prev_state{replace(at<"prev_state"_>(proto), '\\', "")};
	const json::iov::push replacements[]
	{
		{ event, { "auth_events", replaced_auth_events }},
		{ event, { "prev_events", replaced_prev_events }},
		{ event, { "prev_state",  replaced_prev_state  }},
	};

	const json::strung hash_preimage
	{
		event
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_buffer{hash_preimage}}
	};

	char hashb64[uint(hash.size() * 1.34) + 2];
	const json::strung hashes
	{
		json::members
		{
			{ "sha256", b64encode_unpadded(hashb64, hash) }
		}
	};

	const json::strung event_id_preimage
	{
		event
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> event_id_hash
	{
		sha256{const_buffer{event_id_preimage}}
	};

	char event_id_hash_b64[uint(event_id_hash.size() * 1.34) + 2];
	const event::id::buf event_id_buf
	{
		b64encode_unpadded(event_id_hash_b64, event_id_hash), my_host()
	};

	const json::iov::push _event_id
	{
		event, { "event_id", event_id_buf }
	};

	const json::strung signature_preimage
	{
		event
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(const_buffer{signature_preimage})
	};

	char signature_buffer[128];
	const json::strung signatures{json::members
	{{
		my_host(), json::members
		{
			json::member { self::public_key_id, b64encode_unpadded(signature_buffer, sig) }
		}
	}}};

	const json::iov::push _signatures
	{
		event, { "signatures", signatures }
	};

	char event_id_urle_buf[768];
	const auto event_id_urle
	{
		urlencode(event.at("event_id"), event_id_urle_buf)
	};

	const fmt::bsprintf<1024> send_join_url
	{
		"_matrix/federation/v1/send_join/%s/%s", room_id_urle, event_id_urle
	};

	const auto join_event
	{
		json::strung(event)
	};

	m::log.debug("%s send_join %s to %s sending: %s membership: %s %s",
	             my_host(),
	             string_view{user_id},
	             string_view{room_id},
	             string_view{event.at("type")},
	             string_view{event.at("membership")},
	             string_view{event.at("event_id")});

	m::request send_join_request
	{
		"PUT", send_join_url, {}, join_event
	};

	unique_buffer<mutable_buffer> send_join_buf
	{
		4_MiB
	};

	ircd::parse::buffer sjpb{send_join_buf};
	const json::array send_join_response
	{
		session(sjpb, send_join_request)
	};

	const auto status
	{
		send_join_response.at<uint>(0)
	};

	const json::object data
	{
		send_join_response.at(1)
	};

	const json::array state
	{
		data.at("state")
	};

	const json::array auth_chain
	{
		data.at("auth_chain")
	};

	m::log.debug("%s %u send_join %s to %s responded with %zu state and %zu auth_chain events",
	             room_id.host(),
	             status,
	             string_view{user_id},
	             string_view{room_id},
	             state.count(),
	             auth_chain.count());

	eval{state};
	eval{event};
	return event_id_buf;
}

/// Insert a new event originating from this server.
///
/// Figure 1:
///          in    .
///    ___:::::::__V  <-- this function
///    |  ||||||| //
///    |   \\|// //|
///    |    ||| // |
///    |    ||//   |
///    |    !!!    |
///    |     *     |   <----- IRCd core
///    | |//|||\\| |
///    |/|/|/|\|\|\|    <---- release commitment propagation cone
///         out
///
/// This function takes an event object vector and adds our origin and event_id
/// and hashes and signature and attempts to inject the event into the core.
/// The caller is expected to have done their best to check if this event will
/// succeed once it hits the core because failures blow all this effort. The
/// caller's ircd::ctx will obviously yield for evaluation, which may involve
/// requests over the internet in the worst case. Nevertheless, the evaluation,
/// write and release sequence of the core commitment is designed to allow the
/// caller to service a usual HTTP request conveying success or error without
/// hanging their client too much.
///
ircd::m::event::id::buf
ircd::m::vm::commit(json::iov &iov)
{
	const room::id &room_id
	{
		iov.at("room_id")
	};

	std::string prev_events; try
	{
		auto &front(fronts.get(room_id, iov));
		if(!front.map.empty())
		{
			std::string prev_id
			{
				front.map.begin()->first
			};

			const json::value prev[]
			{
				string_view{prev_id}
			};

			const json::value prev2[]
			{
				{ prev, 1 }
			};

			const json::value prevs
			{
				prev2, 1
			};

			prev_events = json::strung(prevs);
		}
	}
	catch(const std::exception &e)
	{

	}

	std::string auth_events; try
	{


	}
	catch(const std::exception &e)
	{

	}

	const json::iov::set set[]
	{
		{ iov, { "origin_server_ts",  ircd::time<milliseconds>() }},
		{ iov, { "origin",            my_host()                  }},
		{ iov, { "prev_events",       prev_events                }},
		{ iov, { "auth_events",       auth_events                }},
	};

	// Need this for now
	const unique_buffer<mutable_buffer> scratch
	{
		64_KiB
	};

	// derp
	auto preimage
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> event_id_hash
	{
		sha256{const_buffer{preimage}}
	};

	char event_id_hash_b64[uint(event_id_hash.size() * 1.34) + 2];
	const event::id::buf event_id_buf
	{
		b64encode_unpadded(event_id_hash_b64, event_id_hash), my_host()
	};

	const json::iov::set event_id
	{
		iov, { "event_id", event_id_buf }
	};

	// derp
	preimage =
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_buffer{preimage}}
	};

	fixed_buffer<mutable_buffer, uint(hash.size() * 1.34) + 1> hashb64;
	const json::iov::set hashes
	{
		iov, json::member
		{
			"hashes", json::members
			{
				{ "sha256", b64encode_unpadded(hashb64, hash) }
			}
		}
	};

	// derp
	preimage =
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(const_buffer{preimage})
	};

	char signature_buffer[128];
	const json::iov::set signatures
	{
		iov, json::member
		{
			"signatures", json::members
			{{
				my_host(), json::members
				{
					json::member { self::public_key_id, b64encode_unpadded(signature_buffer, sig) }
				}
			}}
		}
	};

	const m::event event
	{
		iov
	};

	if(!json::get<"type"_>(event))
		throw BAD_JSON("Required event field: type");

	if(!json::get<"sender"_>(event))
		throw BAD_JSON("Required event field: sender");

	log.debug("injecting event(mark: %ld) %s",
	          vm::current_sequence,
	          pretty_oneline(event));

	ircd::timer timer;

	eval{event};

	log.debug("committed event %s (mark: %ld time: %ld$ms)",
	          at<"event_id"_>(event),
	          vm::current_sequence,
	          timer.at<milliseconds>().count());

	return event_id_buf;
}

//
// Eval
//
// Processes any event from any place from any time and does whatever is
// necessary to validate, reject, learn from new information, ignore old
// information and advance the state of IRCd as best as possible.

namespace ircd::m::vm
{
	bool check_fault_resume(eval &);
	void check_fault_throw(eval &);

	void write(const event &, db::iov &txn);
	void write(eval &);

	int _query(eval &, const query<> &, const closure_bool &);
	int _query_where(eval &, const query<where::equal> &where, const closure_bool &closure);
	int _query_where(eval &, const query<where::logical_and> &where, const closure_bool &closure);

	bool evaluate(eval &, const vector_view<port> &, const size_t &i);
	enum fault evaluate(eval &, const event &);
	enum fault evaluate(eval &, const vector_view<const event> &);
}

decltype(ircd::m::vm::eval::default_opts)
ircd::m::vm::eval::default_opts
{};

ircd::m::vm::eval::eval()
:eval(default_opts)
{
}

ircd::m::vm::eval::eval(const struct opts &opts)
:opts{&opts}
{
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()()
{
	assert(0);
	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const json::array &events)
{
	std::vector<m::event> event;
	event.reserve(events.count());

	auto it(begin(events));
	for(; it != end(events); ++it)
		event.emplace_back(*it);

	return operator()(vector_view<const m::event>(event));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const json::vector &events)
{
	std::vector<m::event> event;
	event.reserve(events.count());

	auto it(begin(events));
	for(; it != end(events); ++it)
		event.emplace_back(*it);

	return operator()(vector_view<const m::event>(event));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
{
	return operator()(vector_view<const m::event>(&event, 1));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const vector_view<const event> &events)
{
	static const size_t max
	{
		1024
	};

	for(size_t i(0); i < events.size(); i += max)
	{
		const vector_view<const event> evs
		{
			events.data() + i, std::min(events.size() - i, size_t(max))
		};

		enum fault code;
		switch((code = evaluate(*this, evs)))
		{
			case fault::ACCEPT:
				continue;

			default:
				//check_fault_throw(*this);
				return code;
		}
	}

	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::evaluate(eval &eval,
                      const vector_view<const event> &events)
{
	const std::unique_ptr<port[]> port_buf
	{
		new port[events.size()]
	};

	const vector_view<port> p
	{
		port_buf.get(), events.size()
	};

	auto &ef{eval.ef};
	size_t a{0}, b{0}, i{0};
	for(size_t i(0); i < events.size(); ++i)
		p[i].event = events.data() + i;

	for(; b < events.size() && a < events.size(); ++b, ++i, i %= events.size())
	{
		if(p[i].h)
			continue;

		if((p[i].h = evaluate(eval, p, i)))
		{
			if(p[i].w)
			{
				write(*p[i].event, eval.txn);
				++eval.cs;
			}

			b = 0;
			++a;
		}
	}

	const auto cs{eval.cs};
	if(cs)
	{
		write(eval);
		for(size_t i(0); i < p.size(); ++i)
		{
			if(p[i].event && p[i].w)
			{
				log.info("%s", pretty_oneline(*p[i].event));
				vm::inserted.notify(*p[i].event);
				p[i] = port{};
			}
		}
	}

	return cs == events.size()? fault::ACCEPT:
	                            fault::EVENT;
}

bool
ircd::m::vm::evaluate(eval &eval,
                      const vector_view<port> &pv,
                      const size_t &i)
try
{
	auto &ef{eval.ef};
	auto &p{pv[i]};
	if(!p.event)
		return true;

	auto &event{*p.event};
	switch((p.code = evaluate(eval, event)))
	{
		case fault::ACCEPT:
		{
			p.w = true;
			ef.erase(at<"event_id"_>(event));
			return true;
		}

		case fault::EVENT:
		{
			const auto it(std::find_if(begin(pv), end(pv), [&ef]
			(const auto &port) -> bool
			{
				return port.event? ef.count(at<"event_id"_>(*port.event)) : false;
			}));

			return it == end(pv);
		}

		case fault::STATE:
			return true;

		default:
			return true;
	}
}
catch(const std::exception &e)
{
	log.error("%s", e.what());
	return true;
}

enum ircd::m::vm::fault
ircd::m::vm::evaluate(eval &eval,
                      const event &event)
{
	const auto &event_id
	{
		at<"event_id"_>(event)
	};

	const auto &depth
	{
		at<"depth"_>(event)
	};

	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	auto &front
	{
		fronts.get(room_id, event)
	};

	front.top = depth;

	auto code
	{
		fault::ACCEPT
	};

	const m::event::prev prev{event};
	m::for_each(prev, [&eval, &code](const event::id &event_id)
	{
		const query<where::equal> q
		{
			{ "event_id", event_id }
		};

		if(eval.capstan.test(q) != true)
		{
			//eval.ef.emplace(event_id);
			//code = fault::EVENT;
		}
	});

	log.debug("%s %s",
	          reflect(code),
	          pretty_oneline(event));

	if(code == fault::ACCEPT)
		eval.capstan.fwd(event);

	return code;
}

void
ircd::m::vm::write(eval &eval)
{
	log.debug("Committing %zu events to database...",
	          eval.cs);

	eval.txn();
	vm::current_sequence += eval.cs;
	eval.txn.clear();
	eval.cs = 0;
}

/*
bool
ircd::m::vm::check_fault_resume(eval &eval)
{
	switch(fault)
	{
		case fault::ACCEPT:
			return true;

		default:
			check_fault_throw(eval);
			return false;
    }
}

void
ircd::m::vm::check_fault_throw(eval &eval)
{
	// With nothrow there is nothing else for this function to do as the user
	// will have to test the fault code and error ptr manually.
	if(eval.opts->nothrow)
		return;

	switch(fault)
	{
		case fault::ACCEPT:
			return;

		case fault::GENERAL:
			if(eval.error)
				std::rethrow_exception(eval.error);
			// [[fallthrough]]

		default:
			throw VM_FAULT("(%u): %s", uint(fault), reflect(fault));
    }
}
*/
int
ircd::m::vm::test(eval &eval,
                  const query<> &where)
{
	return test(eval, where, [](const auto &event)
	{
		return true;
	});
}

int
ircd::m::vm::test(eval &eval,
                  const query<> &clause,
                  const closure_bool &closure)
{
	return _query(eval, clause, [&closure](const auto &event)
	{
		return closure(event);
 	});
}

int
ircd::m::vm::_query(eval &eval,
                    const query<> &clause,
                    const closure_bool &closure)
{
	switch(clause.type)
	{
		case where::equal:
		{
			const auto &c
			{
				dynamic_cast<const query<where::equal> &>(clause)
			};

			return _query_where(eval, c, closure);
		}

		case where::logical_and:
		{
			const auto &c
			{
				dynamic_cast<const query<where::logical_and> &>(clause)
			};

			return _query_where(eval, c, closure);
		}

		default:
			return -1;
	}
}

int
ircd::m::vm::_query_where(eval &eval,
                          const query<where::equal> &where,
                          const closure_bool &closure)
{
	const int ret
	{
		eval.capstan.test(where)
	};

	log.debug("eval(%p): Query [%s]: %s -> %d",
	          &eval,
	          "where equal",
	          pretty_oneline(where.value),
	          ret);

	return ret;
}

int
ircd::m::vm::_query_where(eval &eval,
                          const query<where::logical_and> &where,
                          const closure_bool &closure)
{
	const auto &lhs{*where.a}, &rhs{*where.b};
	const auto reclosure{[&lhs, &rhs, &closure]
	(const auto &event)
	{
		if(!rhs(event))
			return false;

		return closure(event);
	}};

	return _query(eval, lhs, reclosure);
}

ircd::ctx::view<const ircd::m::event>
ircd::m::vm::inserted
{};

uint64_t
ircd::m::vm::current_sequence
{};

template<>
std::list<ircd::m::vm::witness *>
ircd::m::vm::witness::instance_list<ircd::m::vm::witness>::list
{
};

ircd::m::vm::capstan::capstan()
:acc(witness::list.size())
{
	size_t i(0);
	const auto &list{vm::witness::list};
	for(auto it(begin(list)); it != end(list); ++it, i++)
	{
		auto &witness{**it};
		this->acc[i] = witness.init();
	}
}

ircd::m::vm::capstan::~capstan()
noexcept
{
}

void
ircd::m::vm::capstan::fwd(const event &event)
{
	size_t i(0);
	const auto &list{vm::witness::list};
	for(auto it(begin(list)); it != end(list); ++it, i++)
	{
		auto &witness{**it};
		const auto &acc{this->acc[i]};
		witness.add(acc.get(), event);
	}
}

void
ircd::m::vm::capstan::rev(const event &event)
{
	size_t i(0);
	const auto &list{vm::witness::list};
	for(auto it(begin(list)); it != end(list); ++it, i++)
	{
		auto &witness{**it};
		const auto &acc{this->acc[i]};
		witness.add(acc.get(), event);
	}
}

int
ircd::m::vm::capstan::test(const query<> &q)
const
{
	size_t i(0);
	const auto &list{vm::witness::list};
	for(auto it(begin(list)); it != end(list); ++it, i++)
	{
		auto &witness{**it};
		const auto &acc{this->acc[i]};
		const auto res{witness.test(acc.get(), q)};
		if(res >= 0)
			return res;
	}

	return -1;
}

ssize_t
ircd::m::vm::capstan::count(const query<> &q)
const
{
	size_t i(0);
	const auto &list{vm::witness::list};
	for(auto it(begin(list)); it != end(list); ++it, i++)
	{
		auto &witness{**it};
		const auto &acc{this->acc[i]};
		const auto res{witness.count(acc.get(), q)};
		if(res >= 0)
			return res;
	}

	return -1;
}

ircd::m::vm::accumulator::~accumulator()
noexcept
{
}

ircd::m::vm::witness::witness(std::string name)
:name{std::move(name)}
{
}

ircd::m::vm::witness::~witness()
noexcept
{
}

ircd::m::vm::front &
ircd::m::vm::fronts::get(const room::id &room_id,
                         const event &event)
try
{
	front &ret
	{
		map[std::string(room_id)]
	};

	if(ret.map.empty())
		fetch(room_id, ret, event);

	return ret;
}
catch(const std::exception &e)
{
	map.erase(std::string(room_id));
	throw;
}

ircd::m::vm::front &
ircd::m::vm::fronts::get(const room::id &room_id)
{
	const auto it
	{
		map.find(std::string(room_id))
	};

	if(it == end(map))
		throw m::NOT_FOUND
		{
			"No fronts for unknown room %s", room_id
		};

	front &ret{it->second};
	if(unlikely(ret.map.empty()))
		throw m::NOT_FOUND
		{
			"No fronts for room %s", room_id
		};

	return ret;
}

ircd::m::vm::front &
ircd::m::vm::fetch(const room::id &room_id,
                   front &front,
                   const event &event)
{
	const query<where::equal> query
	{
		{ "room_id", room_id },
	};

	for_each(query, [&front]
	(const auto &event)
	{
		for_each(m::event::prev{event}, [&front, &event]
		(const auto &key, const auto &prev_events)
		{
			for(const json::array &prev_event : prev_events)
			{
				const event::id &prev_event_id{unquote(prev_event[0])};
				front.map.erase(std::string{prev_event_id});
			}

			const auto &depth
			{
				json::get<"depth"_>(event)
			};

			if(depth > front.top)
				front.top = depth;

			front.map.emplace(at<"event_id"_>(event), depth);
		});
	});

	if(!front.map.empty())
		return front;

	const event::id &event_id
	{
		at<"event_id"_>(event)
	};

	if(!my_host(room_id.host()))
	{
		log.debug("No fronts available for %s; acquiring state eigenvalue at %s...",
		          string_view{room_id},
		          string_view{event_id});

		if(event_id.host() == "matrix.org" && room_id.host() == "matrix.org")
			statefill(room_id, event_id);

		return front;
	}

	log.debug("No fronts available for %s using %s",
	          string_view{room_id},
	          string_view{event_id});

	front.map.emplace(at<"event_id"_>(event), json::get<"depth"_>(event));
	front.top = json::get<"depth"_>(event);

	if(!my_host(room_id.host()))
	{
		backfill(room_id, event_id, 64);
		return front;
	}

	return front;
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "ACCEPT";
		case fault::GENERAL:      return "GENERAL";
		case fault::DEBUGSTEP:    return "DEBUGSTEP";
		case fault::BREAKPOINT:   return "BREAKPOINT";
		case fault::EVENT:        return "EVENT";
	}

	return "??????";
}

//
// Write
//

namespace ircd::m
{
	void append_indexes(const event &, db::iov &);
}

void
ircd::m::vm::write(const event &event,
                   db::iov &txn)
{
	db::iov::append
	{
		txn, at<"event_id"_>(event), event
	};

	append_indexes(event, txn);
}

//
// Query
//

namespace ircd::m::vm
{
	bool _query(const query<> &, const closure_bool &);

	bool _query_event_id(const query<> &, const closure_bool &);
	bool _query_in_room_id(const query<> &, const closure_bool &, const room::id &);
	bool _query_for_type_state_key_in_room_id(const query<> &, const closure_bool &, const room::id &, const string_view &type = {}, const string_view &state_key = {});

	int _query_where_event_id(const query<where::equal> &, const closure_bool &);
	int _query_where_room_id_at_event_id(const query<where::equal> &, const closure_bool &);
	int _query_where_room_id(const query<where::equal> &, const closure_bool &);
	int _query_where(const query<where::equal> &where, const closure_bool &closure);
	int _query_where(const query<where::logical_and> &where, const closure_bool &closure);

	const query<> noop;
}

ircd::m::vm::query<>::~query()
noexcept
{
}

bool
ircd::m::vm::exists(const event::id &event_id)
{
	db::column column
	{
		*event::events, "event_id"
	};

	return has(column, event_id);
}

bool
ircd::m::vm::test(const query<> &where)
{
	return test(where, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::vm::test(const query<> &clause,
                  const closure_bool &closure)
{
	bool ret{false};
	_query(clause, [&ret, &closure](const auto &event)
	{
		ret = closure(event);
		return true;
 	});

	return ret;
}

size_t
ircd::m::vm::count(const query<> &where)
{
	return count(where, [](const auto &event)
	{
		return true;
	});
}

size_t
ircd::m::vm::count(const query<> &where,
                       const closure_bool &closure)
{
	size_t i(0);
	for_each(where, [&closure, &i](const auto &event)
	{
		i += closure(event);
	});

	return i;
}

/*
void
ircd::m::vm::rfor_each(const closure &closure)
{
	const query<where::noop> where{};
	rfor_each(where, closure);
}

void
ircd::m::vm::rfor_each(const query<> &where,
                           const closure &closure)
{
	rquery(where, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

bool
ircd::m::vm::rquery(const closure_bool &closure)
{
	const query<where::noop> where{};
	return rquery(where, closure);
}

bool
ircd::m::vm::rquery(const query<> &where,
                        const closure_bool &closure)
{
	//return _rquery_(where, closure);
	return true;
}
*/

void
ircd::m::vm::for_each(const closure &closure)
{
	const query<where::noop> where{};
	for_each(where, closure);
}

void
ircd::m::vm::for_each(const query<> &clause,
                      const closure &closure)
{
	_query(clause, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

bool
ircd::m::vm::_query(const query<> &clause,
                    const closure_bool &closure)
{
	switch(clause.type)
	{
		case where::equal:
		{
			const auto &c
			{
				dynamic_cast<const query<where::equal> &>(clause)
			};

			switch(_query_where(c, closure))
			{
				case 0:   return false;
				case 1:   return true;
				default:  break;
			}

			break;
		}

		case where::logical_and:
		{
			const auto &c
			{
				dynamic_cast<const query<where::logical_and> &>(clause)
			};

			switch(_query_where(c, closure))
			{
				case 0:   return false;
				case 1:   return true;
				default:  break;
			}

			break;
		}

		default:
		{
			return _query_event_id(clause, closure);
		}
	}

	return _query_event_id(clause, closure);
}

int
ircd::m::vm::_query_where(const query<where::equal> &where,
                          const closure_bool &closure)
{
	log.debug("Query [%s]: %s",
	          "where equal",
	          pretty_oneline(where.value));

	const auto &value{where.value};
	const auto &room_id{json::get<"room_id"_>(value)};
	if(room_id)
		return _query_where_room_id(where, closure);

	const auto &event_id{json::get<"event_id"_>(value)};
	if(event_id)
		return _query_where_event_id(where, closure);

	return -1;
}

int
ircd::m::vm::_query_where(const query<where::logical_and> &where,
                          const closure_bool &closure)
{
	const auto &lhs{*where.a}, &rhs{*where.b};
	const auto reclosure{[&lhs, &rhs, &closure]
	(const auto &event)
	{
		if(!rhs(event))
			return false;

		return closure(event);
	}};

	return _query(lhs, reclosure);
}

int
ircd::m::vm::_query_where_event_id(const query<where::equal> &where,
                                   const closure_bool &closure)
{
	const event::id &event_id
	{
		at<"event_id"_>(where.value)
	};

	if(my(event_id))
	{
		std::cout << "GET LOCAL? " << event_id << std::endl;
		if(_query_event_id(where, closure))
			return true;
	}

	std::cout << "GET REMOTE? " << event_id << std::endl;
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	event::fetch tab
	{
		event_id, buf
	};

	const m::event event
	{
		io::acquire(tab)
	};

	return closure(event);
}

int
ircd::m::vm::_query_where_room_id_at_event_id(const query<where::equal> &where,
                                              const closure_bool &closure)
{
	std::cout << "where room id at event id?" << std::endl;
	const auto &value{where.value};
	const auto &room_id{json::get<"room_id"_>(value)};
	const auto &event_id{json::get<"event_id"_>(value)};
	const auto &state_key{json::get<"state_key"_>(value)};

	auto &front{fronts.get(room_id, value)};

	if(!defined(state_key))
		return _query_where_event_id(where, closure);

	if(my(room_id))
	{
		std::cout << "GET LOCAL STATE? " << event_id << std::endl;
		return -1;
	}

	//std::cout << "GET REMOTE STATE? " << event_id << std::endl;
	//vm::statefill(room_id, event_id);
	//auto &front{fronts.get(room_id)};

	const auto &type{json::get<"type"_>(value)};
	if(type && state_key)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	return _query_in_room_id(where, closure, room_id);
}

int
ircd::m::vm::_query_where_room_id(const query<where::equal> &where,
                                  const closure_bool &closure)
{
	const auto &value{where.value};
	const auto &room_id{json::get<"room_id"_>(value)};
	const auto &event_id{json::get<"event_id"_>(value)};
	if(event_id)
		return _query_where_room_id_at_event_id(where, closure);

	//std::cout << "where room id?" << std::endl;
	//auto &front{fronts.get(room_id, value)};
	//std::cout << "where room id front..." << std::endl;

	const auto &type{json::get<"type"_>(value)};
	const auto &state_key{json::get<"state_key"_>(value)};
	const bool is_state{defined(state_key) == true};
	if(type && is_state)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	if(is_state)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	//std::cout << "in room id?" << std::endl;
	return _query_in_room_id(where, closure, room_id);
}

bool
ircd::m::vm::_query_event_id(const query<> &query,
                             const closure_bool &closure)
{
	cursor cursor
	{
		"event_id", &query
	};

	for(auto it(cursor.begin()); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::vm::_query_in_room_id(const query<> &query,
                                   const closure_bool &closure,
                                   const room::id &room_id)
{
	cursor cursor
	{
		"event_id in room_id", &query
	};

	for(auto it(cursor.begin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::vm::_query_for_type_state_key_in_room_id(const query<> &query,
                                                      const closure_bool &closure,
                                                      const room::id &room_id,
                                                      const string_view &type,
                                                      const string_view &state_key)
{
	cursor cursor
	{
		"event_id for type,state_key in room_id", &query
	};

	static const size_t max_type_size
	{
		255
	};

	static const size_t max_state_key_size
	{
		255
	};

	const auto key_max
	{
		room::id::buf::SIZE + max_type_size + max_state_key_size + 2
	};

	size_t key_len;
	char key[key_max]; key[0] = '\0';
	key_len = strlcat(key, room_id, sizeof(key));
	key_len = strlcat(key, "..", sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, type, sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, state_key, sizeof(key)); //TODO: prefix protocol
	for(auto it(cursor.begin(key)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

namespace ircd::m
{
	struct indexer;

	extern std::set<std::shared_ptr<indexer>> indexers;
}

struct ircd::m::indexer
{
	struct concat;
	struct concat_v;
	struct concat_2v;

	std::string name;

	virtual void operator()(const event &, db::iov &iov) const = 0;

	indexer(std::string name)
	:name{std::move(name)}
	{}

	virtual ~indexer() noexcept;
};

ircd::m::indexer::~indexer()
noexcept
{
}

void
ircd::m::append_indexes(const event &event,
                        db::iov &iov)
{
	for(const auto &ptr : indexers)
	{
		const m::indexer &indexer{*ptr};
		indexer(event, iov);
	}
}


struct ircd::m::indexer::concat
:indexer
{
	std::string col_a;
	std::string col_b;

	void operator()(const event &, db::iov &) const override;

	concat(std::string col_a, std::string col_b)
	:indexer
	{
		fmt::snstringf(512, "%s in %s", col_a, col_b)
	}
	,col_a{col_a}
	,col_b{col_b}
	{}
};

void
ircd::m::indexer::concat::operator()(const event &event,
                                     db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_a) || !iov.has(db::op::SET, col_b))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto function
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_b, function);
	at(event, col_a, function);

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			{},          // val
		}
	};
}

struct ircd::m::indexer::concat_v
:indexer
{
	std::string col_a;
	std::string col_b;
	std::string col_c;

	void operator()(const event &, db::iov &) const override;

	concat_v(std::string col_a, std::string col_b, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s in %s", col_a, col_b, col_c)
	}
	,col_a{col_a}
	,col_b{col_b}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_v::operator()(const event &event,
                                       db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_c) || !iov.has(db::op::SET, col_b) || !iov.has(db::op::SET, col_a))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	at(event, col_b, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

struct ircd::m::indexer::concat_2v
:indexer
{
	std::string col_a;
	std::string col_b0;
	std::string col_b1;
	std::string col_c;

	void operator()(const event &, db::iov &) const override;

	concat_2v(std::string col_a, std::string col_b0, std::string col_b1, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s,%s in %s", col_a, col_b0, col_b1, col_c)
	}
	,col_a{col_a}
	,col_b0{col_b0}
	,col_b1{col_b1}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_2v::operator()(const event &event,
                                        db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_c) || !iov.has(db::op::SET, col_b0) || !iov.has(db::op::SET, col_b1))
		return;

	static const size_t buf_max
	{
		2048
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	strlcat(index, "..", buf_max);  //TODO: special
	at(event, col_b0, concat);
	at(event, col_b1, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

std::set<std::shared_ptr<ircd::m::indexer>> ircd::m::indexers
{{
	std::make_shared<ircd::m::indexer::concat>("event_id", "sender"),
	std::make_shared<ircd::m::indexer::concat>("event_id", "room_id"),
	std::make_shared<ircd::m::indexer::concat_v>("event_id", "room_id", "type"),
	std::make_shared<ircd::m::indexer::concat_v>("event_id", "room_id", "sender"),
	std::make_shared<ircd::m::indexer::concat_2v>("event_id", "type", "state_key", "room_id"),
}};

ircd::string_view
ircd::m::vm::reflect(const where &w)
{
	switch(w)
	{
		case where::noop:           return "noop";
		case where::test:           return "test";
		case where::equal:          return "equal";
		case where::not_equal:      return "not_equal";
		case where::logical_or:     return "logical_or";
		case where::logical_and:    return "logical_and";
		case where::logical_not:    return "logical_not";
	}

	return "?????";
}
