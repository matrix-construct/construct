// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_LOGGER_H

// windows.h #define conflicts with our facility
#ifdef HAVE_WINDOWS_H
#undef ERROR
#endif

namespace ircd
{
	const char *smalldate(const time_t &);
}

/// Logging system
namespace ircd::log
{
	enum facility :int;
	const char *reflect(const facility &);

	struct log;
	struct vlog;
	struct mark;
	struct console_quiet;

	struct critical;
	struct error;
	struct derror;
	struct warning;
	struct dwarning;
	struct notice;
	struct info;
	struct debug;

	bool console_enabled(const facility &);
	void console_disable(const facility &);
	void console_enable(const facility &);

	void flush();
	void close();
	void open();

	void init();
	void fini();
}

enum ircd::log::facility
:int
{
	CRITICAL  = 0,  ///< Catastrophic/unrecoverable; program is in a compromised state.
	ERROR     = 1,  ///< Things that shouldn't happen; user impacted and should know.
	DERROR    = 2,  ///< An error but only worthy of developers in debug mode.
	WARNING   = 3,  ///< Non-impacting undesirable behavior user should know about.
	DWARNING  = 4,  ///< A warning but only for developers in debug mode.
	NOTICE    = 5,  ///< An infrequent important message with neutral or positive news.
	INFO      = 6,  ///< A more frequent message with good news.
	DEBUG     = 7,  ///< Maximum verbosity for developers.
	_NUM_
};

/// A named logger. Create an instance of this to help categorize log messages.
/// All messages sent to this logger will be prefixed with the given name.
/// Admins will use this to create masks to filter log messages. Instances
/// of this class are registered with instance_list for de-confliction and
/// iteration, so the recommended duration of this class is static.
struct ircd::log::log
:instance_list<log>
{
	string_view name;
	char snote;

  public:
	template<class... args> void operator()(const facility &, const char *const &fmt, args&&...);

	template<class... args> void critical(const char *const &fmt, args&&...);
	template<class... args> void error(const char *const &fmt, args&&...);
	template<class... args> void warning(const char *const &fmt, args&&...);
	template<class... args> void notice(const char *const &fmt, args&&...);
	template<class... args> void info(const char *const &fmt, args&&...);

	#ifdef RB_DEBUG
	template<class... args> void derror(const char *const &fmt, args&&...);
	template<class... args> void dwarning(const char *const &fmt, args&&...);
	template<class... args> void debug(const char *const &fmt, args&&...);
	#else // Required for DCE in gcc 6.3.0 20170519
	void derror(const char *const &fmt, ...);
	void dwarning(const char *const &fmt, ...);
	void debug(const char *const &fmt, ...);
	#endif

	log(const string_view &name, const char &snote = '\0');
};

struct ircd::log::vlog
{
	vlog(const facility &, const string_view &name, const char *const &fmt, const va_rtti &ap);
	vlog(const facility &, const char *const &fmt, const va_rtti &ap);
};

struct ircd::log::mark
{
	mark(const facility &, const string_view &msg = {});
	mark(const string_view &msg = {});
};

struct ircd::log::console_quiet
{
	console_quiet(const bool &showmsg = true);
	~console_quiet();
};

#ifdef RB_DEBUG
struct ircd::log::debug
{
	template<class... args>
	debug(const char *const &fmt, args&&... a)
	{
		vlog(facility::DEBUG, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	debug(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::DEBUG, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};
#else
struct ircd::log::debug
{
	debug(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}

	debug(const log &, const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
};
#endif

struct ircd::log::info
{
	template<class... args>
	info(const char *const &fmt, args&&... a)
	{
		vlog(facility::INFO, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	info(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::INFO, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::notice
{
	template<class... args>
	notice(const char *const &fmt, args&&... a)
	{
		vlog(facility::NOTICE, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	notice(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::NOTICE, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};

#ifdef RB_DEBUG
struct ircd::log::dwarning
{
	template<class... args>
	dwarning(const char *const &fmt, args&&... a)
	{
		vlog(facility::DWARNING, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	dwarning(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::DWARNING, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};
#else
struct ircd::log::dwarning
{
	dwarning(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}

	dwarning(const log &, const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
};
#endif

struct ircd::log::warning
{
	template<class... args>
	warning(const char *const &fmt, args&&... a)
	{
		vlog(facility::WARNING, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	warning(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::WARNING, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};

#ifdef RB_DEBUG
struct ircd::log::derror
{
	template<class... args>
	derror(const char *const &fmt, args&&... a)
	{
		vlog(facility::DERROR, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	derror(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::DERROR, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};
#else
struct ircd::log::derror
{
	derror(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}

	derror(const log &, const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
};
#endif

struct ircd::log::error
{
	template<class... args>
	error(const char *const &fmt, args&&... a)
	{
		vlog(facility::ERROR, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	error(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::ERROR, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::critical
{
	template<class... args>
	critical(const char *const &fmt, args&&... a)
	{
		vlog(facility::CRITICAL, fmt, va_rtti{std::forward<args>(a)...});
	}

	template<class... args>
	critical(const log &log, const char *const &fmt, args&&... a)
	{
		vlog(facility::CRITICAL, log.name, fmt, va_rtti{std::forward<args>(a)...});
	}
};

#ifdef RB_DEBUG
template<class... args>
void
ircd::log::log::debug(const char *const &fmt,
                      args&&... a)
{
	operator()(facility::DEBUG, fmt, va_rtti{std::forward<args>(a)...});
}
#else
inline void
ircd::log::log::debug(const char *const &fmt,
                      ...)
{
	// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
}
#endif

template<class... args>
void
ircd::log::log::info(const char *const &fmt,
                     args&&... a)
{
	operator()(facility::INFO, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::notice(const char *const &fmt,
                       args&&... a)
{
	operator()(facility::NOTICE, fmt, va_rtti{std::forward<args>(a)...});
}

#ifdef RB_DEBUG
template<class... args>
void
ircd::log::log::dwarning(const char *const &fmt,
                         args&&... a)
{
	operator()(facility::DWARNING, fmt, va_rtti{std::forward<args>(a)...});
}
#else
inline void
ircd::log::log::dwarning(const char *const &fmt,
                         ...)
{
	// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
}
#endif

template<class... args>
void
ircd::log::log::warning(const char *const &fmt,
                        args&&... a)
{
	operator()(facility::WARNING, fmt, va_rtti{std::forward<args>(a)...});
}

#ifdef RB_DEBUG
template<class... args>
void
ircd::log::log::derror(const char *const &fmt,
                       args&&... a)
{
	operator()(facility::DERROR, fmt, va_rtti{std::forward<args>(a)...});
}
#else
inline void
ircd::log::log::derror(const char *const &fmt,
                       ...)
{
	// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
}
#endif

template<class... args>
void
ircd::log::log::error(const char *const &fmt,
                      args&&... a)
{
	operator()(facility::ERROR, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::critical(const char *const &fmt,
                         args&&... a)
{
	operator()(facility::CRITICAL, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::operator()(const facility &f,
                           const char *const &fmt,
                           args&&... a)
{
	vlog(f, name, fmt, va_rtti{std::forward<args>(a)...});
}
