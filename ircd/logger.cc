// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_IOSTREAM
// <iostream> inclusion here runs std::ios_base::Init() statically as this unit
// is initialized (GNU initialization order given in Makefile).

namespace ircd::log
{
	struct confs;

	static bool is_conf_mask_file(const string_view &name);
	static bool is_conf_mask_console(const string_view &name);
	static void slog(const log &, const level &, const window_buffer::closure &) noexcept;
	static void vlog_threadsafe(const log &, const level &, const string_view &fmt, const va_rtti &ap);
	static std::string file_path(const level &);
	static void open(const level &);
	static void mkdir();

	extern bool ready;
	extern const size_t CTX_NAME_TRUNC;
	extern const size_t LOG_NAME_TRUNC;
	extern const size_t LOG_BUFSIZE;
	extern const std::array<string_view, num_of<level>()> console_ansi;
	extern std::array<confs, num_of<level>()> confs;
	extern conf::item<std::string> unmask_file;
	extern conf::item<std::string> unmask_console;
	extern conf::item<std::string> mask_file;
	extern conf::item<std::string> mask_console;
	extern std::array<std::ofstream, num_of<level>()> file;
	extern std::array<ulong, num_of<level>()> console_quiet_stdout;
	extern std::array<ulong, num_of<level>()> console_quiet_stderr;
	std::ostream &out_console{std::cout};
	std::ostream &err_console{std::cerr};
}

struct ircd::log::confs
{
	conf::item<bool> file_enable;
	conf::item<bool> file_flush;
	conf::item<bool> console_stdout;
	conf::item<bool> console_stderr;
	conf::item<bool> console_flush;
};

/// Linkage for list of named loggers.
template<>
decltype(ircd::instance_list<ircd::log::log>::allocator)
ircd::instance_list<ircd::log::log>::allocator
{};

template<>
decltype(ircd::instance_list<ircd::log::log>::list)
ircd::instance_list<ircd::log::log>::list
{
	allocator
};

decltype(ircd::log::file)
ircd::log::file;

decltype(ircd::log::console_quiet_stdout)
ircd::log::console_quiet_stdout;

decltype(ircd::log::console_quiet_stderr)
ircd::log::console_quiet_stderr;

decltype(ircd::log::LOG_BUFSIZE)
ircd::log::LOG_BUFSIZE
{
	1024
};

decltype(ircd::log::LOG_NAME_TRUNC)
ircd::log::LOG_NAME_TRUNC
{
	12
};

decltype(ircd::log::CTX_NAME_TRUNC)
ircd::log::CTX_NAME_TRUNC
{
	10
};

decltype(ircd::log::ready)
ircd::log::ready
{
	false
};

/// The '*' logger is for log::mark()'s which do not target any specific named
/// logger because such mark()'s are not repeated to all named loggers. The '*'
/// logger should not otherwise be the target of log messages.
decltype(ircd::log::star)
ircd::log::star
{
	"*", '*'
};

/// The general logger is for all core and miscellaneous log messages. This
/// is the default logger target for log:: calls which do not pass a specific
/// logger.
decltype(ircd::log::general)
ircd::log::general
{
	"ircd", 'G'
};

decltype(ircd::log::hook)
ircd::log::hook;

//
// init
//

void
ircd::log::init()
{
	// DEBUG is very noisy so it always starts off by default unless -debug
	if(!ircd::debugmode)
		console_disable(level::DEBUG);

	// In non-debug-mode compilation we don't want DWARNING and DERROR
	// messages which weren't DCE'ed to be displayed by default.
	if(!RB_DEBUG_LEVEL && !ircd::debugmode)
	{
		console_disable(level::DERROR);
		console_disable(level::DWARNING);
	}

	if(!ircd::write_avoid)
	{
		mkdir();
		open();
	}

	ircd::log::ready = true;
}

void
ircd::log::fini()
{
	flush();
	close();
}

void
ircd::log::mkdir()
{
	if(!fs::exists(fs::base::log))
		fs::mkdir(fs::base::log);
}

//
// interface
//

void
ircd::log::open()
{
	for_each<level>([](const level &lev)
	{
		if(file[lev].is_open())
			file[lev].close();

		file[lev].clear();
		file[lev].exceptions(std::ios::badbit | std::ios::failbit);
		open(lev);
	});
}

void
ircd::log::close()
{
	for_each<level>([](const level &lev)
	{
		if(file[lev].is_open())
			file[lev].close();
	});
}

void
ircd::log::flush()
{
	for_each<level>([](const level &lev)
	{
		file[lev].flush();
	});

	out_console << std::flush;
}

void
ircd::log::open(const level &lev)
try
{
	const auto &mode(std::ios::app);
	const auto &path(file_path(lev));
	file[lev].open(path.c_str(), mode);
}
catch(const std::exception &e)
{
	fprintf(stderr, "!!! Opening log file [%s] failed: %s",
	        file_path(lev).c_str(),
	        e.what());
	throw;
}

std::string
ircd::log::file_path(const level &lev)
{
	return fs::path_string(fs::path_views
	{
		fs::base::log, reflect(lev)
	});
}

void
ircd::log::console_enable()
{
	for_each<level>([](const level &lev)
	{
		console_enable(lev);
	});
}

void
ircd::log::console_disable()
{
	for_each<level>([](const level &lev)
	{
		console_disable(lev);
	});
}

void
ircd::log::console_enable(const level &lev)
{
	if(console_quiet_stdout[lev])
		console_quiet_stdout[lev]--;

	if(console_quiet_stderr[lev])
		console_quiet_stderr[lev]--;
}

void
ircd::log::console_disable(const level &lev)
{
	console_quiet_stdout[lev]++;
	console_quiet_stderr[lev]++;
}

bool
ircd::log::console_enabled(const level &lev)
{
	return !console_quiet_stdout[lev] ||
	       !console_quiet_stderr[lev];
}

void
ircd::log::file_mask(const vector_view<const string_view> &list)
{
	for(auto *const &log : log::list)
		log->fmasked = std::find(begin(list), end(list), log->name) != end(list);
}

void
ircd::log::file_unmask(const vector_view<const string_view> &list)
{
	for(auto *const &log : log::list)
		log->fmasked = std::find(begin(list), end(list), log->name) == end(list);
}

void
ircd::log::console_mask(const vector_view<const string_view> &list)
{
	for(auto *const &log : log::list)
		log->cmasked = std::find(begin(list), end(list), log->name) != end(list);
}

void
ircd::log::console_unmask(const vector_view<const string_view> &list)
{
	for(auto *const &log : log::list)
		log->cmasked = std::find(begin(list), end(list), log->name) == end(list);
}

//
// console quiet
//

ircd::log::console_quiet::console_quiet(const bool &showmsg)
{
	if(showmsg)
		notice("Log messages are now quieted at the console");

	console_disable();
}

ircd::log::console_quiet::~console_quiet()
{
	console_enable();
}

//
// mark
//

ircd::log::mark::mark(const string_view &msg)
{
	for_each<level>([&msg]
	(const auto &lev)
	{
		if(lev > RB_LOG_LEVEL)
			return;

		mark(lev, msg);
	});
}

ircd::log::mark::mark(const level &lev,
                      const string_view &msg)
{
	vlog(star, lev, "%s", msg);
}

//
// log
//

ircd::log::log *
ircd::log::log::find(const string_view &name)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&name]
		(const auto *const &log)
		{
			return log->name == name;
		})
	};

	return *it;
}

ircd::log::log *
ircd::log::log::find(const char &snote)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&snote]
		(const auto *const &log)
		{
			return log->snote == snote;
		})
	};

	return *it;
}

/// This function is related to cross-thread logging; its purpose
/// is to check if a given named logger still exists after the pointer
/// is conveyed to the main thread from some other thread.
bool
ircd::log::log::exists(const log *const &ptr)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&ptr]
		(const auto *const &log)
		{
			return log == ptr;
		})
	};

	return it != end(list);
}

//
// log::log
//

ircd::log::log::log(const string_view &name,
                    const char &snote)
:name{name}
,snote{snote}
,cmasked
{
	is_conf_mask_console(name)
}
,fmasked
{
	is_conf_mask_file(name)
}
{
	for(const auto *const &other : list)
	{
		if(other == this)
			continue;

		if(other->name == name)
			throw ircd::error
			{
				"Logger with name '%s' already exists at %p",
				name,
				other
			};

		if(snote != '\0' && other->snote == snote)
			throw ircd::error
			{
				"Logger with snote '%c' is '%s' and already exists at %p",
				snote,
				other->name,
				other
			};
	}
}

// required for instance_list template instantiation.
ircd::log::log::~log()
noexcept
{
}

//
// vlog
//

/// ircd::log is not thread-safe. This internal function is called when the
/// normal vlog() detects it's not on the main IRCd thread. It then generates
/// the formatted log message on this thread, and posts the message to the
/// main IRCd event loop which is running on the main thread.
void
ircd::log::vlog_threadsafe(const log &log,
                           const level &lev,
                           const string_view &fmt,
                           const va_rtti &ap)
{
	static ios::descriptor descriptor
	{
		"ircd::log::vlog_threadsafe"
	};

	// Generate the formatted message on this thread first
	std::string str
	{
		fmt::vsnstringf(1024, fmt, ap)
	};

	// The pointer to the logger is copied to the main thread.
	auto *const logp{&log};
	ircd::dispatch{descriptor, ios::defer, [lev, str(std::move(str)), logp]
	{
		// If that named logger was destroyed while this closure was
		// travelling to the main thread then we just discard this message.
		if(!log::exists(logp))
			return;

		slog(*logp, lev, [&str](const mutable_buffer &out) -> size_t
		{
			return copy(out, string_view{str});
		});
	}};
}

ircd::log::vlog::vlog(const log &log,
                      const level &lev,
                      const string_view &fmt,
                      const va_rtti &ap)
noexcept
{
	if(!is_main_thread() && likely(ios::available()))
	{
		vlog_threadsafe(log, lev, fmt, ap);
		return;
	}

	slog(log, lev, [&fmt, &ap](const mutable_buffer &out) -> size_t
	{
		return fmt::vsprintf(out, fmt, ap);
	});
}

namespace ircd::log
{
	bool entered;

	static bool can_skip(const log &, const level &);
}

void
ircd::log::slog(const log &log,
                const level &lev,
                const window_buffer::closure &closure)
noexcept
{
	if(can_skip(log, lev))
		return;

	// Have to be on the main thread to call slog().
	assert_main_thread();

	// During the composition of this log message, if another log message
	// is created from calls for normal reasons or from errors, it's a
	// problem too. We can only have one log slog() at a time for now...
	const reentrance_assertion<entered> ra;

	// If slog() yields for some reason that's not good either...
	const ctx::critical_assertion ca;

	// The principal buffer doesn't have to be static but courtesy of all the
	// above effort we might as well take advantage...
	static char buf[LOG_BUFSIZE], date[64];

	// Maximum size of log line leaving 2 characters for \r\n
	const size_t max(sizeof(buf) - 2);

	// Get the ANSI color for the level.
	const string_view &console_ansi
	{
		ircd::log::console_ansi.at(lev)
	};

	// Get the current tick count for the core event-loop.
	const auto &epoch
	{
		ios::epoch()
	};

	// Fxied width of the epoch column
	static const size_t epoch_width
	{
		12
	};

	// Fxied width of the log level column.
	static const size_t lev_width
	{
		8
	};

	// Fxied width of the context idnum column.
	static const size_t ctx_id_width
	{
		5
	};

	// Compose the prefix sequence into the buffer.
	std::stringstream s;
	pubsetbuf(s, buf);
	s
	<< microdate(date)
	<< ' '
	<< std::setw(epoch_width)
	<< std::right
	<< epoch
	<< ' '
	<< console_ansi
	<< std::setw(lev_width)
	<< std::right
	<< reflect(lev)
	<< (console_ansi? "\033[0m " : " ")
//	<< (log.snote? log.snote : '-')
	<< std::setw(LOG_NAME_TRUNC)
	<< std::left
	<< trunc(log.name, LOG_NAME_TRUNC)
	<< ' '
	<< std::setw(ctx_id_width)
	<< std::right
	<< ctx::id()
	<< ' '
	<< std::setw(CTX_NAME_TRUNC)
	<< std::left
	<< trunc(ctx::name(), CTX_NAME_TRUNC)
	<< " :";

	// Compose the user message after prefix
	const size_t pos(s.tellp());
	const mutable_buffer userspace{buf + pos, max - pos};
	window_buffer sb{userspace};
	sb(closure);

	// Compose the newline after user message.
	size_t len{pos + sb.consumed()};
	assert(len + 2 <= sizeof(buf));
	buf[len++] = '\r';
	buf[len++] = '\n';

	// The final message
	assert(len <= sizeof(buf));
	const string_view msg{buf, len};

	// Call the hooks listening for log messages. The return value in
	// `used` indicates at least one function was called, which we expect
	// after calling can_skip() above.
	bool used {false};
	hook(used, log, lev, msg);
	assert(used);
}

bool
ircd::log::can_skip(const log &log,
                    const level &lev)
{
	// Subsystem not initialized
	if(unlikely(!ircd::log::ready))
		return true;

	// Nobody is listening.
	if(hook.empty())
		return true;

	// If used remains false, we can skip generating this log message; none
	// of the listeners will be making use of it.
	bool used {false};
	hook(used, log, lev, string_view{});
	return !used;
}

//
// Primary internal log hooks. This will likely be moved out to
// `$(topdir)/construct` at some point.
//

namespace ircd::log
{
	static void check(std::ostream &) noexcept;
	static void handle_to_stdout(bool &, const log &, const level &, const string_view &) noexcept;
	static void handle_to_file(bool &, const log &, const level &, const string_view &) noexcept;

	extern hook::callback log_to_stdout;
	extern hook::callback log_to_file;
}

decltype(ircd::log::log_to_file)
ircd::log::log_to_file
{
	hook, handle_to_file
};

void
ircd::log::handle_to_file(bool &ret,
                          const log &log,
                          const level &lev,
                          const string_view &msg)
noexcept
{
	const auto &conf
	{
		confs.at(lev)
	};

	const bool copy_to_file
	{
		bool(conf.file_enable)
		&& file[lev].is_open()
		&& (log.fmasked || lev == level::CRITICAL)
	};

	ret |= copy_to_file;
	if(!copy_to_file || !msg)
		return;

	file[lev].clear();
	check(file[lev]);
	file[lev].write(data(msg), size(msg));
	if(conf.file_flush)
		file[lev].flush();
}

decltype(ircd::log::log_to_stdout)
ircd::log::log_to_stdout
{
	hook, handle_to_stdout
};

void
ircd::log::handle_to_stdout(bool &ret,
                            const log &log,
                            const level &lev,
                            const string_view &msg)
noexcept
{
	const auto &conf
	{
		confs.at(lev)
	};

	bool
	copy_to_stdout(conf.console_stdout),
	copy_to_stderr(conf.console_stderr);

	copy_to_stdout &= log.cmasked;
	copy_to_stderr &= log.cmasked;

	copy_to_stdout &= !console_quiet_stdout[lev];
	copy_to_stderr &= !console_quiet_stderr[lev];

	// Note that CRITICAL messages unconditionally hit stderr even if their
	// level and facility are muted. The only CRITICAL message which won't
	// hit stderr is the initial startup diagnostic if console logging is
	// quieted at startup.
	copy_to_stderr |= lev == level::CRITICAL && ios::epoch() != 0;

	ret |= copy_to_stdout | copy_to_stderr;
	if(likely(!(copy_to_stdout | copy_to_stderr) || !msg))
		return;

	if(unlikely(copy_to_stderr))
	{
		err_console.clear();
		check(err_console);
		err_console.write(data(msg), size(msg));
	}

	if(likely(copy_to_stdout))
	{
		out_console.clear();
		check(out_console);
		out_console.write(data(msg), size(msg));
		if(conf.console_flush)
			out_console << std::flush;
	}
}

//
// ircd::log util
//

void
ircd::log::check(std::ostream &s)
noexcept try
{
	if(likely(s.good()))
		return;

	thread_local char buf[128];
	snprintf(buf, sizeof(buf), "fatal: log stream good[%d] bad[%d] fail[%d] eof[%d]",
	         s.good(),
	         s.bad(),
	         s.fail(),
	         s.eof());

	fprintf(stderr, "log stream(%p) fatal: %s\n", (const void *)&s, buf);
	fprintf(stdout, "log stream(%p) fatal: %s\n", (const void *)&s, buf);
	fflush(stdout);
	s.exceptions(s.eofbit | s.failbit | s.badbit);
	throw std::runtime_error(buf);
}
catch(const std::exception &e)
{
	fprintf(stderr, "%s\n", e.what());
	fprintf(stdout, "%s\n", e.what());
	fflush(stdout);
	ircd::terminate();
}

ircd::log::level
ircd::log::reflect(const string_view &f)
{
	if(f == "CRITICAL")    return level::CRITICAL;
	if(f == "ERROR")       return level::ERROR;
	if(f == "DERROR")      return level::DERROR;
	if(f == "DWARNING")    return level::DWARNING;
	if(f == "WARNING")     return level::WARNING;
	if(f == "NOTICE")      return level::NOTICE;
	if(f == "INFO")        return level::INFO;
	if(f == "DEBUG")       return level::DEBUG;

	throw ircd::error
	{
		"'%s' is not a recognized log level", f
	};
}

ircd::string_view
ircd::log::reflect(const level &f)
{
	switch(f)
	{
		case level::CRITICAL:   return "CRITICAL";
		case level::ERROR:      return "ERROR";
		case level::DERROR:     return "ERROR";
		case level::WARNING:    return "WARNING";
		case level::DWARNING:   return "WARNING";
		case level::INFO:       return "INFO";
		case level::NOTICE:     return "NOTICE";
		case level::DEBUG:      return "DEBUG";
		case level::_NUM_:      break; // Allows -Wswitch to remind developer to add reflection here
	};

	throw panic
	{
		"'%d' is not a recognized log level", int(f)
	};
}

decltype(ircd::log::console_ansi)
ircd::log::console_ansi
{{
	"\033[1;5;37;45m"_sv,     // CRITICAL
	"\033[1;37;41m"_sv,       // ERROR
	"\033[0;30;43m"_sv,       // WARNING
	"\033[1;37;46m"_sv,       // NOTICE
	"\033[1;37;42m"_sv,       // INFO
	"\033[0;31;47m"_sv,       // DERROR
	"\033[0;30;47m"_sv,       // DWARNING
	"\033[1;30;47m"_sv,       // DEBUG
}};

bool
ircd::log::is_conf_mask_console(const string_view &name)
{
	if(token_exists(string_view(mask_console), ' ', name))
		return true;

	if(token_exists(string_view(unmask_console), ' ', name))
		return false;

	// When nothing is in the list then we consider this item masked.
	return empty(string_view(mask_console));
}

bool
ircd::log::is_conf_mask_file(const string_view &name)
{
	if(token_exists(string_view(mask_file), ' ', name))
		return true;

	if(token_exists(string_view(unmask_file), ' ', name))
		return false;

	// When nothing is in the list then we consider this item masked.
	return empty(string_view(mask_file));
}

decltype(ircd::log::unmask_file)
ircd::log::unmask_file
{
	{
		{ "name",     "ircd.log.unmask.file" },
		{ "default",  string_view{}          },
	}, []
	{
		if(!empty(string_view(unmask_file)))
			file_unmask(tokens<std::vector>(unmask_file, ' '));
	}
};

decltype(ircd::log::unmask_console)
ircd::log::unmask_console
{
	{
		{ "name",     "ircd.log.unmask.console" },
		{ "default",  string_view{}             },
	}, []
	{
		if(!empty(string_view(unmask_console)))
			console_unmask(tokens<std::vector>(unmask_console, ' '));
	}
};

decltype(ircd::log::mask_file)
ircd::log::mask_file
{
	{
		{ "name",     "ircd.log.mask.file" },
		{ "default",  string_view{}        },
	}, []
	{
		if(!empty(string_view(mask_file)))
			file_mask(tokens<std::vector>(mask_file, ' '));
	}
};

decltype(ircd::log::mask_console)
ircd::log::mask_console
{
	{
		{ "name",     "ircd.log.mask.console" },
		{ "default",  string_view{}           },
	}, []
	{
		if(!empty(string_view(mask_console)))
			console_mask(tokens<std::vector>(mask_console, ' '));
	}
};

decltype(ircd::log::confs)
ircd::log::confs
{{
	// CRITICAL
	{
		// file enable
		{
			{ "name",     "ircd.log.critical.file.enable" },
			{ "default",  true                            },
		},

		// file flush
		{
			{ "name",     "ircd.log.critical.file.flush" },
			{ "default",  true                           },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.critical.console.stdout" },
			{ "default",  true                               },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.critical.console.stderr" },
			{ "default",  false                              },
		},

		// console flush
		{
			{ "name",     "ircd.log.critical.console.flush" },
			{ "default",  true                              },
		},
	},

	// ERROR
	{
		// file enable
		{
			{ "name",     "ircd.log.error.file.enable" },
			{ "default",  false                        },
		},

		// file flush
		{
			{ "name",     "ircd.log.error.file.flush" },
			{ "default",  true                        },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.error.console.stdout" },
			{ "default",  true                            },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.error.console.stderr" },
			{ "default",  false                           },
		},

		// console flush
		{
			{ "name",     "ircd.log.error.console.flush" },
			{ "default",  true                           },
		},
	},

	// WARNING
	{
		// file enable
		{
			{ "name",     "ircd.log.warning.file.enable" },
			{ "default",  false                          },
		},

		// file flush
		{
			{ "name",     "ircd.log.warning.file.flush" },
			{ "default",  false                         },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.warning.console.stdout" },
			{ "default",  true                              },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.warning.console.stderr" },
			{ "default",  false                             },
		},

		// console flush
		{
			{ "name",     "ircd.log.warning.console.flush" },
			{ "default",  true                             },
		},
	},

	// NOTICE
	{
		// file enable
		{
			{ "name",     "ircd.log.notice.file.enable" },
			{ "default",  true                          },
		},

		// file flush
		{
			{ "name",     "ircd.log.notice.file.flush" },
			{ "default",  true                         },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.notice.console.stdout" },
			{ "default",  true                             },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.notice.console.stderr" },
			{ "default",  false                            },
		},

		// console flush
		{
			{ "name",     "ircd.log.notice.console.flush" },
			{ "default",  true                            },
		},
	},

	// INFO
	{
		// file enable
		{
			{ "name",     "ircd.log.info.file.enable" },
			{ "default",  false                       },
		},

		// file flush
		{
			{ "name",     "ircd.log.info.file.flush" },
			{ "default",  false                      },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.info.console.stdout" },
			{ "default",  true                           },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.info.console.stderr" },
			{ "default",  false                          },
		},

		// console flush
		{
			{ "name",     "ircd.log.info.console.flush" },
			{ "default",  true                          },
		},
	},

	// DERROR
	{
		// file enable
		{
			{ "name",     "ircd.log.derror.file.enable" },
			{ "default",  ircd::debugmode               },
		},

		// file flush
		{
			{ "name",     "ircd.log.derror.file.flush" },
			{ "default",  false                        },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.derror.console.stdout" },
			{ "default",  true                             },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.derror.console.stderr" },
			{ "default",  false                            },
		},

		// console flush
		{
			{ "name",     "ircd.log.derror.console.flush" },
			{ "default",  true                            },
		},
	},

	// DWARNING
	{
		// file enable
		{
			{ "name",     "ircd.log.dwarning.file.enable" },
			{ "default",  ircd::debugmode                 },
		},

		// file flush
		{
			{ "name",     "ircd.log.dwarning.file.flush" },
			{ "default",  false                          },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.dwarning.console.stdout" },
			{ "default",  true                               },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.dwarning.console.stderr" },
			{ "default",  false                              },
		},

		// console flush
		{
			{ "name",     "ircd.log.dwarning.console.flush" },
			{ "default",  false                             },
		},
	},

	// DEBUG
	{
		// file enable
		{
			{ "name",     "ircd.log.debug.file.enable" },
			{ "default",  ircd::debugmode              },
		},

		// file flush
		{
			{ "name",     "ircd.log.debug.file.flush" },
			{ "default",  false                       },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.debug.console.stdout" },
			{ "default",  true                            },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.debug.console.stderr" },
			{ "default",  false                           },
		},

		// console flush
		{
			{ "name",     "ircd.log.debug.console.flush" },
			{ "default",  false                          },
		},
	}
}};
