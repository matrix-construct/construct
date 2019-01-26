// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, event);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const event &e)
{
	using prototype = void (std::ostream &, const event &);
	static mods::import<prototype> pretty
	{
		"m_event", "pretty__event"
	};

	pretty(s, e);
	return s;
}

std::string
ircd::m::pretty_oneline(const event &event,
                        const bool &content_keys)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_oneline(s, event, content_keys);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event &e,
                        const bool &content_keys)
{
	using prototype = void (std::ostream &, const event &, const bool &);
	static mods::import<prototype> pretty_oneline
	{
		"m_event", "pretty_oneline__event"
	};

	pretty_oneline(s, e, content_keys);
	return s;
}

std::string
ircd::m::pretty_msgline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_msgline(s, event);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_msgline(std::ostream &s,
                        const event &e)
{
	using prototype = void (std::ostream &, const event &);
	static mods::import<prototype> pretty_msgline
	{
		"m_event", "pretty_msgline__event"
	};

	pretty_msgline(s, e);
	return s;
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, prev);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const event::prev &prev)
{
	using prototype = void (std::ostream &, const event::prev &);
	static mods::import<prototype> pretty
	{
		"m_event", "pretty__prev"
	};

	pretty(s, prev);
	return s;
}

std::string
ircd::m::pretty_oneline(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_oneline(s, prev);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event::prev &prev)
{
	using prototype = void (std::ostream &, const event::prev &);
	static mods::import<prototype> pretty_oneline
	{
		"m_event", "pretty_oneline__prev"
	};

	pretty_oneline(s, prev);
	return s;
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 id::event::buf &buf)
{
	const crh::sha256::buf hash{event};
	return make_id(event, buf, hash);
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 id::event::buf &buf,
                 const const_buffer &hash)
{
	char readable[b58encode_size(sha256::digest_size)];
	const id::event ret
	{
		buf, b58encode(readable, hash), my_host()
	};

	buf.assigned(ret);
	return ret;
}

bool
ircd::m::before(const event &a,
                const event &b)
{
	const event::prev prev{b};
	return prev.prev_events_has(at<"event_id"_>(a));
}

bool
ircd::m::operator>=(const event &a, const event &b)
{
	assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return at<"depth"_>(a) >= at<"depth"_>(b);
}

bool
ircd::m::operator<=(const event &a, const event &b)
{
	assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return at<"depth"_>(a) <= at<"depth"_>(b);
}

bool
ircd::m::operator>(const event &a, const event &b)
{
	assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return at<"depth"_>(a) > at<"depth"_>(b);
}

bool
ircd::m::operator<(const event &a, const event &b)
{
	assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return at<"depth"_>(a) < at<"depth"_>(b);
}

bool
ircd::m::operator==(const event &a, const event &b)
{
	assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return at<"event_id"_>(a) == at<"event_id"_>(b);
}

bool
ircd::m::bad(const id::event &event_id)
{
	bool ret {false};
	index(event_id, std::nothrow, [&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx == 0;
	});

	return ret;
}

bool
ircd::m::good(const id::event &event_id)
{
	return index(event_id, std::nothrow) != 0;
}

bool
ircd::m::cached(const id::event &event_id)
{
	return cached(event_id, event::fetch::default_opts);
}

bool
ircd::m::cached(const id::event &event_id,
                const event::fetch::opts &opts)
{
	if(!db::cached(dbs::event_idx, event_id, opts.gopts))
		return false;

	const event::idx event_idx
	{
		index(event_id, std::nothrow)
	};

	return event_idx?
		cached(event_idx, opts.gopts):
		false;
}

bool
ircd::m::cached(const event::idx &event_idx)
{
	return cached(event_idx, event::fetch::default_opts);
}

bool
ircd::m::cached(const event::idx &event_idx,
                const event::fetch::opts &opts)
{
	const byte_view<string_view> &key
	{
		event_idx
	};

	auto &columns(dbs::event_column);
	const event::keys &select(opts.keys);
	return std::all_of(begin(select), end(select), [&opts, &key, &columns]
	(const string_view &colname)
	{
		const auto &idx
		{
			json::indexof<event>(colname)
		};

		auto &column
		{
			columns.at(idx)
		};

		if(!column)
			return true;

		if(db::cached(column, key, opts.gopts))
			return true;

		if(!db::has(column, key, opts.gopts))
			return true;

		return false;
	});
}

bool
ircd::m::exists(const id::event &event_id,
                const bool &good)
{
	return good?
		m::good(event_id):
		m::exists(event_id);
}

bool
ircd::m::exists(const id::event &event_id)
{
	auto &column
	{
		dbs::event_idx
	};

	return has(column, event_id);
}

void
ircd::m::check_size(const event &event)
{
	const size_t &event_size
	{
		serialized(event)
	};

	if(event_size > size_t(event::max_size))
		throw m::BAD_JSON
		{
			"Event is %zu bytes which is larger than the maximum %zu bytes",
			event_size,
			size_t(event::max_size)
		};
}

bool
ircd::m::check_size(std::nothrow_t,
                    const event &event)
{
	const size_t &event_size
	{
		serialized(event)
	};

	return event_size <= size_t(event::max_size);
}

ircd::string_view
ircd::m::membership(const event &event)
{
	const json::object &content
	{
		json::get<"content"_>(event)
	};

	return unquote(content.get("membership"));
}

size_t
ircd::m::degree(const event &event)
{
	return degree(event::prev{event});
}

size_t
ircd::m::degree(const event::prev &prev)
{
	size_t ret{0};
	json::for_each(prev, [&ret]
	(const auto &, const json::array &prevs)
	{
		ret += prevs.count();
	});

	return ret;
}

size_t
ircd::m::count(const event::prev &prev)
{
	size_t ret{0};
	m::for_each(prev, [&ret](const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::for_each(const event::prev &prev,
                  const event::id::closure &closure)
{
	m::for_each(prev, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::for_each(const event::prev &prev,
                  const event::id::closure_bool &closure)
{
	return json::until(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const json::array &prev : prevs)
			if(!closure(event::id(unquote(prev.at(0)))))
				return false;

		return true;
	});
}

bool
ircd::m::my(const event &event)
{
	const auto &origin(json::get<"origin"_>(event));
	const auto &eid(json::get<"event_id"_>(event));
	return
		origin?
			my_host(origin):
		eid?
			my(event::id(eid)):
		false;
}

bool
ircd::m::my(const id::event &event_id)
{
	return self::host(event_id.host());
}

//
// event
//

/// The maximum size of an event we will create. This may also be used in
/// some contexts for what we will accept, but the protocol limit and hard
/// worst-case buffer size is still event::MAX_SIZE.
decltype(ircd::m::event::max_size)
ircd::m::event::max_size
{
	{ "name",     "m.event.max_size" },
	{ "default",   65507L            },
};

//
// event::event
//

ircd::m::event::event(const json::object &source)
:super_type
{
	source
}
,source
{
	source
}
{
}

ircd::m::event::event(const json::object &source,
                      const keys &keys)
:super_type
{
	source, keys
}
,source
{
	source
}
{
}

namespace ircd::m
{
	static json::object make_hashes(const mutable_buffer &out, const sha256::buf &hash);
}

ircd::json::object
ircd::m::hashes(const mutable_buffer &out,
                const event &event)
{
	const sha256::buf hash_
	{
		hash(event)
	};

	return make_hashes(out, hash_);
}

ircd::json::object
ircd::m::event::hashes(const mutable_buffer &out,
                       json::iov &event,
                       const string_view &content)
{
	const sha256::buf hash_
	{
		hash(event, content)
	};

	return make_hashes(out, hash_);
}

ircd::json::object
ircd::m::make_hashes(const mutable_buffer &out,
                     const sha256::buf &hash)
{
	static const auto b64bufsz(b64encode_size(sizeof(hash)));
	thread_local char hashb64buf[b64bufsz];
	const json::members hashes
	{
		{ "sha256", b64encode_unpadded(hashb64buf, hash) }
	};

	return json::stringify(mutable_buffer{out}, hashes);
}

ircd::sha256::buf
ircd::m::event::hash(json::iov &event,
                     const string_view &content)
{
	const json::iov::push _content
	{
		event, { "content", content }
	};

	return m::hash(event);
}

ircd::sha256::buf
ircd::m::hash(const event &event)
{
	thread_local char buf[event::MAX_SIZE];
	string_view preimage;

	//TODO: tuple::keys::selection
	if(defined(json::get<"signatures"_>(event)) ||
	   defined(json::get<"hashes"_>(event)))
	{
		m::event event_{event};
		json::get<"signatures"_>(event_) = {};
		json::get<"hashes"_>(event_) = {};
		preimage =
		{
			stringify(buf, event_)
		};
	}
	else preimage =
	{
		stringify(buf, event)
	};

	const sha256::buf hash
	{
		sha256{preimage}
	};

	return hash;
}

bool
ircd::m::verify_hash(const event &event)
{
	const sha256::buf hash
	{
		m::hash(event)
	};

	return verify_hash(event, hash);
}

bool
ircd::m::verify_hash(const event &event,
                     const sha256::buf &hash)
{
	static const size_t hashb64sz
	{
		size_t(hash.size() * 1.34) + 1
	};

	thread_local char b64buf[hashb64sz];
	return verify_sha256b64(event, b64encode_unpadded(b64buf, hash));
}

bool
ircd::m::verify_sha256b64(const event &event,
                          const string_view &b64)
try
{
	const json::object &object
	{
		at<"hashes"_>(event)
	};

	const string_view &hash
	{
		unquote(object.at("sha256"))
	};

	return hash == b64;
}
catch(const json::not_found &)
{
	return false;
}

ircd::json::object
ircd::m::event::signatures(const mutable_buffer &out,
                           json::iov &event,
                           const json::iov &content)
{
	const ed25519::sig sig
	{
		sign(event, content)
	};

	thread_local char sigb64buf[b64encode_size(sizeof(sig))];
	const json::members sigb64
	{
		{ self::public_key_id, b64encode_unpadded(sigb64buf, sig) }
	};

	const json::members sigs
	{
		{ my_host(), sigb64 }
    };

	return json::stringify(mutable_buffer{out}, sigs);
}

ircd::m::event
ircd::m::signatures(const mutable_buffer &out_,
                    const m::event &event_)
{
	thread_local char content[event::MAX_SIZE];
	m::event event
	{
		essential(event_, content)
	};

	thread_local char buf[event::MAX_SIZE];
	const json::object &preimage
	{
		stringify(buf, event)
	};

	const ed25519::sig sig
	{
		sign(preimage)
	};

	thread_local char sigb64buf[b64encode_size(sizeof(sig))];
	const json::member my_sig
	{
		my_host(), json::members
		{
			{ self::public_key_id, b64encode_unpadded(sigb64buf, sig) }
		}
	};

	static const size_t SIG_MAX{64};
	thread_local std::array<json::member, SIG_MAX> sigs;

	size_t i(0);
	sigs.at(i++) = my_sig;
	for(const auto &other : json::get<"signatures"_>(event_))
		if(!my_host(unquote(other.first)))
			sigs.at(i++) = { other.first, other.second };

	event = event_;
	mutable_buffer out{out_};
	json::get<"signatures"_>(event) = json::stringify(out, sigs.data(), sigs.data() + i);
	return event;
}

ircd::ed25519::sig
ircd::m::event::sign(json::iov &event,
                     const json::iov &contents)
{
	return sign(event, contents, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(json::iov &event,
                     const json::iov &contents,
                     const ed25519::sk &sk)
{
	ed25519::sig sig;
	essential(event, contents, [&sk, &sig]
	(json::iov &event)
	{
		sig = m::sign(event, sk);
	});

	return sig;
}

ircd::ed25519::sig
ircd::m::sign(const event &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::sign(const event &event,
              const ed25519::sk &sk)
{
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return event::sign(preimage, sk);
}

ircd::ed25519::sig
ircd::m::event::sign(const json::object &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(const json::object &event,
                     const ed25519::sk &sk)
{
	//TODO: skip rewrite
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return sign(preimage, sk);
}

ircd::ed25519::sig
ircd::m::event::sign(const string_view &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(const string_view &event,
                     const ed25519::sk &sk)
{
	const ed25519::sig sig
	{
		sk.sign(event)
	};

	return sig;
}
bool
ircd::m::verify(const event &event)
{
	const string_view &origin
	{
		at<"origin"_>(event)
	};

	return verify(event, origin);
}

bool
ircd::m::verify(const event &event,
                const string_view &origin)
{
	const json::object &signatures
	{
		at<"signatures"_>(event)
	};

	const json::object &origin_sigs
	{
		signatures.at(origin)
	};

	for(const auto &p : origin_sigs)
		if(verify(event, origin, unquote(p.first)))
			return true;

	return false;
}

bool
ircd::m::verify(const event &event,
                const string_view &origin,
                const string_view &keyid)
try
{
	const m::node::id::buf node_id
	{
		m::node::id::origin, origin
	};

	const m::node node
	{
		node_id
	};

	bool ret{false};
	node.key(keyid, [&ret, &event, &origin, &keyid]
	(const ed25519::pk &pk)
	{
		ret = verify(event, pk, origin, keyid);
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	log::derror
	{
		"Failed to verify %s because key %s for %s :%s",
		string_view{json::get<"event_id"_>(event)},
		keyid,
		origin,
		e.what()
	};

	return false;
}

bool
ircd::m::verify(const event &event,
                const ed25519::pk &pk,
                const string_view &origin,
                const string_view &keyid)
{
	const json::object &signatures
	{
		at<"signatures"_>(event)
	};

	const json::object &origin_sigs
	{
		signatures.at(origin)
	};

	const ed25519::sig sig
	{
		[&origin_sigs, &keyid](auto &buf)
		{
			b64decode(buf, unquote(origin_sigs.at(keyid)));
		}
	};

	return verify(event, pk, sig);
}

bool
ircd::m::verify(const event &event_,
                const ed25519::pk &pk,
                const ed25519::sig &sig)
{
	thread_local char content[event::MAX_SIZE];
	m::event event
	{
		essential(event_, content)
	};

	thread_local char buf[event::MAX_SIZE];
	const json::object &preimage
	{
		stringify(buf, event)
	};

	return event::verify(preimage, pk, sig);
}

bool
ircd::m::event::verify(const json::object &event,
                       const ed25519::pk &pk,
                       const ed25519::sig &sig)
{
	//TODO: skip rewrite
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return verify(preimage, pk, sig);
}

bool
ircd::m::event::verify(const string_view &event,
                       const ed25519::pk &pk,
                       const ed25519::sig &sig)
{
	return pk.verify(event, sig);
}

void
ircd::m::event::essential(json::iov &event,
                          const json::iov &contents,
                          const closure_iov_mutable &closure)
{
	using prototype = void (json::iov &, const json::iov &, const closure_iov_mutable &);

	static mods::import<prototype> _essential
	{
		"m_event", "_essential__iov"
	};

	_essential(event, contents, closure);
}

ircd::m::event
ircd::m::essential(m::event event,
                   const mutable_buffer &contentbuf)
{
	using prototype = m::event &(m::event &, const mutable_buffer &);

	static mods::import<prototype> _essential
	{
		"m_event", "_essential"
	};

	return _essential(event, contentbuf);
}

//
// event::prev
//

bool
ircd::m::event::prev::prev_events_has(const event::id &event_id)
const
{
	for(const json::array &p : json::get<"prev_events"_>(*this))
		if(unquote(p.at(0)) == event_id)
			return true;

	return false;
}

bool
ircd::m::event::prev::prev_states_has(const event::id &event_id)
const
{
	for(const json::array &p : json::get<"prev_state"_>(*this))
		if(unquote(p.at(0)) == event_id)
			return true;

	return false;
}

bool
ircd::m::event::prev::auth_events_has(const event::id &event_id)
const
{
	for(const json::array &p : json::get<"auth_events"_>(*this))
		if(unquote(p.at(0)) == event_id)
			return true;

	return false;
}

size_t
ircd::m::event::prev::prev_events_count()
const
{
	return json::get<"prev_events"_>(*this).count();
}

size_t
ircd::m::event::prev::prev_states_count()
const
{
	return json::get<"prev_state"_>(*this).count();
}

size_t
ircd::m::event::prev::auth_events_count()
const
{
	return json::get<"auth_events"_>(*this).count();
}

ircd::m::event::id
ircd::m::event::prev::auth_event(const uint &idx)
const
{
	return std::get<0>(auth_events(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_state(const uint &idx)
const
{
	return std::get<0>(prev_states(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_event(const uint &idx)
const
{
	return std::get<0>(prev_events(idx));
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::auth_events(const uint &idx)
const
{
	const json::array &auth_event
	{
		at<"auth_events"_>(*this).at(idx)
	};

	return
	{
		unquote(auth_event.at(0)), unquote(auth_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_states(const uint &idx)
const
{
	const json::array &state_event
	{
		at<"prev_state"_>(*this).at(idx)
	};

	return
	{
		unquote(state_event.at(0)), unquote(state_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_events(const uint &idx)
const
{
	const json::array &prev_event
	{
		at<"prev_events"_>(*this).at(idx)
	};

	return
	{
		unquote(prev_event.at(0)), unquote(prev_event[1])
	};
}

void
ircd::m::prefetch(const event::id &event_id,
                  const event::fetch::opts &opts)
{
	prefetch(index(event_id), opts);
}

void
ircd::m::prefetch(const event::id &event_id,
                  const string_view &key)
{
	prefetch(index(event_id), key);
}

void
ircd::m::prefetch(const event::idx &event_idx,
                  const event::fetch::opts &opts)
{
	const event::keys keys{opts.keys};
	const vector_view<const string_view> cols{keys};
	for(const auto &col : cols)
		if(col)
			prefetch(event_idx, col);
}

void
ircd::m::prefetch(const event::idx &event_idx,
                  const string_view &key)
{
	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	db::prefetch(column, byte_view<string_view>{event_idx});
}

ircd::const_buffer
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const mutable_buffer &out)
{
	const auto &ret
	{
		get(std::nothrow, index(event_id), key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for %s not found in database",
			key,
			string_view{event_id}
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &out)
{
	const const_buffer ret
	{
		get(std::nothrow, event_idx, key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database",
			key,
			event_idx
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const mutable_buffer &buf)
{
	return get(std::nothrow, index(event_id), key, buf);
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &buf)
{
	const_buffer ret;
	get(std::nothrow, event_idx, key, [&buf, &ret]
	(const string_view &value)
	{
		ret = { data(buf), copy(buf, value) };
	});

	return ret;
}

void
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, index(event_id), key, closure))
		throw m::NOT_FOUND
		{
			"%s for %s not found in database",
			key,
			string_view{event_id}
		};
}

void
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, event_idx, key, closure))
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database",
			key,
			event_idx
		};
}

bool
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	return get(std::nothrow, index(event_id), key, closure);
}

bool
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	const string_view &column_key
	{
		byte_view<string_view>{event_idx}
	};

	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	return column(column_key, std::nothrow, closure);
}

ircd::m::event::idx
ircd::m::index(const event &event)
try
{
	return index(at<"event_id"_>(event));
}
catch(const json::not_found &)
{
	throw m::NOT_FOUND
	{
		"Cannot find index for event without an event_id."
	};
}

ircd::m::event::idx
ircd::m::index(const event &event,
               std::nothrow_t)
try
{
	return index(at<"event_id"_>(event), std::nothrow);
}
catch(const json::not_found &)
{
	return 0;
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	const auto ret
	{
		index(event_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"no index found for %s",
			string_view{event_id}

		};

	return ret;
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id,
               std::nothrow_t)
{
	event::idx ret{0};
	index(event_id, std::nothrow, [&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	});

	return ret;
}

bool
ircd::m::index(const event::id &event_id,
               std::nothrow_t,
               const event::closure_idx &closure)
{
	auto &column
	{
		dbs::event_idx
	};

	return column(event_id, std::nothrow, [&closure]
	(const string_view &value)
	{
		const event::idx &event_idx
		{
			byte_view<event::idx>(value)
		};

		closure(event_idx);
	});
}

void
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id)
{
	if(!seek(fetch, event_id, std::nothrow))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id,
              std::nothrow_t)
{
	const auto &event_idx
	{
		index(event_id, std::nothrow)
	};

	if(!event_idx)
	{
		fetch.valid = false;
		return fetch.valid;
	}

	return seek(fetch, event_idx, std::nothrow);
}

void
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx)
{
	if(!seek(fetch, event_idx, std::nothrow))
		throw m::NOT_FOUND
		{
			"%lu not found in database", event_idx
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              std::nothrow_t)
{
	auto &event
	{
		static_cast<m::event &>(fetch)
	};

	const string_view &key
	{
		byte_view<string_view>(event_idx)
	};

	assert(fetch.fopts);
	const auto &opts(*fetch.fopts);
	if(fetch.should_seek_json(opts))
		if((fetch.valid = fetch._json.load(key, opts.gopts)))
			return fetch.assign_from_json();

	if((fetch.valid = db::seek(fetch.row, key, opts.gopts)))
		fetch.valid = fetch.assign_from_row(key);

	return fetch.valid;
}

//
// event::fetch
//

decltype(ircd::m::event::fetch::default_opts)
ircd::m::event::fetch::default_opts
{};

void
ircd::m::event::fetch::event_id(const idx &idx,
                                const id::closure &closure)
{
	if(!get(std::nothrow, idx, "event_id", closure))
		throw m::NOT_FOUND
		{
			"%lu not found in database", idx
		};
}

bool
ircd::m::event::fetch::event_id(const idx &idx,
                                std::nothrow_t,
                                const id::closure &closure)
{
	return get(std::nothrow, idx, "event_id", closure);
}

//
// event::fetch::fetch
//

/// Seek to event_id and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             const opts &opts)
:fetch
{
	index(event_id), opts
}
{
}

/// Seek to event_id and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             std::nothrow_t,
                             const opts &opts)
:fetch
{
	index(event_id, std::nothrow), std::nothrow, opts
}
{
}

/// Seek to event_idx and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             const opts &opts)
:fetch
{
	event_idx, std::nothrow, opts
}
{
	if(!valid)
		throw m::NOT_FOUND
		{
			"idx %zu not found in database", event_idx
		};
}

/// Seek to event_idx and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             std::nothrow_t,
                             const opts &opts)
:event{}
,fopts
{
	&opts
}
,_json
{
	dbs::event_json,
	event_idx && should_seek_json(opts)?
		key(&event_idx):
		string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	event_idx && !_json.valid(key(&event_idx))?
		key(&event_idx):
		string_view{},
	event_idx && !_json.valid(key(&event_idx))?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	event_idx && _json.valid(key(&event_idx))?
		assign_from_json():
		assign_from_row(key(&event_idx))
}
{
}

/// Seekless constructor.
ircd::m::event::fetch::fetch(const opts &opts)
:event{}
,fopts
{
	&opts
}
,_json
{
	dbs::event_json,
	string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	string_view{},
	!should_seek_json(opts)?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	false
}
{
}

bool
ircd::m::event::fetch::assign_from_json()
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	const json::object source
	{
		_json.val()
	};

	assert(fopts);
	event =
	{
		source, fopts->keys
	};

	assert(!empty(source));
	assert(data(event.source) == data(source));
	return true;
}

bool
ircd::m::event::fetch::assign_from_row(const string_view &key)
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	if(!row.valid(key))
		return false;

	assign(event, row, key);
	event.source = {};
	return true;
}

bool
ircd::m::event::fetch::should_seek_json(const opts &opts)
{
	// User always wants to make the event_json query regardless
	// of their keys selection.
	if(opts.query_json_force)
		return true;

	// If and only if selected keys have direct columns we can return
	// false to seek direct columns. If any other keys are selected we
	/// must perform the event_json query instead.
	for(size_t i(0); i < opts.keys.size(); ++i)
		if(opts.keys.test(i))
			if(!dbs::event_column.at(i))
				return true;

	return false;
}

ircd::string_view
ircd::m::event::fetch::key(const event::idx *const &event_idx)
{
	assert(event_idx);
	return byte_view<string_view>(*event_idx);
}

//
// event::fetch::opts
//

ircd::m::event::fetch::opts::opts(const db::gopts &gopts,
                                  const event::keys::selection &keys)
:opts
{
	keys, gopts
}
{
}

ircd::m::event::fetch::opts::opts(const event::keys::selection &keys,
                                  const db::gopts &gopts)
:keys{keys}
,gopts{gopts}
{
}

//
// event::conforms
//

namespace ircd::m
{
	const size_t event_conforms_num{num_of<event::conforms::code>()};
	extern const std::array<string_view, event_conforms_num> event_conforms_reflects;
}

decltype(ircd::m::event_conforms_reflects)
ircd::m::event_conforms_reflects
{
	"INVALID_OR_MISSING_EVENT_ID",
	"INVALID_OR_MISSING_ROOM_ID",
	"INVALID_OR_MISSING_SENDER_ID",
	"MISSING_TYPE",
	"MISSING_ORIGIN",
	"INVALID_ORIGIN",
	"INVALID_OR_MISSING_REDACTS_ID",
	"MISSING_CONTENT_MEMBERSHIP",
	"INVALID_CONTENT_MEMBERSHIP",
	"MISSING_PREV_EVENTS",
	"MISSING_PREV_STATE",
	"DEPTH_NEGATIVE",
	"DEPTH_ZERO",
	"MISSING_SIGNATURES",
	"MISSING_ORIGIN_SIGNATURE",
	"MISMATCH_ORIGIN_SENDER",
	"MISMATCH_ORIGIN_EVENT_ID",
	"SELF_REDACTS",
	"SELF_PREV_EVENT",
	"SELF_PREV_STATE",
	"DUP_PREV_EVENT",
	"DUP_PREV_STATE",
};

std::ostream &
ircd::m::operator<<(std::ostream &s, const event::conforms &conforms)
{
	thread_local char buf[1024];
	s << conforms.string(buf);
	return s;
}

ircd::string_view
ircd::m::reflect(const event::conforms::code &code)
try
{
	return event_conforms_reflects.at(code);
}
catch(const std::out_of_range &e)
{
	return "??????"_sv;
}

ircd::m::event::conforms::code
ircd::m::event::conforms::reflect(const string_view &name)
{
	const auto it
	{
		std::find(begin(event_conforms_reflects), end(event_conforms_reflects), name)
	};

	if(it == end(event_conforms_reflects))
		throw std::out_of_range
		{
			"There is no event::conforms code by that name."
		};

	return code(std::distance(begin(event_conforms_reflects), it));
}

ircd::m::event::conforms::conforms(const event &e,
                                   const uint64_t &skip)
:conforms{e}
{
	report &= ~skip;
}

ircd::m::event::conforms::conforms(const event &e)
:report{0}
{
	if(!valid(m::id::EVENT, json::get<"event_id"_>(e)))
		set(INVALID_OR_MISSING_EVENT_ID);

	if(!valid(m::id::ROOM, json::get<"room_id"_>(e)))
		set(INVALID_OR_MISSING_ROOM_ID);

	if(!valid(m::id::USER, json::get<"sender"_>(e)))
		set(INVALID_OR_MISSING_SENDER_ID);

	if(empty(json::get<"type"_>(e)))
		set(MISSING_TYPE);

	if(empty(json::get<"origin"_>(e)))
		set(MISSING_ORIGIN);

	//TODO: XXX
	if(false)
		set(INVALID_ORIGIN);

	if(empty(json::get<"signatures"_>(e)))
		set(MISSING_SIGNATURES);

	if(empty(json::object{json::get<"signatures"_>(e).get(json::get<"origin"_>(e))}))
		set(MISSING_ORIGIN_SIGNATURE);

	if(!has(INVALID_OR_MISSING_SENDER_ID))
		if(json::get<"origin"_>(e) != m::id::user{json::get<"sender"_>(e)}.host())
			set(MISMATCH_ORIGIN_SENDER);

	if(!has(INVALID_OR_MISSING_EVENT_ID))
		if(json::get<"origin"_>(e) != m::id::event{json::get<"event_id"_>(e)}.host())
			set(MISMATCH_ORIGIN_EVENT_ID);

	if(json::get<"type"_>(e) == "m.room.redaction")
		if(!valid(m::id::EVENT, json::get<"redacts"_>(e)))
			set(INVALID_OR_MISSING_REDACTS_ID);

	if(json::get<"redacts"_>(e))
		if(json::get<"redacts"_>(e) == json::get<"event_id"_>(e))
			set(SELF_REDACTS);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(unquote(json::get<"content"_>(e).get("membership"))))
			set(MISSING_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(unquote(json::get<"content"_>(e).get("membership"))))
			set(INVALID_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"prev_events"_>(e)))
			set(MISSING_PREV_EVENTS);

	/*
	if(json::get<"type"_>(e) != "m.room.create")
		if(!empty(json::get<"state_key"_>(e)))
			if(empty(json::get<"prev_state"_>(e)))
				set(MISSING_PREV_STATE);
	*/

	if(json::get<"depth"_>(e) != json::undefined_number && json::get<"depth"_>(e) < 0)
		set(DEPTH_NEGATIVE);

	if(json::get<"type"_>(e) != "m.room.create")
		if(json::get<"depth"_>(e) == 0)
			set(DEPTH_ZERO);

	const prev p{e};
	size_t i{0}, j{0};
	for(const json::array &pe : json::get<"prev_events"_>(p))
	{
		if(unquote(pe.at(0)) == json::get<"event_id"_>(e))
			set(SELF_PREV_EVENT);

		j = 0;
		for(const json::array &pe_ : json::get<"prev_events"_>(p))
			if(i != j++)
				if(pe_.at(0) == pe.at(0))
					set(DUP_PREV_EVENT);

		++i;
	}

	i = 0;
	for(const json::array &ps : json::get<"prev_state"_>(p))
	{
		if(unquote(ps.at(0)) == json::get<"event_id"_>(e))
			set(SELF_PREV_STATE);

		j = 0;
		for(const json::array &ps_ : json::get<"prev_state"_>(p))
			if(i != j++)
				if(ps_.at(0) == ps.at(0))
					set(DUP_PREV_STATE);

		++i;
	}
}

void
ircd::m::event::conforms::operator|=(const code &code)
&
{
	set(code);
}

void
ircd::m::event::conforms::del(const code &code)
{
	report &= ~(1UL << code);
}

void
ircd::m::event::conforms::set(const code &code)
{
	report |= (1UL << code);
}

ircd::string_view
ircd::m::event::conforms::string(const mutable_buffer &out)
const
{
	mutable_buffer buf{out};
	for(uint64_t i(0); i < num_of<code>(); ++i)
	{
		if(!has(code(i)))
			continue;

		if(begin(buf) != begin(out))
			consume(buf, copy(buf, " "_sv));

		consume(buf, copy(buf, m::reflect(code(i))));
	}

	return { data(out), begin(buf) };
}

bool
ircd::m::event::conforms::has(const code &code)
const
{
	return report & (1UL << code);
}

bool
ircd::m::event::conforms::has(const uint &code)
const
{
	return (report & (1UL << code)) == code;
}

bool
ircd::m::event::conforms::operator!()
const
{
	return clean();
}

ircd::m::event::conforms::operator bool()
const
{
	return !clean();
}

bool
ircd::m::event::conforms::clean()
const
{
	return report == 0;
}
