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
	struct console_quiet;

	void vlog(const facility &, const std::string &name, const char *const &fmt, const va_rtti &ap);
	void vlog(const facility &, const char *const &fmt, const va_rtti &ap);
	void mark(const facility &, const char *const &msg = nullptr);
	void mark(const char *const &msg = nullptr);

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

class ircd::log::log
{
	std::string name;

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

	log(const std::string &name, const char &snote);
	log(const std::string &name);
};

struct ircd::log::console_quiet
{
	console_quiet(const bool &showmsg = true);
	~console_quiet();
};

struct ircd::log::debug
{
	#ifdef RB_DEBUG
	template<class... args>
	debug(const char *const &fmt, args&&... a)
	{
		vlog(facility::DEBUG, fmt, va_rtti{std::forward<args>(a)...});
	}
	#else
	debug(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
	#endif
};

struct ircd::log::info
{
	template<class... args>
	info(const char *const &fmt, args&&... a)
	{
		vlog(facility::INFO, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::notice
{
	template<class... args>
	notice(const char *const &fmt, args&&... a)
	{
		vlog(facility::NOTICE, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::dwarning
{
	#ifdef RB_DEBUG
	template<class... args>
	dwarning(const char *const &fmt, args&&... a)
	{
		vlog(facility::DWARNING, fmt, va_rtti{std::forward<args>(a)...});
	}
	#else
	dwarning(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
	#endif
};

struct ircd::log::warning
{
	template<class... args>
	warning(const char *const &fmt, args&&... a)
	{
		vlog(facility::WARNING, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::derror
{
	#ifdef RB_DEBUG
	template<class... args>
	derror(const char *const &fmt, args&&... a)
	{
		vlog(facility::DERROR, fmt, va_rtti{std::forward<args>(a)...});
	}
	#else
	derror(const char *const &, ...)
	{
		// Required in gcc 6.3.0 20170519, template param packs are not DCE'ed
	}
	#endif
};

struct ircd::log::error
{
	template<class... args>
	error(const char *const &fmt, args&&... a)
	{
		vlog(facility::ERROR, fmt, va_rtti{std::forward<args>(a)...});
	}
};

struct ircd::log::critical
{
	template<class... args>
	critical(const char *const &fmt, args&&... a)
	{
		vlog(facility::CRITICAL, fmt, va_rtti{std::forward<args>(a)...});
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
