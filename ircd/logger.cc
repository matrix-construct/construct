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

	extern const std::array<string_view, num_of<facility>()> default_ansi;
	extern std::array<confs, num_of<facility>()> confs;

	std::array<std::ofstream, num_of<facility>()> file;
	std::array<ulong, num_of<facility>()> console_quiet_stdout;
	std::array<ulong, num_of<facility>()> console_quiet_stderr;

	std::ostream &out_console{std::cout};
	std::ostream &err_console{std::cerr};

	static std::string dir_path();
	static std::string file_path(const facility &);

	static void mkdir();
	static void open(const facility &);
}

struct ircd::log::confs
{
	conf::item<bool> file_enable;
	conf::item<bool> file_flush;
	conf::item<bool> console_stdout;
	conf::item<bool> console_stderr;
	conf::item<bool> console_flush;
	conf::item<std::string> console_ansi;
};

void
ircd::log::init()
{
	if(!ircd::debugmode)
		console_disable(facility::DEBUG);

	mkdir();
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
	const std::string dir
	{
		dir_path()
	};

	if(fs::exists(dir))
		return;

	fs::mkdir(dir);
}

void
ircd::log::open()
{
	for_each<facility>([](const facility &fac)
	{
		const auto &conf(confs.at(fac));
		if(!bool(conf.file_enable))
			return;

		if(file[fac].is_open())
			file[fac].close();

		file[fac].clear();
		file[fac].exceptions(std::ios::badbit | std::ios::failbit);
		open(fac);
	});
}

void
ircd::log::close()
{
	for_each<facility>([](const facility &fac)
	{
		if(file[fac].is_open())
			file[fac].close();
	});
}

void
ircd::log::flush()
{
	for_each<facility>([](const facility &fac)
	{
		file[fac].flush();
	});

	std::flush(out_console);
	std::flush(err_console);
}

void
ircd::log::open(const facility &fac)
try
{
	const auto &mode(std::ios::app);
	const auto &path(file_path(fac));
	file[fac].open(path.c_str(), mode);
}
catch(const std::exception &e)
{
	fprintf(stderr, "!!! Opening log file [%s] failed: %s",
	        file_path(fac).c_str(),
	        e.what());
	throw;
}

std::string
ircd::log::file_path(const facility &fac)
{
	const std::string base
	{
		dir_path()
	};

	const string_view parts[]
	{
		base, reflect(fac)
	};

	return fs::make_path(parts);
}

std::string
ircd::log::dir_path()
{
	const string_view parts[]
	{
		fs::get(fs::LOG)
	};

	return fs::make_path(parts);
}

void
ircd::log::console_enable()
{
	for_each<facility>([](const facility &fac)
	{
		console_enable(fac);
	});
}

void
ircd::log::console_disable()
{
	for_each<facility>([](const facility &fac)
	{
		console_disable(fac);
	});
}

void
ircd::log::console_enable(const facility &fac)
{
	if(console_quiet_stdout[fac])
		console_quiet_stdout[fac]--;

	if(console_quiet_stderr[fac])
		console_quiet_stderr[fac]--;
}

void
ircd::log::console_disable(const facility &fac)
{
	console_quiet_stdout[fac]++;
	console_quiet_stderr[fac]++;
}

bool
ircd::log::console_enabled(const facility &fac)
{
	return !console_quiet_stdout[fac] ||
	       !console_quiet_stderr[fac];
}

void
ircd::log::console_mask(const vector_view<string_view> &list)
{
	for(auto *const &log : log::list)
		log->cmasked = std::find(begin(list), end(list), log->name) != end(list);
}

void
ircd::log::console_unmask(const vector_view<string_view> &list)
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
	for_each<facility>([&msg]
	(const auto &fac)
	{
		mark(fac, msg);
	});
}

ircd::log::mark::mark(const facility &fac,
                      const string_view &msg)
{
	vlog(star, fac, "%s", msg);
}

//
// log
//

/// Linkage for list of named loggers.
template<>
decltype(ircd::instance_list<ircd::log::log>::list)
ircd::instance_list<ircd::log::log>::list
{};

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

//
// vlog
//

namespace ircd::log
{
	static void check(std::ostream &) noexcept;
	static void slog(const log &, const facility &, const window_buffer::closure &) noexcept;
	static void vlog_threadsafe(const log &, const facility &, const string_view &fmt, const va_rtti &ap);
}

decltype(ircd::log::star)
ircd::log::star
{
	"*", '*'
};

decltype(ircd::log::general)
ircd::log::general
{
	"ircd", 'G'
};

/// ircd::log is not thread-safe. This internal function is called when the
/// normal vlog() detects it's not on the main IRCd thread. It then generates
/// the formatted log message on this thread, and posts the message to the
/// main IRCd event loop which is running on the main thread.
void
ircd::log::vlog_threadsafe(const log &log,
                           const facility &fac,
                           const string_view &fmt,
                           const va_rtti &ap)
{
	// Generate the formatted message on this thread first
	std::string str
	{
		fmt::vsnstringf(1024, fmt, ap)
	};

	// The pointer to the logger is copied to the main thread.
	auto *const logp{&log};
	ircd::post([fac, str(std::move(str)), logp]
	{
		// If that named logger was destroyed while this closure was
		// travelling to the main thread then we just discard this message.
		if(!log::exists(logp))
			return;

		slog(*logp, fac, [&str](const mutable_buffer &out) -> size_t
		{
			return copy(out, string_view{str});
		});
	});
}

ircd::log::vlog::vlog(const log &log,
                      const facility &fac,
                      const string_view &fmt,
                      const va_rtti &ap)
{
	if(!is_main_thread())
	{
		vlog_threadsafe(log, fac, fmt, ap);
		return;
	}

	slog(log, fac, [&fmt, &ap](const mutable_buffer &out) -> size_t
	{
		return fmt::vsprintf(out, fmt, ap);
	});
}

namespace ircd::log
{
	// linkage for slog() reentrance assertion
	bool entered;

	static bool can_skip(const log &, const facility &);
}

void
ircd::log::slog(const log &log,
                const facility &fac,
                const window_buffer::closure &closure)
noexcept
{
	const auto &conf(confs.at(fac));
	if(can_skip(log, fac))
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
	static char buf[1024], date[64];

	// Maximum size of log line leaving 2 characters for \r\n
	const size_t max(sizeof(buf) - 2);

	// Compose the prefix sequence into the buffer through stringstream
	std::stringstream s;
	pubsetbuf(s, buf);
	s << microtime(date)
	  << ' '
	  << string_view{conf.console_ansi}
	  << std::setw(8)
	  << std::right
	  << reflect(fac)
	  << (string_view{conf.console_ansi}? "\033[0m " : " ")
//	  << (log.snote? log.snote : '-')
	  << std::setw(9)
	  << std::right
	  << log.name
	  << ' '
	  << std::setw(8)
	  << trunc(ctx::name(), 8)
	  << ' '
	  << std::setw(6)
	  << std::right
	  << ctx::id()
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

	// Closure to copy the message to various places
	assert(len <= sizeof(buf));
	const string_view msg{buf, len};
	const auto write{[&msg](std::ostream &s)
	{
		check(s);
		s.write(data(msg), size(msg));
	}};

	// copy to std::cerr
	if(log.cmasked && bool(conf.console_stderr) && !console_quiet_stderr[fac])
	{
		err_console.clear();
		write(err_console);
	}

	// copy to std::cout
	if(log.cmasked && bool(conf.console_stdout) && !console_quiet_stdout[fac])
	{
		out_console.clear();
		write(out_console);
		if(conf.console_flush)
			std::flush(out_console);
	}

	// copy to file
	if(log.fmasked && file[fac].is_open())
	{
		file[fac].clear();
		write(file[fac]);
		if(conf.file_flush)
			std::flush(file[fac]);
	}
}

bool
ircd::log::can_skip(const log &log,
                    const facility &fac)
{
	const auto &conf(confs.at(fac));

	// When all of these conditions are true there is no possible log output
	// so we can bail real quick.
	if(!file[fac].is_open() && !bool(conf.console_stdout) && !bool(conf.console_stderr))
		return true;

	// Same for this set of conditions...
	if((!file[fac].is_open() || !log.fmasked) && (!log.cmasked || !console_enabled(fac)))
		return true;

	return false;
}

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
	fflush(stderr);
	fflush(stdout);
	s.exceptions(s.eofbit | s.failbit | s.badbit);
	throw std::runtime_error(buf);
}
catch(const std::exception &e)
{
	fprintf(stderr, "%s\n", e.what());
	fprintf(stdout, "%s\n", e.what());
	fflush(stderr);
	fflush(stdout);
	ircd::terminate();
}

ircd::log::facility
ircd::log::reflect(const string_view &f)
{
	if(f == "CRITICAL")    return facility::CRITICAL;
	if(f == "ERROR")       return facility::ERROR;
	if(f == "DERROR")      return facility::DERROR;
	if(f == "DWARNING")    return facility::DWARNING;
	if(f == "WARNING")     return facility::WARNING;
	if(f == "NOTICE")      return facility::NOTICE;
	if(f == "INFO")        return facility::INFO;
	if(f == "DEBUG")       return facility::DEBUG;

	throw ircd::error
	{
		"'%s' is not a recognized log facility", f
	};
}

ircd::string_view
ircd::log::reflect(const facility &f)
{
	switch(f)
	{
		case facility::CRITICAL:   return "CRITICAL";
		case facility::ERROR:      return "ERROR";
		case facility::DERROR:     return "ERROR";
		case facility::WARNING:    return "WARNING";
		case facility::DWARNING:   return "WARNING";
		case facility::INFO:       return "INFO";
		case facility::NOTICE:     return "NOTICE";
		case facility::DEBUG:      return "DEBUG";
		case facility::_NUM_:      break; // Allows -Wswitch to remind developer to add reflection here
	};

	throw assertive
	{
		"'%d' is not a recognized log facility", int(f)
	};
}

decltype(ircd::log::default_ansi)
ircd::log::default_ansi
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

const char *
ircd::smalldate(const time_t &ltime)
{
	static const size_t MAX_DATE_STRING
	{
		32 // maximum string length for a date string (ircd_defs.h)
	};

	struct tm lt;
	localtime_r(&ltime, &lt);
	thread_local char buf[MAX_DATE_STRING];
	snprintf(buf, sizeof(buf), "%d/%d/%d %02d.%02d",
	         lt.tm_year + 1900,
	         lt.tm_mon + 1,
	         lt.tm_mday,
	         lt.tm_hour,
	         lt.tm_min);

	return buf;
}

decltype(ircd::log::confs)
ircd::log::confs
{{
	// CRITICAL
	{
		// file enable
		{
			{ "name",     "ircd.log.critical.file.enable" },
			{ "default",  false                           },
		},

		// file flush
		{
			{ "name",     "ircd.log.critical.file.flush" },
			{ "default",  false                          },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.critical.console.stdout" },
			{ "default",  true                               },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.critical.console.stderr" },
			{ "default",  true                               },
		},

		// console flush
		{
			{ "name",     "ircd.log.critical.console.flush" },
			{ "default",  true                              },
		},

		// console ansi
		{
			{ "name",     "ircd.log.critical.console.ansi"    },
			{ "default",  default_ansi.at(facility::CRITICAL) },
		}
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
			{ "default",  false                       },
		},

		// console enable on stdout
		{
			{ "name",     "ircd.log.error.console.stdout" },
			{ "default",  true                            },
		},

		// console enable on stderr
		{
			{ "name",     "ircd.log.error.console.stderr" },
			{ "default",  true                            },
		},

		// console flush
		{
			{ "name",     "ircd.log.error.console.flush" },
			{ "default",  true                           },
		},

		// console ansi
		{
			{ "name",     "ircd.log.error.console.ansi"    },
			{ "default",  default_ansi.at(facility::ERROR) },
		}
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

		// console ansi
		{
			{ "name",     "ircd.log.warning.console.ansi"    },
			{ "default",  default_ansi.at(facility::WARNING) },
		}
	},

	// NOTICE
	{
		// file enable
		{
			{ "name",     "ircd.log.notice.file.enable" },
			{ "default",  false                         },
		},

		// file flush
		{
			{ "name",     "ircd.log.notice.file.flush" },
			{ "default",  false                        },
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

		// console ansi
		{
			{ "name",     "ircd.log.notice.console.ansi"     },
			{ "default",  default_ansi.at(facility::NOTICE)  },
		}
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

		// console ansi
		{
			{ "name",     "ircd.log.info.console.ansi"     },
			{ "default",  default_ansi.at(facility::INFO)  },
		}
	},

	// DERROR
	{
		// file enable
		{
			{ "name",     "ircd.log.derror.file.enable" },
			{ "default",  false                         },
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

		// console ansi
		{
			{ "name",     "ircd.log.derror.console.ansi"     },
			{ "default",  default_ansi.at(facility::DERROR)  },
		}
	},

	// DWARNING
	{
		// file enable
		{
			{ "name",     "ircd.log.dwarning.file.enable" },
			{ "default",  false                           },
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

		// console ansi
		{
			{ "name",     "ircd.log.dwarning.console.ansi"     },
			{ "default",  default_ansi.at(facility::DWARNING)  },
		}
	},

	// DEBUG
	{
		// file enable
		{
			{ "name",     "ircd.log.debug.file.enable" },
			{ "default",  false                        },
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

		// console ansi
		{
			{ "name",     "ircd.log.debug.console.ansi"    },
			{ "default",  default_ansi.at(facility::DEBUG) },
		}
	}
}};
