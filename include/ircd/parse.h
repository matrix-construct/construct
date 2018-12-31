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
#define HAVE_IRCD_PARSE_H

namespace ircd
{
	struct parse;
}

/// NOTE: This interface will likely be removed. It predates the entire
/// NOTE: ircd::buffer system and only has very few unique qualities left
/// NOTE: which have not been replicated within ircd::buffer yet.
///
struct ircd::parse
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, grammar_error)
	IRCD_EXCEPTION(error, syntax_error)
	IRCD_EXCEPTION(error, buffer_error)

	using read_closure = std::function<void (char *&, char *)>;
	using parse_closure = std::function<bool (const char *&, const char *)>;

	struct grammar;
	struct capstan;
	struct buffer;
};

struct ircd::parse::buffer
{
	char *base;                                  // Lowest address of the buffer (const)
	const char *parsed;                          // Data between [base, parsed] is completed
	char *read;                                  // Data between [parsed, read] is unparsed
	char *stop;                                  // Data between [read, stop] is unreceived (const)

	size_t size() const                          { return stop - base;                             }
	size_t completed() const                     { return parsed - base;                           }
	size_t unparsed() const                      { return read - parsed;                           }
	size_t remaining() const                     { return stop - read;                             }

	void discard();
	void remove();

	buffer(const buffer &old, const mutable_buffer &mb)
	:base{data(mb)}
	,parsed{data(mb)}
	,read{data(mb) + old.unparsed()}
	,stop{data(mb) + ircd::size(mb)}
	{
		memmove(base, old.base, old.unparsed());
	}

	buffer(const mutable_buffer &mb)
	:base{data(mb)}
	,parsed{data(mb)}
	,read{data(mb)}
	,stop{data(mb) + ircd::size(mb)}
	{}

	template<size_t N> buffer(const buffer &old, char (&buf)[N])
	:buffer{old, buf}
	{}
};

struct ircd::parse::capstan
{
	const char *&parsed;
	char *&read;
	char *stop;

	size_t unparsed() const                      { return read - parsed;                           }
	size_t remaining() const                     { return stop - read;                             }

	read_closure reader;
	void operator()(const parse_closure &);

	capstan(const char *&parsed, char *&read, char *const &max, const decltype(reader) &reader = nullptr);
	capstan(buffer &, const decltype(reader) &reader = nullptr);
};

inline
ircd::parse::capstan::capstan(buffer &buffer,
                              const decltype(reader) &reader)
:parsed{buffer.parsed}
,read{buffer.read}
,stop{buffer.stop}
,reader{reader}
{
}

inline
ircd::parse::capstan::capstan(const char *&parsed,
                              char *&read,
                              char *const &max,
                              const decltype(reader) &reader)
:parsed{parsed}
,read{read}
,stop{max}
,reader{reader}
{
}

inline void
ircd::parse::capstan::operator()(const parse_closure &pc)
try
{
	while(!pc(parsed, const_cast<const char *>(read)) && read != stop)
		reader(read, stop);
}
catch(const std::bad_function_call &e)
{
	throw assertive
	{
		"Invalid parse (parsed:%p read:%p stop:%p unparsed:%zu remaining: %zu)",
		parsed,
		read,
		stop,
		unparsed(),
		remaining()
	};
}

inline void
ircd::parse::buffer::remove()
{
	memmove(base, parsed, unparsed());
	read = base + unparsed();
	parsed = base;
}

inline void
ircd::parse::buffer::discard()
{
	read -= unparsed();
}
