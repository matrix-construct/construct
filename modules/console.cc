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

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

static void init_cmds();

mapi::header
IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable self-reflecting command library.", []
	{
		init_cmds();
	}
};

/// The first parameter for all commands. This aggregates general options
/// passed to commands as well as providing the output facility with an
/// ostream interface. Commands should only send output to this object. The
/// command's input line is not included here; it's the second param to a cmd.
struct opt
{
	std::ostream &out;
	bool html {false};
	seconds timeout {30};

	operator std::ostream &()
	{
		return out;
	}

	template<class T> auto &operator<<(T&& t)
	{
		out << std::forward<T>(t);
		return out;
	}

	auto &operator<<(std::ostream &(*manip)(std::ostream &))
	{
		return manip(out);
	}
};

/// Instances of this object are generated when this module reads its
/// symbols to find commands. These instances are then stored in the
/// cmds set for lookup and iteration.
struct cmd
{
	using is_transparent = void;

	static constexpr const size_t &PATH_MAX
	{
		8
	};

	std::string name;
	std::string symbol;
	mods::sym_ptr ptr;

	bool operator()(const cmd &a, const cmd &b) const
	{
		return a.name < b.name;
	}

	bool operator()(const string_view &a, const cmd &b) const
	{
		return a < b.name;
	}

	bool operator()(const cmd &a, const string_view &b) const
	{
		return a.name < b;
	}

	cmd(std::string name, std::string symbol)
	:name{std::move(name)}
	,symbol{std::move(symbol)}
	,ptr{IRCD_MODULE, this->symbol}
	{}

	cmd() = default;
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
};

std::set<cmd, cmd>
cmds;

void
init_cmds()
{
	auto symbols
	{
		mods::symbols(mods::path(IRCD_MODULE))
	};

	for(std::string &symbol : symbols)
	{
		// elide lots of grief by informally finding this first
		if(!has(symbol, "console_cmd"))
			continue;

		thread_local char buf[1024];
		const string_view demangled
		{
			demangle(buf, symbol)
		};

		std::string command
		{
			replace(between(demangled, "__", "("), "__", " ")
		};

		const auto iit
		{
			cmds.emplace(std::move(command), std::move(symbol))
		};

		if(!iit.second)
			throw error
			{
				"Command '%s' already exists", command
			};
	}
}

const cmd *
find_cmd(const string_view &line)
{
	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		const auto it{cmds.lower_bound(name)};
		if(it == end(cmds) || it->name != name)
			continue;

		return &(*it);
	}

	return nullptr;
}

//
// Main command dispatch
//

int console_command_derived(opt &, const string_view &line);

/// This function may be linked and called by those wishing to execute a
/// command. Output from the command will be appended to the provided ostream.
/// The input to the command is passed in `line`. Since `struct opt` is not
/// accessible outside of this module, all public options are passed via a
/// plaintext string which is parsed here.
extern "C" int
console_command(std::ostream &out,
                const string_view &line,
                const string_view &opts)
try
{
	opt opt
	{
		out,
		has(opts, "html")
	};

	const cmd *const cmd
	{
		find_cmd(line)
	};

	if(!cmd)
		return console_command_derived(opt, line);

	const auto args
	{
		lstrip(split(line, cmd->name).second, ' ')
	};

	const auto &ptr{cmd->ptr};
	using prototype = bool (struct opt &, const string_view &);
	return ptr.operator()<prototype>(opt, args);
}
catch(const params::error &e)
{
	out << e.what() << std::endl;
	return true;
}
catch(const bad_command &e)
{
	return -2;
}

//
// Help
//

bool
console_cmd__help(opt &out, const string_view &line)
{
	const auto cmd
	{
		find_cmd(line)
	};

	if(cmd)
	{
		out << "No help available for '" << cmd->name << "'."
		    << std::endl;

		//TODO: help string symbol map
	}

	out << "Commands available: \n"
	    << std::endl;

	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		string_view last;
		auto it{cmds.lower_bound(name)};
		if(it == end(cmds))
			continue;

		for(; it != end(cmds); ++it)
		{
			if(!startswith(it->name, name))
				break;

			const auto prefix
			{
				tokens_before(it->name, ' ', e)
			};

			if(last == prefix)
				continue;

			last = prefix;
			const auto suffix
			{
				e > 1? tokens_after(prefix, ' ', e - 2) : prefix
			};

			if(empty(suffix))
				continue;

			out << suffix << std::endl;
		}

		break;
	}

	return true;
}

//
// Test trigger stub
//

bool
console_cmd__test(opt &out, const string_view &line)
{
	return true;
}

//
// Derived commands
//

bool console_id__user(const m::user::id &id, const string_view &args);
bool console_id__room(const m::room::id &id, const string_view &args);
bool console_id__event(const m::event::id &id, const string_view &args);
bool console_json(const json::object &);

int
console_command_derived(opt &out, const string_view &line)
{
	const string_view id{token(line, ' ', 0)};
	const string_view args{tokens_after(line, ' ', 0)};
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

	return -1;
}

//
// Command by JSON
//

bool
console_json(const json::object &object)
{
	if(!object.has("type"))
		return true;

	//return console_cmd__exec__event(object);
	return true;
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
// misc
//


bool
console_cmd__debug(opt &out, const string_view &line)
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
// conf
//

bool
console_cmd__conf__list(opt &out, const string_view &line)
{
	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
		out
		<< std::setw(48) << std::right << item.first
		<< " = " << item.second->get(val)
		<< std::endl;

	return true;
}

//
// mod
//

bool
console_cmd__mod(opt &out, const string_view &line)
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
			mods::loaded(mod)? "\033[1;32;42m+\033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod__path(opt &out, const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod__syms(opt &out, const string_view &line)
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
console_cmd__mod__reload(opt &out, const string_view &line)
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
	out << "reload " << name << std::endl;
	return true;
}

bool
console_cmd__mod__load(opt &out, const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(m::modules.find(name) != end(m::modules))
	{
		out << name << " is already loaded." << std::endl;
		return true;
	}

	m::modules.emplace(name, name);
	return true;
}

bool
console_cmd__mod__unload(opt &out, const string_view &line)
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

	out << "unloaded " << name << std::endl;
	return true;
}

//
// db
//

bool
console_cmd__db__sync(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	sync(database);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__flush(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[blocking]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto blocking
	{
		param.at(1, false)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	flush(database, blocking);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__compact(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[colname]", "[begin]", "[end]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const auto begin
	{
		param[2]
	};

	const auto end
	{
		param[3]
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	if(!colname)
	{
		compact(database);
		out << "done" << std::endl;
		return true;
	}

	db::column column
	{
		database, colname
	};

	compact(column, begin, end);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__ticker(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[ticker]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto ticker
	{
		param[1]
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(!empty(ticker))
	{
		out << ticker << ": " << db::ticker(database, ticker) << std::endl;
		return true;
	}

	for(uint32_t i(0); i < db::ticker_max; ++i)
	{
		const string_view &name
		{
			db::ticker_id(i)
		};

		if(!name)
			continue;

		out << std::setw(48) << std::right << name
		    << "  " << db::ticker(database, i)
		    << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__prop(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "property"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto property
	{
		param.at(2)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(colname == "*")
	{
		const uint64_t value
		{
			db::property(database, property)
		};

		out << value << std::endl;
		return true;
	}

	const auto query{[&out, &database, &property]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::property<db::prop_map>(column, property)
		};

		for(const auto &p : value)
			out << p.first << " : " << p.second << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		query(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &column_name : database.column_names)
	{
		out << std::setw(16) << std::right << column_name << " : ";
		query(column_name);
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__files(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	uint64_t msz;
	const auto files
	{
		db::files(database, msz)
	};

	for(const auto &file : files)
		out << file << std::endl;

	out << "-- " << files.size() << " files; "
	    << "manifest is " << msz << " bytes.";

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txns(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto limit
	{
		lex_cast<size_t>(token(line, ' ', 2, "32"))
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	for_each(database, seqnum, db::seq_closure_bool{[&out, &limit]
	(db::txn &txn, const uint64_t &seqnum) -> bool
	{
		m::event::id::buf event_id;
		txn.get(db::op::SET, "event_id", [&event_id]
		(const db::delta &delta)
		{
			event_id = std::get<delta.KEY>(delta);
		});

		if(event_id)
			return true;

		out << std::setw(12) << std::right << seqnum << " : "
		    << string_view{event_id}
		    << std::endl;

		return --limit;
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txn(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	get(database, seqnum, db::seq_closure{[&out]
	(db::txn &txn, const uint64_t &seqnum)
	{
		for_each(txn, [&out, &seqnum]
		(const db::delta &delta)
		{
			out << std::setw(12)  << std::right  << seqnum << " : "
			    << std::setw(8)   << std::left   << reflect(std::get<delta.OP>(delta)) << " "
			    << std::setw(18)  << std::right  << std::get<delta.COL>(delta) << " "
			    << std::get<delta.KEY>(delta)
			    << std::endl;
		});
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__checkpoint(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	const auto directory
	{
		token(line, ' ', 1)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	checkpoint(database, directory);

	out << "Checkpoint " << name(database)
	    << " to " << directory << " complete."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__list(opt &out, const string_view &line)
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
// peer
//

static bool
html__peer(opt &out, const string_view &line)
{
	out << "<table>";

	out << "<tr>";
	out << "<td> HOST </td>";
	out << "<td> ADDR </td>";
	out << "<td> LINKS </td>";
	out << "<td> REQS </td>";
	out << "<td> ▲ BYTES Q</td>";
	out << "<td> ▼ BYTES Q</td>";
	out << "<td> ▲ BYTES</td>";
	out << "<td> ▼ BYTES</td>";
	out << "<td> ERROR </td>";
	out << "</tr>";

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << "<tr>";

		out << "<td>" << host << "</td>";
		out << "<td>" << ipp << "</td>";

		out << "<td>" << peer.link_count() << "</td>";
		out << "<td>" << peer.tag_count() << "</td>";
		out << "<td>" << peer.write_size() << "</td>";
		out << "<td>" << peer.read_size() << "</td>";
		out << "<td>" << peer.write_total() << "</td>";
		out << "<td>" << peer.read_total() << "</td>";

		out << "<td>";
		if(peer.err_has() && peer.err_msg())
			out << peer.err_msg();
		else if(peer.err_has())
			out << "<unknown error>"_sv;
		out << "</td>";

		out << "</tr>";
	}

	out << "</table>";
	return true;
}

bool
console_cmd__peer(opt &out, const string_view &line)
{
	if(out.html)
		return html__peer(out, line);

	const bool all
	{
		has(line, "all")
	};

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		if(peer.err_has() && !all)
			continue;

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << " " << setw(2) << right << peer.link_count()   << " L"
		    << " " << setw(2) << right << peer.tag_count()    << " T"
		    << " " << setw(9) << right << peer.write_size()   << " UP Q"
		    << " " << setw(9) << right << peer.read_size()    << " DN Q"
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

bool
console_cmd__peer__error(opt &out, const string_view &line)
{
	for(const auto &pair : ircd::server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{pair.first};
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(!peer.err_has())
			continue;

		const net::ipport &ipp{peer.remote};
		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << peer.e->etime;

		if(peer.err_msg())
			out << "  :" << peer.err_msg();
		else
			out << "  <unknown error>"_sv;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__peer__error__clear__all(opt &out, const string_view &line)
{
	size_t cleared(0);
	for(auto &pair : ircd::server::peers)
	{
		const auto &name{pair.first};
		assert(bool(pair.second));
		auto &peer{*pair.second};
		cleared += peer.err_clear();
	}

	out << "cleared " << cleared
	    << " of " << ircd::server::peers.size()
	    << std::endl;

	return true;
}

bool
console_cmd__peer__error__clear(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__peer__error__clear__all(out, line);

	const net::hostport hp
	{
		token(line, ' ', 0)
	};

	const auto cleared
	{
		server::errclear(hp)
	};

	out << std::boolalpha << cleared << std::endl;
	return true;
}

bool
console_cmd__peer__version(opt &out, const string_view &line)
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

		if(!empty(peer.server_name))
			out << " :" << peer.server_name;

		out << std::endl;
	}

	return true;
}

//
// net
//

bool
console_cmd__net__host(opt &out, const string_view &line)
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

bool
console_cmd__net__host__cache(opt &out, const string_view &line)
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
				out << std::setw(48) << std::right << host
				    << "  =>  " << std::setw(21) << std::left << ipp
				    << "  expires " << timestr(record.ttl, ircd::localtime)
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

				out << std::setw(48) << std::right << key
				    << "  =>  " << std::setw(48) << std::left << hostport
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

//
// client
//

bool
console_cmd__client(opt &out, const string_view &line)
{
	using std::setw;
	using std::left;
	using std::right;

	for(const auto *const &client : ircd::client::list)
	{
		out << setw(8) << left << client->id
		    << "  " << right << setw(22) << local(*client)
		    << "  " << left << setw(22) << remote(*client)
		    ;

		if(bool(client->sock))
		{
			const auto stat
			{
				net::bytes(*client->sock)
			};

			out << " | UP " << setw(8) << right << stat.second
			    << " | DN " << setw(8) << right << stat.first
			    << " |";
		}

		if(client->reqctx)
			out << " CTX " << setw(4) << id(*client->reqctx);

		if(client->longpoll)
			out << " POLL";

		if(client->request.user_id)
			out << " USER " << client->request.user_id;

		if(client->request.origin)
			out << " PEER " << client->request.origin;

		if(client->request.head.method)
			out << " " << client->request.head.method << ""
			    << " " << client->request.head.path;

		out << std::endl;
	}

	return true;
}

//
// key
//

bool
console_cmd__key(opt &out, const string_view &line)
{
	out << "origin:                  " << m::my_host() << std::endl;
	out << "public key ID:           " << m::self::public_key_id << std::endl;
	out << "public key base64:       " << m::self::public_key_b64 << std::endl;
	out << "TLS cert sha256 base64:  " << m::self::tls_cert_der_sha256_b64 << std::endl;

	return true;
}

bool
console_cmd__key__get(opt &out, const string_view &line)
{
	const auto server_name
	{
		token(line, ' ', 0)
	};

	m::keys::get(server_name, [&out]
	(const auto &keys)
	{
		out << keys << std::endl;
	});

	return true;
}

bool
console_cmd__key__fetch(opt &out, const string_view &line)
{

	return true;
}

//
// vm
//

bool
console_cmd__vm__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"start", "[limit]"
	}};

	const uint64_t start
	{
		param.at<uint64_t>(0, uint64_t(-1))
	};

	size_t limit
	{
		param.at<size_t>(1, 32)
	};

	m::vm::events::rfor_each(start, [&out, &limit]
	(const uint64_t &seq, const m::event &event)
	{
		out << seq << " " << pretty_oneline(event) << std::endl;;
		return --limit;
	});

	return true;
}

//
// event
//

bool
console_cmd__event(opt &out, const string_view &line)
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

bool
console_cmd__event__erase(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	m::dbs::write(txn, event, opts);
	txn();

	out << "erased " << txn.size() << " cells"
	    << " for " << event_id << std::endl;

	return true;
}

bool
console_cmd__event__dump(opt &out, const string_view &line)
{
	const auto filename
	{
		token(line, ' ', 0)
	};

	db::column column
	{
		*m::dbs::events, "event_id"
	};

	db::gopts gopts
	{
		db::get::NO_CACHE
	};

	gopts.snapshot = db::database::snapshot
	{
		*m::dbs::events
	};

	const auto etotal
	{
		db::property<uint64_t>(column, "rocksdb.estimate-num-keys")
	};

	const unique_buffer<mutable_buffer> buf
	{
		512_KiB
	};

	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::event::fetch event;
	char *pos{data(buf)};
	for(auto it(column.begin(gopts)); bool(it); ++it, ++ecount)
	{
		const auto remain
		{
			size_t(data(buf) + size(buf) - pos)
		};

		assert(remain >= 64_KiB && remain <= size(buf));
		const mutable_buffer mb{pos, remain};
		const string_view event_id{it->second};
		seek(event, event_id, std::nothrow);
		if(unlikely(!event.valid(event_id)))
		{
			log::error
			{
				"dump[%s] @ %zu of %zu (est): Failed to fetch %s from database",
				filename,
				ecount,
				etotal,
				event_id
			};

			++errcount;
			continue;
		}

		pos += json::print(mb, event);
		if(pos + 64_KiB > data(buf) + size(buf))
		{
			const const_buffer cb{data(buf), pos};
			foff += size(fs::append(filename, cb));
			pos = data(buf);
			++acount;

			const float pct
			{
				(ecount / float(etotal)) * 100.0f
			};

			log::info
			{
				"dump[%s] %lf$%c @ %zu of %zu (est) events; %zu bytes; %zu writes; %zu errors",
				filename,
				pct,
				'%', //TODO: fix gram
				ecount,
				etotal,
				foff,
				acount,
				errcount
			};
		}
	}

	if(pos > data(buf))
	{
		const const_buffer cb{data(buf), pos};
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
console_cmd__event__fetch(opt &out, const string_view &line)
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

	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
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

//
// state
//

bool
console_cmd__state__count(opt &out, const string_view &line)
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
console_cmd__state__each(opt &out, const string_view &line)
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

	m::state::for_each(root, type, [&out]
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
	});

	return true;
}

bool
console_cmd__state__get(opt &out, const string_view &line)
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

	m::state::get(root, type, state_key, [&out]
	(const auto &value)
	{
		out << "got: " << value << std::endl;
	});

	return true;
}

bool
console_cmd__state__dfs(opt &out, const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	m::state::dfs(root, [&out]
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
console_cmd__state__root(opt &out, const string_view &line)
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
// commit
//

bool
console_cmd__commit(opt &out, const string_view &line)
{
	m::event event
	{
		json::object{line}
	};

	return true;
}

//
// eval
//

bool
console_cmd__eval(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "opts",
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const auto &args
	{
		param[1]
	};

	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	const m::event event
	{
		event_id, buf
	};

	m::vm::opts opts;
	opts.errorlog = 0;
	opts.warnlog = 0;
	opts.nothrows = 0;
	opts.non_conform |= m::event::conforms::MISSING_PREV_STATE;

	tokens(args, ' ', [&opts](const auto &arg)
	{
		switch(hash(arg))
		{
			case "replay"_:
				opts.replays = true;
				break;

			case "noverify"_:
				opts.verify = false;
				break;
		}
	});

	m::vm::eval eval{opts};
	out << pretty(event) << std::endl;
	eval(event);
	out << "done" << std::endl;
	return true;
}

bool
console_cmd__eval__file(opt &out, const string_view &line)
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
	opts.verify = false;
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
// rooms
//

bool
console_cmd__rooms(opt &out, const string_view &line)
{
	m::rooms::for_each(m::room::id::closure{[&out]
	(const m::room::id &room_id)
	{
		out << room_id << std::endl;
	}});

	return true;
}

//
// room
//

bool
console_cmd__room__head(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	out << head(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__depth(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	out << depth(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__members(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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

	const auto closure{[&out](const m::event &event)
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
console_cmd__room__origins(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	origins.test([&out](const string_view &origin)
	{
		out << origin << std::endl;
		return false;
	});

	return true;
}

bool
console_cmd__room__state(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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

	state.for_each([&out](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__count(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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
console_cmd__room__messages(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "depth|-limit", "order", "limit"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const int64_t depth
	{
		param.at<int64_t>(1, std::numeric_limits<int64_t>::max())
	};

	const char order
	{
		param.at(2, "b"_sv).at(0)
	};

	ssize_t limit
	{
		depth < 0?
			std::abs(depth):
			param.at(3, ssize_t(32))
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it{room};
	if(depth >= 0 && depth < std::numeric_limits<int64_t>::max())
		it.seek(depth);

	for(; it && limit >= 0; order == 'b'? --it : ++it, --limit)
		out << pretty_oneline(*it) << std::endl;

	return true;
}

bool
console_cmd__room__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "state_key", "args"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view type
	{
		param.at(1)
	};

	const string_view state_key
	{
		param.at(2)
	};

	const string_view arg
	{
		param[3]
	};

	const m::room::state room
	{
		room_id
	};

	room.get(type, state_key, [&out, &arg]
	(const m::event &event)
	{
		if(has(arg, "raw"))
			out << event << std::endl;
		else if(has(arg, "content"))
			out << json::get<"content"_>(event) << std::endl;
		else
			out << pretty(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__set(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"room_id", "sender", "type", "state_key", "content"
		}
	};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &sender
	{
		param.at(1)
	};

	const string_view type
	{
		param.at(2)
	};

	const string_view state_key
	{
		param.at(3)
	};

	const json::object &content
	{
		param.at(4, json::object{})
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
console_cmd__room__message(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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
console_cmd__room__redact(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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
console_cmd__room__join(opt &out, const string_view &line)
{
	const string_view room_id_or_alias
	{
		token(line, ' ', 0)
	};

	const m::user::id &user_id
	{
		token(line, ' ', 1)
	};

	const string_view &event_id
	{
		token(line, ' ', 2, {})
	};

	switch(m::sigil(room_id_or_alias))
	{
		case m::id::ROOM:
		{
			const m::room room
			{
				room_id_or_alias, event_id
			};

			const auto join_event
			{
				m::join(room, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		case m::id::ROOM_ALIAS:
		{
			const m::room::alias alias
			{
				room_id_or_alias
			};

			const auto join_event
			{
				m::join(alias, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		default: throw error
		{
			"Don't know how to join '%s'", room_id_or_alias
		};
	}

	return true;
}

bool
console_cmd__room__id(opt &out, const string_view &id)
{
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::USER:
			out << m::user{id}.room_id() << std::endl;
			break;

		case m::id::NODE:
			out << m::node{id}.room_id() << std::endl;
			break;

		case m::id::ROOM_ALIAS:
			out << m::room_id(m::room::alias(id)) << std::endl;
			break;

		default:
			break;
	}

	return true;
}

bool
console_cmd__room__purge(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	return true;
}

//
// user
//

bool
console_cmd__user__password(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id", "password"
		}
	};

	m::user user
	{
		param.at(0)
	};

	const string_view &password
	{
		param.at(1)
	};

	const auto eid
	{
		user.password(password)
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__active(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	const m::user user
	{
		param.at(0)
	};

	out << user.user_id << " is "
	    << (user.is_active()? "active" : "inactive")
	    << std::endl;

	return true;
}

bool
console_cmd__user__activate(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	m::user user
	{
		param.at(0)
	};

	if(user.is_active())
	{
		out << user.user_id << " is already active" << std::endl;
		return true;
	}

	const auto eid
	{
		user.activate()
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__deactivate(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	m::user user
	{
		param.at(0)
	};

	if(!user.is_active())
	{
		out << user.user_id << " is already inactive" << std::endl;
		return true;
	}

	const auto eid
	{
		user.deactivate()
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__presence(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "limit"
	}};

	const m::user user
	{
		param.at(0)
	};

	auto limit
	{
		param.at(1, size_t(16))
	};

	const m::user::room user_room{user};
	user_room.for_each("m.presence", m::event::closure_bool{[&out, &limit]
	(const m::event &event)
	{
		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"content"_>(event)
		    << " " << at<"event_id"_>(event)
		    << std::endl;

		return --limit > 0;
	}});

	return true;
}

bool
console_cmd__user__rooms(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[membership]"
	}};

	const m::user &user
	{
		param.at(0)
	};

	const string_view &membership
	{
		param[1]
	};

	const m::user::rooms rooms
	{
		user
	};

	rooms.for_each(membership, m::user::rooms::closure{[&out]
	(const m::room &room, const string_view &membership)
	{
		out << room.room_id
		    << " " << membership
		    << std::endl;
	}});

	return true;
}

bool
console_cmd__user__read(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id",
	}};

	const m::user user
	{
		param.at(0)
	};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	state.for_each("m.read", m::event::closure{[&out]
	(const m::event &event)
	{
		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"state_key"_>(event)
		    << " " << at<"content"_>(event)
		    << " " << at<"event_id"_>(event)
		    << std::endl;
	}});

	return true;
}

//
// feds
//

bool
console_cmd__feds__version(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto room
	{
		m::room_id(param.at(0))
	};

	struct req
	:m::v1::version
	{
		char origin[256];
		char buf[16_KiB];

		req(m::v1::version::opts &&opts)
		:m::v1::version{buf, std::move(opts)}
		{}
	};

	std::list<req> reqs;
	const m::room::origins origins{room};
	origins.for_each([&out, &reqs]
	(const string_view &origin)
	{
		m::v1::version::opts opts;
		opts.remote = origin;
		opts.dynamic = false; try
		{
			reqs.emplace_back(std::move(opts));
		}
		catch(const std::exception &e)
		{
			out << "! " << origin << " " << e.what() << std::endl;
			return;
		}

		strlcpy(reqs.back().origin, origin);
	});

	auto all
	{
		ctx::when_all(begin(reqs), end(reqs))
	};

	all.wait(out.timeout, std::nothrow);

	for(auto &req : reqs) try
	{
		if(req.wait(0ms, std::nothrow))
		{
			const auto code{req.get()};
			const json::object &response{req};
			out << "+ " << std::setw(40) << std::left << req.origin
			    << " " << string_view{response}
			    << std::endl;
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << std::setw(40) << std::left << req.origin
		    << " " << e.what()
		    << std::endl;
	}

	return true;
}

bool
console_cmd__feds__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto room
	{
		m::room_id(param.at(0))
	};

	const m::event::id event_id
	{
		param.at(1)
	};

	struct req
	:m::v1::event
	{
		char origin[256];
		char buf[96_KiB];

		req(const m::event::id &event_id, m::v1::event::opts opts)
		:m::v1::event{event_id, buf, std::move(opts)}
		{}
	};

	std::list<req> reqs;
	const m::room::origins origins{room};
	origins.for_each([&out, &event_id, &reqs]
	(const string_view &origin)
	{
		m::v1::event::opts opts;
		opts.remote = origin;
		opts.dynamic = false; try
		{
			reqs.emplace_back(event_id, std::move(opts));
		}
		catch(const std::exception &e)
		{
			out << "! " << origin << " " << e.what() << std::endl;
			return;
		}

		strlcpy(reqs.back().origin, origin);
	});

	auto all
	{
		ctx::when_all(begin(reqs), end(reqs))
	};

	all.wait(out.timeout, std::nothrow);

	for(auto &req : reqs) try
	{
		if(req.wait(0ms, std::nothrow))
		{
			const auto code{req.get()};
			out << "+ " << req.origin << " " << http::status(code) << std::endl;
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << req.origin << " " << e.what() << std::endl;
	}

	return true;
}

//
// fed
//

bool
console_cmd__fed__groups(opt &out, const string_view &line)
{
	const m::id::node &node
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	m::user::id ids[8];
	string_view tok[8];
	const auto count{std::min(tokens(args, ' ', tok), size_t(8))};
	std::copy(begin(tok), begin(tok) + count, begin(ids));

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::groups::publicised::opts opts;
	m::v1::groups::publicised request
	{
		node, vector_view<const m::user::id>(ids, count), buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request.in.content
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__head(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "user_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	const m::user::id &user_id
	{
		param.at(2, m::me.user_id)
	};

	thread_local char buf[16_KiB];
	m::v1::make_join::opts opts;
	opts.remote = remote;
	m::v1::make_join request
	{
		room_id, m::me.user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object proto
	{
		request.in.content
	};

	const json::array prev_events
	{
		proto.at({"event", "prev_events"})
	};

	for(const json::array &prev_event : prev_events)
	{
		const string_view &id{prev_event.at(0)};
		out << id << " :" << string_view{prev_event.at(1)} << std::endl;
	}

	return true;
}

bool
console_cmd__fed__send(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "event_id",
	}};

	const net::hostport remote
	{
		param.at(0)
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const m::event::fetch event
	{
		event_id
	};

	thread_local char pdubuf[64_KiB];
	const json::value pdu
	{
		json::stringify(mutable_buffer{pdubuf}, event)
	};

	const vector_view<const json::value> pdus
	{
		&pdu, &pdu + 1
	};

	const auto txn
	{
		m::txn::create(pdus)
	};

	thread_local char idbuf[128];
	const auto txnid
	{
		m::txn::create_id(idbuf, txn)
	};

	const unique_buffer<mutable_buffer> bufs
	{
		16_KiB
	};

	m::v1::send::opts opts;
	opts.remote = remote;
	m::v1::send request
	{
		txnid, const_buffer{txn}, bufs, std::move(opts)
	};

	request.wait(out.timeout);

	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const m::v1::send::response resp
	{
		response
	};

	resp.for_each_pdu([&]
	(const m::event::id &event_id, const json::object &error)
	{
		out << remote << " ->" << txnid << " " << event_id << " ";
		if(empty(error))
			out << http::status(code) << std::endl;
		else
			out << string_view{error} << std::endl;
	});

	return true;
}

bool
console_cmd__fed__sync(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "limit", "event_id", "timeout"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	const auto &limit
	{
		param.at(2, size_t(128))
	};

	const string_view &event_id
	{
		param[3]
	};

	const auto timeout
	{
		param.at(4, out.timeout)
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::state::opts stopts;
	stopts.remote = remote;
	stopts.event_id = event_id;
	const mutable_buffer stbuf
	{
		data(buf), size(buf) / 2
	};

	m::v1::state strequest
	{
		room_id, stbuf, std::move(stopts)
	};

	m::v1::backfill::opts bfopts;
	bfopts.remote = remote;
	bfopts.event_id = event_id;
	bfopts.limit = limit;
	const mutable_buffer bfbuf
	{
		buf + size(stbuf)
	};

	m::v1::backfill bfrequest
	{
		room_id, bfbuf, std::move(bfopts)
	};

	const auto when
	{
		now<steady_point>() + timeout
	};

	bfrequest.wait_until(when);
	strequest.wait_until(when);

	bfrequest.get();
	strequest.get();

	const json::array &auth_chain
	{
		json::object{strequest}.get("auth_chain")
	};

	const json::array &pdus
	{
		json::object{strequest}.get("pdus")
	};

	const json::array &messages
	{
		json::object{bfrequest}.get("pdus")
	};

	std::vector<m::event> events;
	events.reserve(auth_chain.size() + pdus.size() + messages.size());

	for(const json::object &event : auth_chain)
		events.emplace_back(event);

	for(const json::object &event : pdus)
		events.emplace_back(event);

	for(const json::object &event : messages)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.head_must_exist = false;
	vmopts.history = false;
	vmopts.notify = false;
	vmopts.debuglog_accept = true;
	vmopts.nothrows = -1;
	m::vm::eval eval
	{
		vmopts
	};

	for(const auto &event : events)
		eval(event);

	return true;
}

bool
console_cmd__fed__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "event_id|op", "op"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	string_view event_id
	{
		param[2]
	};

	string_view op
	{
		param[3]
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		for(const json::object &event : auth_chain)
			out << pretty_oneline(m::event{event}) << std::endl;

		for(const json::object &event : pdus)
			out << pretty_oneline(m::event{event}) << std::endl;

		return true;
	}

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.head_must_exist = false;
	vmopts.history = false;
	vmopts.notify = false;
	m::vm::eval eval
	{
		vmopts
	};

	for(const json::object &event : auth_chain)
		eval(event);

	for(const json::object &event : pdus)
		eval(event);

	return true;
}

bool
console_cmd__fed__state_ids(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	string_view event_id
	{
		param[2]
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	opts.ids_only = true;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const json::array &auth_chain
	{
		response["auth_chain_ids"]
	};

	const json::array &pdus
	{
		response["pdu_ids"]
	};

	for(const string_view &event_id : auth_chain)
		out << unquote(event_id) << std::endl;

	for(const string_view &event_id : pdus)
		out << unquote(event_id) << std::endl;

	return true;
}

bool
console_cmd__fed__backfill(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const net::hostport remote
	{
		token(line, ' ', 1)
	};

	const string_view &count
	{
		token(line, ' ', 2, "32")
	};

	string_view event_id
	{
		token(line, ' ', 3, {})
	};

	string_view op
	{
		token(line, ' ', 4, {})
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[16_KiB];
	m::v1::backfill::opts opts;
	opts.remote = remote;
	opts.limit = lex_cast<size_t>(count);
	if(event_id)
		opts.event_id = event_id;

	m::v1::backfill request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		for(const json::object &event : pdus)
			out << pretty_oneline(m::event{event}) << std::endl;

		return true;
	}

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.head_must_exist = false;
	vmopts.history = false;
	vmopts.notify = false;
	m::vm::eval eval
	{
		vmopts
	};

	for(const json::object &event : pdus)
		eval(event);

	return true;
}

bool
console_cmd__fed__event(opt &out, const string_view &line)
{
	const m::event::id &event_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, event_id.host())
	};

	const string_view op
	{
		token(line, ' ', 2, {})
	};

	m::v1::event::opts opts;
	opts.remote = remote;
	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const m::event event
	{
		response
	};

	out << pretty(event) << std::endl;

	if(!verify(event))
		out << "- SIGNATURE FAILED" << std::endl;

	if(!verify_hash(event))
		out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	if(has(op, "raw"))
		out << string_view{response} << std::endl;

	if(!has(op, "eval"))
		return true;

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.head_must_exist = false;
	vmopts.history = false;
	vmopts.notify = false;
	m::vm::eval eval
	{
		vmopts
	};

	eval(event);
	return true;
}

bool
console_cmd__fed__public_rooms(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "limit", "all_networks", "3pid"
	}};

	const net::hostport remote
	{
		param.at(0)
	};

	const auto limit
	{
		param.at(1, 32)
	};

	const auto all_nets
	{
		param.at(2, false)
	};

	const auto tpid
	{
		param[3]
	};

	m::v1::public_rooms::opts opts;
	opts.limit = limit;
	opts.third_party_instance_id = tpid;
	opts.include_all_networks = all_nets;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::public_rooms request
	{
		remote, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const auto total_estimate
	{
		response.get<size_t>("total_room_count_estimate")
	};

	const auto next_batch
	{
		unquote(response.get("next_batch"))
	};

	const json::array &rooms
	{
		response.get("chunk")
	};

	for(const json::object &summary : rooms)
	{
		for(const auto &member : summary)
			out << std::setw(24) << member.first << " => " << member.second << std::endl;

		out << std::endl;
	}

	out << "total: " << total_estimate << std::endl;
	out << "next: " << next_batch << std::endl;
	return true;
}

bool
console_cmd__fed__event_auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id", "remote"
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const net::hostport remote
	{
		param.at(2, event_id.host())
	};

	m::v1::event_auth::opts opts;
	opts.remote = remote;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::event_auth request
	{
		room_id, event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::array &auth_chain
	{
		request
	};

	std::vector<m::event> events(size(auth_chain));
	std::transform(begin(auth_chain), end(auth_chain), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	std::sort(begin(events), end(events));
	for(const auto &event : events)
		out << pretty_oneline(event) << std::endl;

	return true;
}

bool
console_cmd__fed__query__profile(opt &out, const string_view &line)
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

	request.wait(out.timeout);
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
console_cmd__fed__query__directory(opt &out, const string_view &line)
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

	request.wait(out.timeout);
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
console_cmd__fed__user__devices(opt &out, const string_view &line)
{
	const m::id::user &user_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, user_id.host())
	};

	m::v1::user::devices::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::user::devices request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const string_view stream_id
	{
		unquote(response["stream_id"])
	};

	const json::array &devices
	{
		response["devices"]
	};

	for(const json::object &device : devices)
		out << string_view{device} << std::endl;

	out << "-- " << size(devices) << " devices." << std::endl;
	return true;
}

bool
console_cmd__fed__query__client_keys(opt &out, const string_view &line)
{
	const m::id::user &user_id
	{
		token(line, ' ', 0)
	};

	const string_view &device_id
	{
		token(line, ' ', 1)
	};

	const net::hostport remote
	{
		token(line, ' ', 2, user_id.host())
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::query::client_keys request
	{
		user_id, device_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
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
console_cmd__fed__version(opt &out, const string_view &line)
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

	request.wait(out.timeout);
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
