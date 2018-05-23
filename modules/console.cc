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

conf::item<seconds>
default_synapse
{
	{ "name",     "ircd.console.timeout" },
	{ "default",  45L                    },
};

/// The first parameter for all commands. This aggregates general options
/// passed to commands as well as providing the output facility with an
/// ostream interface. Commands should only send output to this object. The
/// command's input line is not included here; it's the second param to a cmd.
struct opt
{
	std::ostream &out;
	bool html {false};
	seconds timeout {default_synapse};
	string_view special;

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

int console_command_numeric(opt &, const string_view &line);
bool console_id__user(opt &, const m::user::id &id, const string_view &line);
bool console_id__node(opt &, const m::node::id &id, const string_view &line);
bool console_id__room(opt &, const m::room::id &id, const string_view &line);
bool console_id__event(opt &, const m::event::id &id, const string_view &line);
bool console_json(const json::object &);

int
console_command_derived(opt &out, const string_view &line)
{
	const string_view id
	{
		token(line, ' ', 0)
	};

	// First check if the line starts with a number, this is a special case
	// sent to a custom dispatcher (which right now is specifically for the
	// event composer suite).
	if(try_lex_cast<int>(id))
		return console_command_numeric(out, line);

	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::EVENT:
			return console_id__event(out, id, line);

		case m::id::ROOM:
			return console_id__room(out, id, line);

		case m::id::NODE:
			return console_id__node(out, id, line);

		case m::id::USER:
			return console_id__user(out, id, line);

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
// log
//

bool
console_cmd__log(opt &out, const string_view &line)
{
	for(const auto *const &log : log::log::list)
		out << (log->snote? log->snote : '-')
		    << " " << std::setw(8) << std::left << log->name
		    << " "
		    << (log->fmasked? " FILE" : "")
		    << (log->cmasked? " CONSOLE" : "")
		    << std::endl;

	return true;
}

bool
console_cmd__log__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"level",
	}};

	if(!param.count())
	{
		for(int i(0); i < num_of<log::facility>(); ++i)
			if(i > RB_LOG_LEVEL)
				out << "[\033[1;40m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
			else if(console_enabled(log::facility(i)))
				out << "[\033[1;42m+\033[0m]: " << reflect(log::facility(i)) << std::endl;
			else
				out << "[\033[1;41m-\033[0m]: " << reflect(log::facility(i)) << std::endl;

		return true;
	}

	const int level
	{
		param.at<int>(0)
	};

	for(int i(0); i < num_of<log::facility>(); ++i)
		if(i > RB_LOG_LEVEL)
		{
			out << "[\033[1;40m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
		}
		else if(i <= level)
		{
			console_enable(log::facility(i));
			out << "[\033[1;42m+\033[0m]: " << reflect(log::facility(i)) << std::endl;
		} else {
			console_disable(log::facility(i));
			out << "[\033[1;41m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
		}

	return true;
}

bool
console_cmd__log__mask(opt &out, const string_view &line)
{
	thread_local string_view list[64];
	const auto &count
	{
		tokens(line, ' ', list)
	};

	log::console_mask({list, count});
	return true;
}

bool
console_cmd__log__unmask(opt &out, const string_view &line)
{
	thread_local string_view list[64];
	const auto &count
	{
		tokens(line, ' ', list)
	};

	log::console_unmask({list, count});
	return true;
}

bool
console_cmd__log__mark(opt &out, const string_view &line)
{
	const string_view &msg
	{
		empty(line)?
			"marked by console":
			line
	};

	log::mark
	{
		msg
	};

	out << "The log files were marked with '" << msg
	    << "'"
	    << std::endl;

	return true;
}

bool
console_cmd__mark(opt &out, const string_view &line)
{
	return console_cmd__log__mark(out, line);
}

//
// info
//

bool
console_cmd__info(opt &out, const string_view &line)
{
	info::dump();

	out << "Daemon information was written to the log."
	    << std::endl;

	return true;
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
		<< std::setw(48) << std::left << std::setfill('_') << item.first
		<< " " << item.second->get(val)
		<< std::endl;

	return true;
}

bool
console_cmd__conf(opt &out, const string_view &line)
{
	return console_cmd__conf__list(out, line);
}

bool
console_cmd__conf__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key", "value"
	}};

	const auto &key
	{
		param.at(0)
	};

	const auto &val
	{
		param.at(1)
	};

	using prototype = m::event::id::buf (const m::user::id &,
	                                     const string_view &key,
	                                     const string_view &val);

	static m::import<prototype> set_conf_item
	{
		"s_conf", "set_conf_item"
	};

	const auto event_id
	{
		set_conf_item(m::me, key, val)
	};

	out << event_id << " <- " << key << " = " << val << std::endl;
	return true;
}

bool
console_cmd__conf__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key"
	}};

	const auto &key
	{
		param.at(0)
	};

	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
	{
		if(item.first != key)
			continue;

		out << std::setw(48) << std::right << item.first
		    << " = " << item.second->get(val)
		    << std::endl;

		return true;
	}

	throw m::NOT_FOUND
	{
		"Conf item '%s' not found", key
	};
}

//
// hook
//

bool
console_cmd__hook__list(opt &out, const string_view &line)
{
	for(const auto &site : m::hook::site::list)
	{
		out << std::setw(24) << std::left << site->name()
		    << std::endl;

		out << string_view{site->feature}
		    << std::endl;

		for(const auto &hookp : site->hooks)
			out << (hookp->registered? '+' : '-')
			    << " " << string_view{hookp->feature}
			    << std::endl;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__hook(opt &out, const string_view &line)
{
	return console_cmd__hook__list(out, line);
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
	const auto names
	{
		tokens<std::vector>(line, ' ')
	};

	for(auto it(names.begin()); it != names.end(); ++it)
	{
		const auto &name{*it};
		if(m::modules.erase(std::string{name}))
			out << name << " unloaded." << std::endl;
		else
			out << name << " is not loaded." << std::endl;
	}

	for(auto it(names.rbegin()); it != names.rend(); ++it)
	{
		const auto &name{*it};
		if(m::modules.emplace(std::string{name}, name).second)
			out << name << " loaded." << std::endl;
		else
			out << name << " is already loaded." << std::endl;
	}

	return true;
}

bool
console_cmd__mod__load(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(m::modules.find(name) != end(m::modules))
		{
			out << name << " is already loaded." << std::endl;
			return;
		}

		m::modules.emplace(std::string{name}, name);
		out << name << " loaded." << std::endl;
	});

	return true;
}

bool
console_cmd__mod__unload(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(!m::modules.erase(std::string{name}))
		{
			out << name << " is not loaded." << std::endl;
			return;
		}

		out << "unloaded " << name << std::endl;
	});

	return true;
}

//
// ctx
//

bool
console_cmd__ctx__interrupt(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	for(size_t i(0); i < param.count(); ++i)
		for(auto *const &ctx : ctx::ctxs)
			if(id(*ctx) == param.at<uint64_t>(i))
			{
				interrupt(*ctx);
				break;
			}

	return true;
}

bool
console_cmd__ctx__term(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	for(size_t i(0); i < param.count(); ++i)
		for(auto *const &ctx : ctx::ctxs)
			if(id(*ctx) == param.at<uint64_t>(i))
			{
				terminate(*ctx);
				break;
			}

	return true;
}

bool
console_cmd__ctx__list(opt &out, const string_view &line)
{
	out << "   "
	    << "ID"
	    << "   "
	    << "STATE"
	    << "   "
	    << "YIELDS"
	    << "      "
	    << "CYCLE COUNT"
	    << "     "
	    << "PCT"
	    << "     "
	    << "STACK "
	    << "   "
	    << "LIMIT"
	    << "     "
	    << "PCT"
	    << "   "
	    << ":NAME"
	    << std::endl;

	for(const auto *const &ctxp : ctx::ctxs)
	{
		const auto &ctx{*ctxp};
		out << std::setw(5) << std::right << id(ctx);
		out << "  "
		    << (started(ctx)? 'S' : '-')
		    << (running(ctx)? 'R' : '-')
		    << (waiting(ctx)? 'W' : '-')
		    << (finished(ctx)? 'F' : '-')
		    << (interruptible(ctx)? '-' : 'N')
		    << (interruption(ctx)? 'I' : '-')
		    << (termination(ctx)? 'T' : '-')
		    ;

		out << " "
		    << std::setw(8) << std::right << yields(ctx)
		    << " ";

		out << " "
		    << std::setw(15) << std::right << cycles(ctx)
		    << " ";

		const long double total_cyc(ctx::prof::total_slice_cycles());
		const auto tsc_pct
		{
			total_cyc > 0.0? (cycles(ctx) / total_cyc) : 0.0L
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (tsc_pct * 100)
		    << "% ";

		out << "  "
		    << std::setw(7) << std::right << stack_at(ctx)
		    << " ";

		out << " "
		    << std::setw(7) << std::right << stack_max(ctx)
		    << " ";

		const auto stack_pct
		{
			stack_at(ctx) / (long double)stack_max(ctx)
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (stack_pct * 100)
		    << "% ";

		out << "  :"
		    << name(ctx);

		out << std::endl;
	}

	return true;
}

bool
console_cmd__ctx(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__ctx__list(out, line);

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
		"dbname", "[sync]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto sync
	{
		param.at(1, false)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	flush(database, sync);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__sort(opt &out, const string_view &line)
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

	sort(database, blocking);
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
		"dbname", "[colname]", "[begin]", "[end]", "[level]"
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

	const auto level
	{
		param.at(4, -1)
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

	const bool integer
	{
		begin? try_lex_cast<uint64_t>(begin) : false
	};

	const std::pair<string_view, string_view> range
	{
		integer? byte_view<string_view>(lex_cast<uint64_t>(begin)) : begin,
		integer && end? byte_view<string_view>(lex_cast<uint64_t>(end)) : end,
	};

	compact(column, range, level);
	compact(column, level);
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
	if(ticker && ticker != "-a")
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

		const auto &val
		{
			db::ticker(database, i)
		};

		if(val == 0 && ticker != "-a")
			continue;

		out << std::left << std::setw(48) << std::setfill('_') << name
		    << "  " << val
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
console_cmd__db__io(opt &out, const string_view &line)
{
	const auto &ic
	{
		db::iostats_current()
	};

	out << db::string(ic) << std::endl;
	return true;
}

bool
console_cmd__db__perf(opt &out, const string_view &line)
{
	const auto &pc
	{
		db::perf_current()
	};

	out << db::string(pc) << std::endl;
	return true;
}

bool
console_cmd__db__perf__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[level]"
	}};

	if(!param.count())
	{
		const auto &level
		{
			db::perf_level()
		};

		out << "Current level is: " << level << std::endl;
		return true;
	}

	const auto &level
	{
		param.at<uint>(0)
	};

	db::perf_level(level);
	out << "Set level to: " << level << std::endl;
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
console_cmd__db__cache(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column",
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	if(!colname)
	{
		const auto usage(db::usage(cache(database)));
		const auto capacity(db::capacity(cache(database)));
		const auto usage_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		out << "(row cache) "
		    << std::setw(9) << usage
		    << " "
		    << std::setw(9) << capacity
		    << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (usage_pct * 100)
		    << "%"
		    << std::endl;

		return true;
	}

	out << std::left
	    << std::setw(16) << "COLUMN"
	    << std::right
	    << " "
	    << std::setw(9) << "CACHED"
	    << " "
	    << std::setw(9) << "CAPACITY"
	    << " "
	    << std::setw(5) << "   PCT"
	    << " "
	    << std::setw(9) << " COMPRESS"
	    << " "
	    << std::setw(9) << "CAPACITY"
	    << " "
	    << std::setw(5) << "   PCT"
	    << " "
	    << std::endl;

	const auto output{[&out]
	(const string_view &column_name,
	 const size_t &usage, const size_t &capacity,
	 const size_t usage_comp, const size_t &capacity_comp,
	 const double &usage_pct, const double &usage_comp_pct)
	{
		out << std::setw(16) << std::left << column_name
		    << " "
		    << std::right
		    << std::setw(9) << usage
		    << " "
		    << std::setw(9) << capacity
		    << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (usage_pct * 100)
		    << "% "
		    << std::setw(9) << usage_comp
		    << " "
		    << std::setw(9) << capacity_comp
		    << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (usage_comp_pct * 100)
		    << "%"
		    << std::endl;
	}};

	const auto query{[&output, &database]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto usage(db::usage(cache(column)));
		const auto capacity(db::capacity(cache(column)));
		const auto usage_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		const auto usage_comp(db::usage(cache_compressed(column)));
		const auto capacity_comp(db::capacity(cache_compressed(column)));
		const auto usage_comp_pct
		{
			capacity_comp > 0.0? (double(usage_comp) / double(capacity_comp)) : 0.0L
		};

		output(colname, usage, capacity, usage_comp, capacity_comp, usage_pct, usage_comp_pct);
	}};

	// Querying the totals for all caches for all columns in a loop
	if(colname == "*")
	{
		size_t usage(0), usage_comp(0);
		size_t capacity(0), capacity_comp(0);
		for(const auto &column : database.columns)
		{
			const db::column c(*column);
			usage += db::usage(cache(c));
			capacity += db::capacity(cache(c));
			usage_comp += db::usage(cache_compressed(c));
			capacity_comp += db::capacity(cache_compressed(c));
		}

		const auto usage_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		const auto usage_comp_pct
		{
			capacity_comp > 0.0? (double(usage_comp) / double(capacity_comp)) : 0.0L
		};

		output("*", usage, capacity, usage_comp, capacity_comp, usage_pct, usage_comp_pct);
		return true;
	}

	// Query the cache for a single column
	if(colname != "**")
	{
		query(colname);
		return true;
	}

	// Querying the cache for all columns in a loop
	for(const auto &column_name : database.column_names)
		query(column_name);

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	return console_cmd__db__prop(out, fmt::snstringf
	{
		1024, "%s %s rocksdb.stats",
		param.at(0),
		param.at(1)
	});
}

bool
console_cmd__db__set(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "option", "value"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto option
	{
		param.at(2)
	};

	const auto value
	{
		param.at(3)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	// Special branch for DBOptions
	if(colname == "*")
	{
		db::setopt(database, option, value);
		out << "done" << std::endl;
		return true;
	}

	const auto setopt{[&out, &database, &option, &value]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		db::setopt(column, option, value);
		out << colname << " :done" << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		setopt(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &colname : database.column_names)
		setopt(colname);

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
console_cmd__db__bytes(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	auto &database
	{
		*db::database::dbs.at(param.at(0))
	};

	if(!param[1] || param[1] == "*")
	{
		const auto bytes
		{
			db::bytes(database)
		};

		out << bytes << std::endl;
		return true;
	}

	const auto query{[&out, &database]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::bytes(column)
		};

		out << std::setw(16) << std::right << colname
		    << " : " << value
		    << std::endl;
	}};

	if(param[1] != "**")
	{
		query(param.at(1));
		return true;
	}

	for(const auto &colname : database.column_names)
		query(colname);

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

bool
console_cmd__db(opt &out, const string_view &line)
try
{
	if(empty(line))
		return console_cmd__db__list(out, line);

	const params param{line, " ",
	{
		"dbname"
	}};

	auto &database
	{
		*db::database::dbs.at(param.at(0))
	};

	out << std::left << std::setw(28) << std::setfill('_') << "UUID "
	    << " " << uuid(database)
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "errors "
	    << " " << db::property(database, "rocksdb.background-errors")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "columns "
	    << " " << database.columns.size()
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "files "
	    << " " << file_count(database)
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "sequence "
	    << " " << sequence(database)
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "keys "
	    << " " << db::property(database, "rocksdb.estimate-num-keys")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "size "
	    << " " << bytes(database)
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "row cache size "
	    << " " << db::usage(cache(database))
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "live data size "
	    << " " << db::property(database, "rocksdb.estimate-live-data-size")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "all tables size "
	    << " " << db::property(database, "rocksdb.size-all-mem-tables")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "active table size "
	    << " " << db::property(database, "rocksdb.cur-size-active-mem-table")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "active table entries "
	    << " " << db::property(database, "rocksdb.num-entries-active-mem-table")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "active table deletes "
	    << " " << db::property(database, "rocksdb.num-deletes-active-mem-table")
	    << std::endl;

	out << std::left << std::setw(28) << std::setfill('_') << "lsm sequence "
	    << " " << db::property(database, "rocksdb.current-super-version-number")
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
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

bool
console_cmd__peer__find(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"ip:port"
	}};

	const auto &ip{rsplit(param.at(0), ':').first};
	const auto &port{rsplit(param.at(0), ':').second};
	const net::ipport ipp{ip, port? port : "0"};

	for(const auto &p : server::peers)
	{
		const auto &hostname{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp_
		{
			peer.remote
		};

		if(is_v6(ipp) && (!is_v6(ipp_) || host6(ipp) != host6(ipp_)))
			continue;

		if(is_v4(ipp) && (!is_v4(ipp_) || host4(ipp) != host4(ipp_)))
			continue;

		if(net::port(ipp) && net::port(ipp) != net::port(ipp_))
			continue;

		out << hostname << std::endl;
		break;
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
	(std::exception_ptr eptr_, const net::hostport &, const net::ipport &ipport_)
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
console_cmd__host(opt &out, const string_view &line)
{
	return console_cmd__net__host(out, line);
}

bool
console_cmd__net__host__cache__A(opt &out, const string_view &line)
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

bool
console_cmd__net__host__cache__SRV(opt &out, const string_view &line)
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

bool
console_cmd__net__host__prefetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	size_t count{0};
	origins.for_each([&count](const string_view &origin)
	{
		net::dns(origin, net::dns::prefetch_ipport);
		++count;
	});

	out << "Prefetch resolving " << count << " origins."
	    << std::endl;

	return true;
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

	const params param{line, " ",
	{
		"[id]",
	}};

	const auto &idnum
	{
		param.at<ulong>(0, 0)
	};

	for(const auto *const &client : ircd::client::list)
	{
		if(idnum && client->id < idnum)
			continue;
		else if(idnum && client->id > idnum)
			break;

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
		else
			out << " ASYNC";

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
	const params param{line, " ",
	{
		"server_name", "[query_server]"
	}};

	const auto server_name
	{
		param.at(0)
	};

	const auto query_server
	{
		param[1]
	};

	if(!query_server)
	{
		m::keys::get(server_name, [&out]
		(const auto &keys)
		{
			out << keys << std::endl;
		});

		return true;
	}

	return true;
}

//
// compose
//

std::vector<std::string> compose;

bool
console_cmd__compose__list(opt &out, const string_view &line)
{
	for(const json::object &object : compose)
	{
		const m::event event{object};
		out << pretty_oneline(event) << std::endl;
	}

	return true;
}

bool
console_cmd__compose(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[json]"
	}};

	if(!param.count())
		return console_cmd__compose__list(out, line);

	const auto &id
	{
		param.at<uint>(0)
	};

	if(compose.size() < id)
		throw error
		{
			"Cannot compose position %d without composing %d first", id, compose.size()
		};

	if(compose.size() == id)
	{
		const m::event base_event
		{
			{ "depth",             json::undefined_number  },
			{ "origin",            my_host()               },
			{ "origin_server_ts",  time<milliseconds>()    },
			{ "sender",            m::me.user_id           },
			{ "room_id",           m::my_room.room_id      },
			{ "type",              "m.room.message"        },
		};

		compose.emplace_back(json::strung(base_event));
	}

	const auto &key
	{
		param[1]
	};

	const auto &val
	{
		tokens_after(line, ' ', 1)
	};

	if(key && val)
	{
		m::event event{compose.at(id)};
		set(event, key, val);
		compose.at(id) = json::strung{event};
	}

	const m::event event
	{
		compose.at(id)
	};

	out << pretty(event) << std::endl;
	out << compose.at(id) << std::endl;
	return true;
}

bool
console_cmd__compose__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	if(id == -1)
	{
		compose.clear();
		return true;
	}

	compose.at(id).clear();
	return true;
}

bool
console_cmd__compose__eval(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	m::vm::opts opts;
	m::vm::eval eval
	{
		opts
	};

	if(id == -1)
		for(size_t i(0); i < compose.size(); ++i)
			eval(m::event{compose.at(i)});
	else
		eval(m::event{compose.at(id)});

	return true;
}

bool
console_cmd__compose__commit(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	m::vm::copts copts;
	m::vm::eval eval
	{
		copts
	};

	if(id == -1)
		for(size_t i(0); i < compose.size(); ++i)
			eval(m::event{compose.at(i)});
	else
		eval(m::event{compose.at(id)});

	return true;
}

int
console_command_numeric(opt &out, const string_view &line)
{
	return console_cmd__compose(out, line);
}

//
// events
//

bool
console_cmd__events(opt &out, const string_view &line)
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

	m::events::rfor_each(start, [&out, &limit]
	(const uint64_t &seq, const m::event &event)
	{
		out << seq << " " << pretty_oneline(event) << std::endl;;
		return --limit;
	});

	return true;
}

bool
console_cmd__events__filter(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"start", "event_filter_json"
	}};

	const uint64_t start
	{
		param.at<uint64_t>(0, uint64_t(-1))
	};

	const m::event_filter filter
	{
		param.at(1)
	};

	m::events::rfor_each(start, filter, [&out]
	(const uint64_t &seq, const m::event &event)
	{
		out << seq << " " << pretty_oneline(event) << std::endl;;
		return true;
	});

	return true;
}

//
// event
//

bool
console_cmd__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	const m::event::fetch event
	{
		event_id
	};

	if(!empty(args)) switch(hash(token(args, ' ', 0)))
	{
		case hash("raw"):
			out << event << std::endl;
			return true;

		case hash("idx"):
			out << index(event) << std::endl;
			return true;

		case hash("content"):
		{
			for(const auto &m : json::get<"content"_>(event))
				out << m.first << ": " << m.second << std::endl;

			return true;
		}
	}

	out << pretty(event) << std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	try
	{
		if(!verify(event))
			out << "- SIGNATURE FAILED" << std::endl;
	}
	catch(const std::exception &e)
	{
		out << "- SIGNATURE FAILED: " << e.what() << std::endl;
	}

	if(!verify_hash(event))
		out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;

	return true;
}

bool
console_id__event(opt &out,
                  const m::event::id &id,
                  const string_view &line)
{
	return console_cmd__event(out, line);
}

bool
console_cmd__event__sign(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "[host]", "[accept|eval]"
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const auto &host
	{
		param.at(1, event_id.host())
	};

	const auto &op
	{
		param[2]
	};

	m::v1::event::opts opts;
	opts.remote = host;
	opts.dynamic = false;
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

	const m::event orig_event
	{
		request
	};

	thread_local char sigbuf[64_KiB];
	const auto event
	{
		m::signatures(mutable_buffer{sigbuf}, orig_event)
	};

	out << pretty(event)
	    << std::endl;

	if(op == "accept")
	{
		const m::vm::opts opts;
		m::vm::accepted a
		{
			event, &opts, &opts.report
		};

		m::vm::accept(a);
	}
	else if(op == "eval")
	{
		m::vm::opts opts;
		m::vm::eval
		{
			event, opts
		};
	}

	return true;
}

bool
console_cmd__event__bad(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id",
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	m::event::idx workaround;
	const bool b
	{
		bad(event_id, workaround)
	};

	out << event_id << "is"
	    << (b? " " : " NOT ") << "BAD";

	if(b && workaround != m::event::idx(-1))
		m::event::fetch::event_id(workaround, std::nothrow, [&out]
		(const m::event::id &event_id)
		{
			out << " and a workaround is " << event_id;
		});

	out << std::endl;
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
	opts.event_idx = index(event);
	m::dbs::write(txn, event, opts);
	txn();

	out << "erased " << txn.size() << " cells"
	    << " for " << event_id << std::endl;

	return true;
}

bool
console_cmd__event__dump(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"filename"
	}};

	const auto filename
	{
		param.at(0)
	};

	const unique_buffer<mutable_buffer> buf
	{
		512_KiB
	};

	char *pos{data(buf)};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::events::for_each(0, [&](const uint64_t &seq, const m::event &event)
	{
		const auto remain
		{
			size_t(data(buf) + size(buf) - pos)
		};

		assert(remain >= 64_KiB && remain <= size(buf));
		const mutable_buffer mb{pos, remain};
		pos += json::print(mb, event);
		++ecount;

		if(pos + 64_KiB > data(buf) + size(buf))
		{
			const const_buffer cb{data(buf), pos};
			foff += size(fs::append(filename, cb));
			pos = data(buf);
			++acount;

			const double pct
			{
				(seq / double(m::vm::current_sequence)) * 100.0
			};

			log::info
			{
				"dump[%s] %lf$%c @ seq %zu of %zu; %zu events; %zu bytes; %zu writes; %zu errors",
				filename,
				pct,
				'%', //TODO: fix gram
				seq,
				m::vm::current_sequence,
				ecount,
				foff,
				acount,
				errcount
			};
		}

		return true;
	});

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
console_cmd__state__find(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"root", "[type]" "[state_key]"
	}};

	const string_view &root
	{
		param.at(0)
	};

	const string_view &type
	{
		param[1]
	};

	const string_view &state_key
	{
		param[2]
	};

	const auto closure{[&out]
	(const auto &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
		return false;
	}};

	m::state::test(root, type, state_key, closure);
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
console_cmd__room__top(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const auto top
	{
		m::top(std::nothrow, room_id)
	};

	out << "idx:      " << std::get<m::event::idx>(top) << std::endl;
	out << "depth:    " << std::get<int64_t>(top) << std::endl;
	out << "event:    " << std::get<m::event::id::buf>(top) << std::endl;
	out << "m_state:  " << std::endl;

	const unique_buffer<mutable_buffer> buf{64_KiB};
	const m::room::state::tuple state
	{
		room_id, buf
	};

	out << pretty(state) << std::endl;

	return true;
}

bool
console_id__room(opt &out,
                 const m::room::id &id,
                 const string_view &line)
{
	//TODO: XXX more detailed summary
	return console_cmd__room__top(out, line);
}

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

	const m::room::head head
	{
		room
	};

	head.for_each([&out]
	(const m::event::idx &event_idx, const m::event::id &event_id)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__head__rebuild(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	using prototype = size_t (const m::room &);
	static m::import<prototype> head__rebuild
	{
		"m_room", "head__rebuild"
	};

	const size_t count
	{
		head__rebuild(room)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__head__reset(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	using prototype = size_t (const m::room &);
	static m::import<prototype> head__reset
	{
		"m_room", "head__reset"
	};

	const size_t count
	{
		head__reset(room)
	};

	out << "done " << count << std::endl;
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
	const params param{line, " ",
	{
		"room_id", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view membership
	{
		param[1]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const m::room::members::closure closure{[&out]
	(const m::user::id &user_id)
	{
		out << user_id << std::endl;
	}};

	members.for_each(membership, closure);
	return true;
}

bool
console_cmd__room__members__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view membership
	{
		param[1]
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

	members.for_each(membership, closure);
	return true;
}

bool
console_cmd__room__members__origin(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "origin", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const auto &origin
	{
		param.at(1)
	};

	const string_view membership
	{
		param[2]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	members.for_each(membership, [&out, &origin]
	(const m::event &event)
	{
		if(json::get<"origin"_>(event) != origin)
			return;

		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__members__read(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "[membership]"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const string_view membership
	{
		param.at(1, "join"_sv)
	};

	m::room::id::buf room_id
	{
		m::get(event_id, "room_id", room_id)
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const m::event::closure event_closure{[&out, &event_id]
	(const m::event &event)
	{
		if(event_id)
			if(unquote(at<"content"_>(event).get("event_id")) != event_id)
				return;

		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"sender"_>(event)
		    << " " << at<"content"_>(event)
		    << " " << at<"event_id"_>(event)
		    << std::endl;
	}};

	const auto member_closure{[&room_id, event_closure]
	(const m::event &event)
	{
		const m::user user
		{
			at<"state_key"_>(event)
		};

		static const m::event::fetch::opts fopts
		{
			m::event::keys::include
			{
				"event_id", "content", "origin_server_ts", "sender"
			},
			{
				db::get::NO_CACHE
			},
		};

		const m::user::room user_room
		{
			user, nullptr, &fopts
		};

		user_room.get(std::nothrow, "ircd.read", room_id, event_closure);
	}};

	members.for_each(membership, member_closure);
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
console_cmd__room__state__rebuild__present(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	using prototype = size_t (const m::room &);
	static m::import<prototype> state__rebuild_present
	{
		"m_room", "state__rebuild_present"
	};

	const size_t count
	{
		state__rebuild_present(room)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__state__rebuild__history(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	using prototype = size_t (const m::room &);
	static m::import<prototype> state__rebuild_history
	{
		"m_room", "state__rebuild_history"
	};

	const size_t count
	{
		state__rebuild_history(room)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "{event_filter_json}"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::event_filter filter
	{
		param[1]
	};

	const m::room room
	{
		room_id
	};

	auto limit
	{
		json::get<"limit"_>(filter)?: -1
	};

	size_t count{0};
	m::room::messages it{room};
	for(; it && limit; --it, --limit)
	{
		const m::event &event{*it};
		count += match(filter, event);
	}

	out << count << std::endl;
	return true;
}

bool
console_cmd__room__events(opt &out, const string_view &line)
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

	const bool roots
	{
		has(out.special, "roots")
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it{room};
	if(depth >= 0 && depth < std::numeric_limits<int64_t>::max())
		it.seek(depth);

	for(; it && limit >= 0; order == 'b'? --it : ++it, --limit)
		if(roots)
			out << std::setw(48) << std::left << it.state_root()
			    << " " << std::setw(8) << std::left << it.event_idx()
			    << " " << it.event_id()
			    << std::endl;
		else
			out << pretty_oneline(*it)
			    << std::endl;

	return true;
}

bool
console_cmd__room__roots(opt &out, const string_view &line)
{
	assert(!out.special);
	out.special = "roots";
	return console_cmd__room__events(out, line);
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
	const params param{line, " ",
	{
		"room_id", "sender", "type", "state_key", "content", "[prev_event_id]"
	}};

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

	const string_view prev_event_id
	{
		param[5]
	};

	const m::room room
	{
		room_id, prev_event_id
	};

	const auto event_id
	{
		send(room, sender, type, state_key, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__send(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"room_id", "sender", "type", "content", "[prev_event_id]"
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

	const json::object &content
	{
		param.at(3, json::object{})
	};

	const string_view prev_event_id
	{
		param[4]
	};

	const m::room room
	{
		room_id, prev_event_id
	};

	const auto event_id
	{
		send(room, sender, type, content)
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
console_cmd__room__create(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"room_id", "[creator]", "[type]", "[parent]"
		}
	};

	const m::room::id room_id
	{
		param.at(0)
	};

	const m::user::id creator
	{
		param.at(1, m::me.user_id)
	};

	const string_view type
	{
		param[2]
	};

	const string_view &parent
	{
		param[3]
	};

	const m::room room
	{
		parent?
			m::create(room_id, creator, parent, type):
			m::create(room_id, creator, type)
	};

	out << room.room_id << std::endl;
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

bool
console_cmd__room__dagree(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	using prototype = size_t (const m::room &, std::vector<size_t> &);
	static m::import<prototype> dagree_histogram
	{
		"m_room", "dagree_histogram"
	};

	std::vector<size_t> v(32, 0);
	const size_t count
	{
		dagree_histogram(room, v)
	};

	for(size_t i(0); i < v.size(); ++i)
		out << std::setw(2) << std::right << i << ": " << v.at(i) << std::endl;

	return true;
}

//
// user
//

//TODO: XXX
bool
console_id__user(opt &out,
                 const m::user::id &id,
                 const string_view &args)
{
	return true;
}

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
console_cmd__user__rooms__origins(opt &out, const string_view &line)
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

	const m::user::rooms::origins origins
	{
		user
	};

	origins.for_each(membership, m::user::rooms::origins::closure{[&out]
	(const string_view &origin)
	{
		out << origin << std::endl;
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
	state.for_each("ircd.read", m::event::closure{[&out]
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

bool
console_cmd__user__filter(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[filter_id]"
	}};

	const m::user user
	{
		param.at(0)
	};

	const auto &filter_id
	{
		param[1]
	};

	if(filter_id)
	{
		out << user.filter(filter_id) << std::endl;
		return true;
	}

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	state.for_each("ircd.filter", m::event::closure{[&out]
	(const m::event &event)
	{
		out << at<"state_key"_>(event) << std::endl;
		out << at<"content"_>(event) << std::endl;
		out << std::endl;
	}});

	return true;
}

//
// node
//

//TODO: XXX
bool
console_id__node(opt &out,
                 const m::node::id &id,
                 const string_view &args)
{
	return true;
}

bool
console_cmd__node__keys(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id", "[limit]"
	}};

	const m::node &node
	{
		param.at(0)
	};

	auto limit
	{
		param.at(1, size_t(1))
	};

	const m::node::room node_room
	{
		node
	};

	const m::room::state state{node_room};
	state.for_each("ircd.key", [&out, &limit]
	(const m::event &event)
	{
		const m::keys keys
		{
			json::get<"content"_>(event)
		};

		out << keys << std::endl;
		return --limit;
	});

	return true;
}

bool
console_cmd__node__key(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id", "key_id"
	}};

	const m::node &node
	{
		param.at(0)
	};

	const auto &key_id
	{
		param[1]
	};

	const m::node::room node_room{node};
	node_room.get("ircd.key", [&out]
	(const m::event &event)
	{
		const m::keys key
		{
			json::get<"content"_>(event)
		};

		out << key << std::endl;
	});

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

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__version
	{
		"federation_federation", "feds__version"
	};

	feds__version(room_id, out.timeout, [&out]
	(const string_view &origin, std::exception_ptr eptr, const json::object &response)
	{
		out << (eptr? '-' : '+')
		    << " "
		    << std::setw(40) << std::left << origin
		    << " ";

		if(eptr)
			out << what(eptr);
		else
			out << string_view{response};

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__feds__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id::buf &event_id
	{
		param.count() > 1? param.at(1) : m::head(room_id)
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const m::event::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__state
	{
		"federation_federation", "feds__state"
	};

	std::forward_list<std::string> origins;
	std::map<std::string, std::forward_list<string_view>, std::less<>> grid;

	feds__state(room_id, event_id, out.timeout, [&out, &grid, &origins]
	(const string_view &origin, std::exception_ptr eptr, const json::object &response)
	{
		if(eptr)
			return true;

		const json::array &auth_chain
		{
			response["auth_chain_ids"]
		};

		const json::array &pdus
		{
			response["pdu_ids"]
		};

		for(const auto &pdu_id : pdus)
		{
			const auto &event_id{unquote(pdu_id)};
			auto it
			{
				grid.lower_bound(event_id)
			};

			if(it == end(grid) || it->first != event_id)
				it = grid.emplace_hint(it, event_id, std::forward_list<string_view>{});

			origins.emplace_front(origin);
			it->second.emplace_front(origins.front());
		}

		return true;
	});

	for(auto &p : grid)
	{
		p.second.sort();

		out << std::setw(64) << std::left << p.first << " : ";
		for(const auto &origin : p.second)
			out << " " << origin;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__feds__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	using prototype = void (const m::event::id &,
	                        std::ostream &);

	static m::import<prototype> feds__event
	{
		"federation_federation", "feds__event"
	};

	feds__event(event_id, out);
	return true;
}

bool
console_cmd__feds__head(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[user_id]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &user_id
	{
		param.at(1, m::me.user_id)
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const m::user::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__head
	{
		"federation_federation", "feds__head"
	};

	feds__head(room_id, user_id, out.timeout, [&out]
	(const string_view &origin, std::exception_ptr eptr, const json::object &event)
	{
		if(eptr)
			return true;

		const json::array prev_events
		{
			event.at("prev_events")
		};

		out << "+ " << std::setw(40) << std::left << origin;
		out << " " << event.at("depth");
		for(const json::array prev_event : prev_events)
		{
			const auto &prev_event_id
			{
				unquote(prev_event.at(0))
			};

			out << " " << string_view{prev_event_id};
		};
		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__feds__heads(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[user_id]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &user_id
	{
		param.at(1, m::me.user_id)
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const m::user::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__head
	{
		"federation_federation", "feds__head"
	};

	std::forward_list<std::string> origins;
	std::map<std::string, std::forward_list<string_view>, std::less<>> grid;

	feds__head(room_id, user_id, out.timeout, [&origins, &grid]
	(const string_view &origin, std::exception_ptr eptr, const json::object &event)
	{
		if(eptr)
			return true;

		const json::array &prev_events
		{
			event.at("prev_events")
		};

		for(const json::array &prev_event : prev_events)
		{
			const auto &event_id
			{
				unquote(prev_event.at(0))
			};

			auto it
			{
				grid.lower_bound(event_id)
			};

			if(it == end(grid) || it->first != event_id)
				it = grid.emplace_hint(it, event_id, std::forward_list<string_view>{});

			origins.emplace_front(origin);
			it->second.emplace_front(origins.front());
		}

		return true;
	});

	for(auto &p : grid)
	{
		p.second.sort();

		out << std::setw(64) << std::left << p.first << " : ";
		for(const auto &origin : p.second)
			out << " " << origin;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__feds__perspective(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "server_name", "key_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view &server_name
	{
		param.at(1)
	};

	const string_view &key_id
	{
		param.at(2)
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::array &);

	using prototype = void (const m::room::id &,
	                        const m::v1::key::server_key &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__perspective
	{
		"federation_federation", "feds__perspective"
	};

	const m::v1::key::server_key server_key
	{
		server_name, key_id
	};

	feds__perspective(room_id, server_key, out.timeout, [&out]
	(const string_view &origin, std::exception_ptr eptr, const json::array &keys)
	{
		if(eptr)
			return true;

		for(const json::object &_key : keys)
		{
			const m::keys &key{_key};
			out << key << std::endl;
		}

		return true;
	});

	return true;
}

bool
console_cmd__feds__backfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[event_id]", "[limit]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id::buf &event_id
	{
		param.count() > 1? param.at(1) : head(room_id)
	};

	const size_t limit
	{
		param.at(2, size_t(4))
	};

	using prototype = void (const m::room::id &,
	                        const m::event::id &,
	                        const size_t &,
	                        std::ostream &);

	static m::import<prototype> feds__backfill
	{
		"federation_federation", "feds__backfill"
	};

	feds__backfill(room_id, event_id, limit, out);
	return true;
}

bool
console_cmd__feds__resend(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id",
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const m::event::fetch event
	{
		event_id
	};

	const m::vm::opts opts;
	m::vm::accepted a{event, &opts, &opts.report};
	m::vm::accept(a);
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
		room_id, user_id, buf, std::move(opts)
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
		if(op != "auth")
			for(const json::object &event : pdus)
				out << pretty_oneline(m::event{event}) << std::endl;

		out << std::endl;
		if(op != "state")
			for(const json::object &event : auth_chain)
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

	std::vector<m::event> events;
	events.reserve(size(pdus) + size(auth_chain));

	for(const json::object &event : auth_chain)
		events.emplace_back(event);

	for(const json::object &event : pdus)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));
	for(const auto &event : events)
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
	vmopts.head = false;
	vmopts.refs = true;
	m::vm::eval eval
	{
		vmopts
	};

	std::vector<m::event> events;
	events.reserve(lex_cast<size_t>(count));
	for(const json::object &event : pdus)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));
	for(const auto &event : events)
		eval(event);

	return true;
}

bool
console_cmd__fed__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "remote", "[op]"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const net::hostport &remote
	{
		param.at(1, event_id.host())
	};

	const string_view op
	{
		param[2]
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
console_cmd__fed__key(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote",
	}};

	const auto &server_name
	{
		param.at(0)
	};

	const auto &key_id
	{
		param[1]
	};

	const unique_buffer<mutable_buffer> buf{16_KiB};
	m::v1::key::opts opts;
	m::v1::key::keys request
	{
		{server_name, key_id}, buf, std::move(opts)
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

	const m::keys &key{response};
	out << key << std::endl;
	return true;
}

bool
console_cmd__fed__key__query(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "[server_name,key_id]...",
	}};

	const auto requests
	{
		tokens_after(line, ' ', 0)
	};

	std::vector<std::pair<string_view, string_view>> r;
	tokens(requests, ' ', [&r]
	(const string_view &req)
	{
		r.emplace_back(split(req, ','));
	});

	m::v1::key::opts opts;
	opts.dynamic = true;
	opts.remote = net::hostport
	{
		param.at(0)
	};

	const unique_buffer<mutable_buffer> buf{24_KiB};
	m::v1::key::query request
	{
		r, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::array keys
	{
		request
	};

	for(const json::object &key : keys)
	{
		const m::keys &k{key};
		out << k << std::endl;
	}

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

//
// file
//

bool
console_cmd__file__room(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server|amalgam", "file"
	}};

	auto server
	{
		param.at(0)
	};

	auto file
	{
		param[1]
	};

	if(empty(file))
	{
		const auto s(split(server, '/'));
		server = s.first;
		file = s.second;
	}

	using prototype = m::room::id (m::room::id::buf &,
	                               const string_view &server,
	                               const string_view &file);

	static m::import<prototype> file_room_id
	{
		"media_media", "file_room_id"
	};

	m::room::id::buf buf;
	out << file_room_id(buf, server, file) << std::endl;
	return true;
}

bool
console_cmd__file__download(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server|amalgam", "file"
	}};

	auto server
	{
		param.at(0)
	};

	auto file
	{
		param[1]
	};

	auto remote
	{
		has(server, '/')? param[1] : param[2]
	};

	if(has(server, '/'))
	{
		const auto s(split(server, '/'));
		server = s.first;
		file = s.second;
	}

	if(!remote)
		remote = server;

	using prototype = m::room::id::buf (const string_view &server,
	                                    const string_view &file,
	                                    const m::user::id &,
	                                    const net::hostport &remote);

	static m::import<prototype> download
	{
		"media_media", "download"
	};

	const m::room::id::buf room_id
	{
		download(server, file, m::me.user_id, remote)
	};

	out << room_id << std::endl;
	return true;
}

//
// vm
//

bool
console_cmd__vm(opt &out, const string_view &line)
{
	out << "sequence:       "
	    << std::right << std::setw(10) << m::vm::current_sequence
	    << std::endl;

	out << "eval total:     "
	    << std::right << std::setw(10) << m::vm::eval::id_ctr
	    << std::endl;

	out << "eval current:   "
	    << std::right << std::setw(10) << size(m::vm::eval::list)
	    << std::endl;

	return true;
}

bool
console_cmd__vm__eval(opt &out, const string_view &line)
{
	for(const auto *const &eval : m::vm::eval::list)
	{
		assert(eval);
		assert(eval->ctx);

		out << std::setw(9) << std::right << eval->id
		    << " | " << std::setw(4) << std::right << id(*eval->ctx)
		    << " | " << std::setw(4) << std::right << eval->sequence
		    << " | " << std::setw(64) << std::left << eval->event_id
		    ;

		out << std::endl;
	}

	return true;
}
