// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/util/params.h>

using namespace ircd;

mapi::header IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable library supporting the console."
};

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

// Buffer all output into this rather than writing to std::cout. This allows
// the console to be reused easily inside the application (like a matrix room).
std::stringstream out;

static bool console_cmd__fed(const string_view &line);
static bool console_cmd__room(const string_view &line);
static bool console_cmd__state(const string_view &line);
static bool console_cmd__event(const string_view &line);
static bool console_cmd__exec(const string_view &line);
static bool console_cmd__key(const string_view &line);
static bool console_cmd__db(const string_view &line);
static bool console_cmd__net(const string_view &line);
static bool console_cmd__mod(const string_view &line);
static bool console_cmd__debuglog(const string_view &line);
static bool console_cmd__test(const string_view &line);

static bool console_id__user(const m::user::id &id, const string_view &args);
static bool console_id__room(const m::room::id &id, const string_view &args);
static bool console_id__event(const m::event::id &id, const string_view &args);

extern "C" int
console_command(const string_view &line, std::string &output)
try
{
	const unwind writeback{[&output]
	{
		output = out.str();
		out.str(std::string{});
	}};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("test"):
			return console_cmd__test(args);

		case hash("debuglog"):
			return console_cmd__debuglog(args);

		case hash("mod"):
			return console_cmd__mod(args);

		case hash("net"):
			return console_cmd__net(args);

		case hash("db"):
			return console_cmd__db(args);

		case hash("key"):
			return console_cmd__key(args);

		case hash("exec"):
			return console_cmd__exec(args);

		case hash("event"):
			return console_cmd__event(args);

		case hash("state"):
			return console_cmd__state(args);

		case hash("room"):
			return console_cmd__room(args);

		case hash("fed"):
			return console_cmd__fed(args);

		default:
		{
			const string_view id{token(line, ' ', 0)};
			if(m::has_sigil(id)) switch(m::sigil(id))
			{
				case m::id::EVENT:
					return console_id__event(id, args);

				case m::id::ROOM:
					return console_id__room(id, args);

				case m::id::USER:
					return console_id__user(id, args);

				default:
					break;
			}
			else break;
		}
	}

	return -1;
}
catch(const bad_command &e)
{
	return -2;
}

//
// Test trigger stub
//

bool
console_cmd__test(const string_view &line)
{
	return true;
}

bool
console_cmd__debuglog(const string_view &line)
{
	if(!RB_DEBUG_LEVEL)
	{
		out << "Debugging is not compiled in." << std::endl;
		return true;
	}

	if(log::console_enabled(log::DEBUG))
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
		return true;
	} else {
		out << "Turning on debuglog..." << std::endl;
		log::console_enable(log::DEBUG);
		return true;
	}
}

//
// Command by ID
//

bool
console_id__event(const m::event::id &id,
                   const string_view &args)
{
	return true;
}

bool
console_id__room(const m::room::id &id,
                 const string_view &args)
{
	return true;
}

bool
console_id__user(const m::user::id &id,
                 const string_view &args)
{
	return true;
}

//
// mod
//

static bool console_cmd__mod_path(const string_view &line);
static bool console_cmd__mod_list(const string_view &line);
static bool console_cmd__mod_syms(const string_view &line);
static bool console_cmd__mod_reload(const string_view &line);

bool
console_cmd__mod(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("path"):
			return console_cmd__mod_path(args);

		case hash("list"):
			return console_cmd__mod_list(args);

		case hash("syms"):
			return console_cmd__mod_syms(args);

		case hash("reload"):
			return console_cmd__mod_reload(args);

		default:
			throw bad_command{};
	}
}

bool
console_cmd__mod_path(const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod_list(const string_view &line)
{
	auto avflist(mods::available());
	const auto b(std::make_move_iterator(begin(avflist)));
	const auto e(std::make_move_iterator(end(avflist)));
	std::vector<std::string> available(b, e);
	std::sort(begin(available), end(available));

	for(const auto &mod : available)
	{
		const auto loadstr
		{
			mods::loaded(mod)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod_syms(const string_view &line)
{
	const std::string path
	{
		token(line, ' ', 0)
	};

	const std::vector<std::string> symbols
	{
		mods::symbols(path)
	};

	for(const auto &sym : symbols)
		out << sym << std::endl;

	out << " -- " << symbols.size() << " symbols in " << path << std::endl;
	return true;
}

bool
console_cmd__mod_reload(const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(!m::modules.erase(name))
	{
		out << name << " is not loaded." << std::endl;
		return true;
	}

	m::modules.emplace(name, name);
	return true;
}

//
// db
//

static bool console_cmd__db_list(const string_view &line);
static bool console_cmd__db_prop(const string_view &line);

bool
console_cmd__db(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("prop"):
			return console_cmd__db_prop(args);

		default:
		case hash("list"):
			return console_cmd__db_list(args);
	}
}

bool
console_cmd__db_prop(const string_view &line)
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	const auto property
	{
		token(line, ' ', 1)
	};

	return true;
}

bool
console_cmd__db_list(const string_view &line)
{
	const auto available
	{
		db::available()
	};

	for(const auto &path : available)
	{
		const auto name
		{
			lstrip(path, db::path("/"))
		};

		const auto it
		{
			db::database::dbs.find(name)
		};

		const auto &light
		{
			it != end(db::database::dbs)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << light << "]"
		    << " " << name
		    << " `" << path << "'"
		    << std::endl;
	}

	return true;
}

//
// net
//

static bool console_cmd__net_peer(const string_view &line);
static bool console_cmd__net_host(const string_view &line);

bool
console_cmd__net(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("host"):
			return console_cmd__net_host(args);

		case hash("peer"):
			return console_cmd__net_peer(args);

		default:
			throw bad_command{};
	}
}

static bool console_cmd__net_peer__default();

bool
console_cmd__net_peer(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	if(empty(line))
		return console_cmd__net_peer__default();

	switch(hash(token(line, ' ', 0)))
	{
		default:
			throw bad_command{};
	}

	return true;
}

bool
console_cmd__net_peer__default()
{
	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << " "
		    << " " << setw(2) << right << peer.link_count()   << " L"
		    << " " << setw(2) << right << peer.tag_count()    << " T"
		    << " " << setw(9) << right << peer.write_total()  << " UP"
		    << " " << setw(9) << right << peer.read_total()   << " DN"
		    ;

		if(peer.err_has() && peer.err_msg())
			out << "  :" << peer.err_msg();
		else if(peer.err_has())
			out << "  <unknown error>"_sv;

		out << std::endl;
	}

	return true;
}

static bool console_cmd__net_host_cache(const string_view &line);
static bool console_cmd__net_host__default(const string_view &line);

bool
console_cmd__net_host(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("cache"):
			return console_cmd__net_host_cache(args);

		default:
			return console_cmd__net_host__default(line);
	}

	return true;
}

bool
console_cmd__net_host_cache(const string_view &line)
{
	switch(hash(token(line, " ", 0)))
	{
		case hash("A"):
		{
			for(const auto &pair : net::dns::cache.A)
			{
				const auto &host{pair.first};
				const auto &record{pair.second};
				const net::ipport ipp{record.ip4, 0};
				out << std::setw(32) << host
				    << " => " << ipp
				    <<  " expires " << timestr(record.ttl, ircd::localtime)
				    << " (" << record.ttl << ")"
				    << std::endl;
			}

			return true;
		}

		case hash("SRV"):
		{
			for(const auto &pair : net::dns::cache.SRV)
			{
				const auto &key{pair.first};
				const auto &record{pair.second};
				const net::hostport hostport
				{
					record.tgt, record.port
				};

				out << std::setw(32) << key
				    << " => " << hostport
				    <<  " expires " << timestr(record.ttl, ircd::localtime)
				    << " (" << record.ttl << ")"
				    << std::endl;
			}

			return true;
		}

		default: throw bad_command
		{
			"Which cache?"
		};
	}
}

bool
console_cmd__net_host__default(const string_view &line)
{
	const params token
	{
		line, " ", {"host", "service"}
	};

	const auto &host{token.at(0)};
	const auto &service
	{
		token.count() > 1? token.at(1) : string_view{}
	};

	const net::hostport hostport
	{
		host, service
	};

	ctx::dock dock;
	bool done{false};
	net::ipport ipport;
	std::exception_ptr eptr;
	net::dns(hostport, [&done, &dock, &eptr, &ipport]
	(std::exception_ptr eptr_, const net::ipport &ipport_)
	{
		eptr = std::move(eptr_);
		ipport = ipport_;
		done = true;
		dock.notify_one();
	});

	while(!done)
		dock.wait();

	if(eptr)
		std::rethrow_exception(eptr);
	else
		out << ipport << std::endl;

	return true;
}

//
// key
//

static bool console_cmd__key__default(const string_view &line);
static bool console_cmd__key__fetch(const string_view &line);
static bool console_cmd__key__get(const string_view &line);

bool
console_cmd__key(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	if(!empty(args)) switch(hash(token(line, " ", 0)))
	{
		case hash("get"):
			return console_cmd__key__get(args);

		case hash("fetch"):
			return console_cmd__key__fetch(args);
	}

	return console_cmd__key__default(args);
}

bool
console_cmd__key__get(const string_view &line)
{
	const auto server_name
	{
		token(line, ' ', 0)
	};

	m::keys::get(server_name, [](const auto &keys)
	{
		out << keys << std::endl;
	});

	return true;
}

bool
console_cmd__key__fetch(const string_view &line)
{

	return true;
}

bool
console_cmd__key__default(const string_view &line)
{
	out << "origin:                  " << m::my_host() << std::endl;
	out << "public key ID:           " << m::self::public_key_id << std::endl;
	out << "public key base64:       " << m::self::public_key_b64 << std::endl;
	out << "TLS cert sha256 base64:  " << m::self::tls_cert_der_sha256_b64 << std::endl;

	return true;
}

//
// event
//

static bool console_cmd__event__default(const string_view &line);
static bool console_cmd__event__dump(const string_view &line);
static bool console_cmd__event__fetch(const string_view &line);

bool
console_cmd__event(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("fetch"):
			return console_cmd__event__fetch(args);

		case hash("dump"):
			return console_cmd__event__dump(args);

		default:
			return console_cmd__event__default(line);
	}
}

bool
console_cmd__event__dump(const string_view &line)
{
	const auto filename
	{
		token(line, ' ', 0)
	};

	db::column column
	{
		*m::dbs::events, "event_id"
	};

	static char buf[512_KiB];
	char *pos{buf};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::event::fetch event;
	for(auto it(begin(column)); it != end(column); ++it, ++ecount)
	{
		const auto remain
		{
			size_t(buf + sizeof(buf) - pos)
		};

		assert(remain >= 64_KiB && remain <= sizeof(buf));
		const mutable_buffer mb{pos, remain};
		const string_view event_id{it->second};
		seek(event, event_id, std::nothrow);
		if(unlikely(!event.valid(event_id)))
		{
			++errcount;
			continue;
		}

		pos += json::print(mb, event);
		if(pos + 64_KiB > buf + sizeof(buf))
		{
			const const_buffer cb{buf, pos};
			foff += size(fs::append(filename, cb));
			pos = buf;
			++acount;
		}
	}

	if(pos > buf)
	{
		const const_buffer cb{buf, pos};
		foff += size(fs::append(filename, cb));
		++acount;
	}

	out << "Dumped " << ecount << " events"
	    << " using " << foff << " bytes"
	    << " in " << acount << " writes"
	    << " to " << filename
	    << " with " << errcount << " errors"
	    << std::endl;

	return true;
}

bool
console_cmd__event__fetch(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	const auto host
	{
		!empty(args)? token(args, ' ', 0) : ""_sv
	};

	m::v1::event::opts opts;
	if(host)
		opts.remote = host;

	static char buf[96_KiB];
	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	//TODO: TO

	const auto code
	{
		request.get()
	};

	const m::event event
	{
		request
	};

	out << json::object{request} << std::endl;
	out << std::endl;
	out << pretty(event) << std::endl;
	return true;
}

bool
console_cmd__event__default(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	static char buf[64_KiB];
	const m::event event
	{
		event_id, buf
	};

	if(!empty(args)) switch(hash(token(args, ' ', 0)))
	{
		case hash("raw"):
			out << json::object{buf} << std::endl;
			return true;
	}

	out << pretty(event) << std::endl;
	return true;
}

//
// state
//

static bool console_cmd__state_root(const string_view &line);
static bool console_cmd__state_count(const string_view &line);
static bool console_cmd__state_get(const string_view &line);
static bool console_cmd__state_each(const string_view &line);
static bool console_cmd__state_dfs(const string_view &line);

bool
console_cmd__state(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("root"):
			return console_cmd__state_root(args);

		case hash("get"):
			return console_cmd__state_get(args);

		case hash("count"):
			return console_cmd__state_count(args);

		case hash("each"):
			return console_cmd__state_each(args);

		case hash("dfs"):
			return console_cmd__state_dfs(args);

		default:
			throw bad_command{};
	}
}

bool
console_cmd__state_count(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	out << m::state::count(root) << std::endl;
	return true;
}

bool
console_cmd__state_each(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view root
	{
		arg
	};

	m::state::for_each(root, type, []
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
	});

	return true;
}

bool
console_cmd__state_get(const string_view &line)
{
	const string_view root
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	m::state::get(root, type, state_key, []
	(const auto &value)
	{
		out << "got: " << value << std::endl;
	});

	return true;
}

bool
console_cmd__state_dfs(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	m::state::dfs(root, []
	(const auto &key, const string_view &val, const uint &depth, const uint &pos)
	{
		out << std::setw(2) << depth << " + " << pos
		    << " : " << key << " => " << val
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__state_root(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	out << m::dbs::state_root(buf, event_id) << std::endl;
	return true;
}

//
// exec
//

static bool console_cmd__exec_file(const string_view &line);

bool
console_cmd__exec(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		default:
			return console_cmd__exec_file(line);
	}
}

bool
console_cmd__exec_file(const string_view &line)
{
	const params token{line, " ",
	{
		"file path", "limit", "start", "room_id/event_id/sender"
	}};

	const auto path
	{
		token.at(0)
	};

	const auto limit
	{
		token.at<size_t>(1)
	};

	const auto start
	{
		token[2]? lex_cast<size_t>(token[2]) : 0
	};

	const string_view id{token[3]};
	const string_view room_id
	{
		id && m::sigil(id) == m::id::ROOM? id : string_view{}
	};

	const string_view event_id
	{
		id && m::sigil(id) == m::id::EVENT? id : string_view{}
	};

	const string_view sender
	{
		id && m::sigil(id) == m::id::USER? id : string_view{}
	};

	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	opts.prev_check_exists = false;
	opts.notify = false;
	m::vm::eval eval
	{
		opts
	};

	size_t foff{0};
	size_t i(0), j(0), r(0);
	for(; !limit || i < limit; ++r)
	{
		static char buf[512_KiB];
		const string_view read
		{
			ircd::fs::read(path, buf, foff)
		};

		size_t boff(0);
		json::object object;
		json::vector vector{read};
		for(; boff < size(read) && i < limit; ) try
		{
			object = *begin(vector);
			boff += size(string_view{object});
			vector = { data(read) + boff, size(read) - boff };
			const m::event event
			{
				object
			};

			if(room_id && json::get<"room_id"_>(event) != room_id)
				continue;

			if(event_id && json::get<"event_id"_>(event) != event_id)
				continue;

			if(sender && json::get<"sender"_>(event) != sender)
				continue;

			if(j++ < start)
				continue;

			eval(event);
			++i;
		}
		catch(const json::parse_error &e)
		{
			break;
		}
		catch(const std::exception &e)
		{
			out << fmt::snstringf
			{
				128, "Error at i=%zu j=%zu r=%zu foff=%zu boff=%zu\n",
				i, j, r, foff, boff
			};

			out << string_view{object} << std::endl;
			out << e.what() << std::endl;
			return true;
		}

		foff += boff;
	}

	out << "Executed " << i
	    << " of " << j << " events"
	    << " in " << foff << " bytes"
	    << " using " << r << " reads"
	    << std::endl;

	return true;
}

//
// room
//

static bool console_cmd__room__id(const string_view &line);
static bool console_cmd__room__redact(const string_view &line);
static bool console_cmd__room__message(const string_view &line);
static bool console_cmd__room__set(const string_view &line);
static bool console_cmd__room__get(const string_view &line);
static bool console_cmd__room__origins(const string_view &line);
static bool console_cmd__room__messages(const string_view &line);
static bool console_cmd__room__members(const string_view &line);
static bool console_cmd__room__count(const string_view &line);
static bool console_cmd__room__state(const string_view &line);
static bool console_cmd__room__depth(const string_view &line);
static bool console_cmd__room__head(const string_view &line);

bool
console_cmd__room(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("depth"):
			return console_cmd__room__depth(args);

		case hash("head"):
			return console_cmd__room__head(args);

		case hash("state"):
			return console_cmd__room__state(args);

		case hash("count"):
			return console_cmd__room__count(args);

		case hash("origins"):
			return console_cmd__room__origins(args);

		case hash("members"):
			return console_cmd__room__members(args);

		case hash("messages"):
			return console_cmd__room__messages(args);

		case hash("get"):
			return console_cmd__room__get(args);

		case hash("set"):
			return console_cmd__room__set(args);

		case hash("message"):
			return console_cmd__room__message(args);

		case hash("redact"):
			return console_cmd__room__redact(args);

		case hash("id"):
			return console_cmd__room__id(args);

		default:
			throw bad_command{};
	}

	return true;
}

bool
console_cmd__room__head(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << head(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__depth(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << depth(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__members(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view membership
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : string_view{}
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const auto closure{[](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	}};

	if(membership)
		members.for_each(membership, closure);
	else
		members.for_each(closure);

	return true;
}

bool
console_cmd__room__origins(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	origins.test([](const string_view &origin)
	{
		out << origin << std::endl;
		return false;
	});

	return true;
}

bool
console_cmd__room__state(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const auto &event_id
	{
		token(line, ' ', 1, {})
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	state.for_each([](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__count(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view &type
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : string_view{}
	};

	const m::room room
	{
		room_id
	};

	const m::room::state state
	{
		room
	};

	if(type)
		out << state.count(type) << std::endl;
	else
		out << state.count() << std::endl;

	return true;
}

bool
console_cmd__room__messages(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const int64_t depth
	{
		token_count(line, ' ') > 1? lex_cast<int64_t>(token(line, ' ', 1)) : -1
	};

	const char order
	{
		token_count(line, ' ') > 2? token(line, ' ', 2).at(0) : 'b'
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it{room};
	if(depth >= 0)
		it.seek(depth);

	for(; it; order == 'b'? --it : ++it)
		out << pretty_oneline(*it) << std::endl;

	return true;
}

bool
console_cmd__room__get(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	room.get(type, state_key, [](const m::event &event)
	{
		out << pretty(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__set(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view type
	{
		token(line, ' ', 2)
	};

	const string_view state_key
	{
		token(line, ' ', 3)
	};

	const json::object &content
	{
		token(line, ' ', 4)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		send(room, sender, type, state_key, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__message(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view body
	{
		tokens_after(line, ' ', 1)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		message(room, sender, body)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__redact(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::event::id &redacts
	{
		token(line, ' ', 1)
	};

	const m::user::id &sender
	{
		token(line, ' ', 2)
	};

	const string_view reason
	{
		tokens_after(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		redact(room, sender, redacts, reason)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__id(const string_view &id)
{
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::USER:
			out << m::user{id}.room_id() << std::endl;
			break;

		case m::id::NODE:
			out << m::node{id}.room_id() << std::endl;
			break;

		default:
			break;
	}

	return true;
}

//
// fed
//

static bool console_cmd__fed__version(const string_view &line);
static bool console_cmd__fed__query(const string_view &line);
static bool console_cmd__fed__event(const string_view &line);
static bool console_cmd__fed__state(const string_view &line);

bool
console_cmd__fed(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("version"):
			return console_cmd__fed__version(args);

		case hash("query"):
			return console_cmd__fed__query(args);

		case hash("event"):
			return console_cmd__fed__event(args);

		case hash("state"):
			return console_cmd__fed__state(args);

		default:
			throw bad_command{};
	}

	return true;
}

bool
console_cmd__fed__state(const string_view &line)
{
	const m::room::id &room_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1)
	};

	const string_view &event_id
	{
		token_count(line, ' ') >= 3? token(line, ' ', 2) : string_view{}
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__event(const string_view &line)
{
	const m::event::id &event_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 0) : event_id.host()
	};

	m::v1::event::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << pretty(m::event{response}) << std::endl;
	return true;
}

static bool console_cmd__fed__query__directory(const string_view &line);
static bool console_cmd__fed__query__profile(const string_view &line);

bool
console_cmd__fed__query(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("profile"):
			return console_cmd__fed__query__profile(args);

		case hash("directory"):
			return console_cmd__fed__query__directory(args);

		default:
			throw bad_command{};
	}

	return true;
}

bool
console_cmd__fed__query__profile(const string_view &line)
{
	const m::user::id &user_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : user_id.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::profile request
	{
		user_id, buf, std::move(opts)
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__query__directory(const string_view &line)
{
	const m::id::room_alias &room_alias
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : room_alias.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::directory request
	{
		room_alias, buf, std::move(opts)
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__version(const string_view &line)
{
	const net::hostport remote
	{
		token(line, ' ', 0)
	};

	m::v1::version::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::version request
	{
		buf, std::move(opts)
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}
