/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
 *
 */

#pragma once
#define HAVE_IRCD_PARSE_H

namespace ircd {

struct parse
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, grammar_error)
	IRCD_EXCEPTION(error, syntax_error)

	using read_closure = std::function<void (char *&, char *)>;
	using parse_closure = std::function<bool (const char *&, const char *)>;

	struct grammar;
	struct context;
	struct buffer;
};

struct parse::grammar
{
	static std::map<std::string_view, const grammar *> grammars;

	const char *const name;

  private:
	decltype(grammars)::const_iterator grammars_it;

  public:
	grammar(const char *const &name);
	~grammar() noexcept;
};

struct parse::buffer
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

	buffer(const buffer &old, char *const &start, char *const &stop)
	:base{start}
	,parsed{start}
	,read{start + old.unparsed()}
	,stop{stop}
	{
		memmove(base, old.base, old.unparsed());
	}

	buffer(char *const &start, char *const &stop)
	:base{start}
	,parsed{start}
	,read{start}
	,stop{stop}
	{}

	template<size_t N> buffer(const buffer &old, char (&buf)[N])
	:buffer{old, buf, buf + N}
	{}

	template<size_t N> buffer(char (&buf)[N])
	:buffer{buf, buf + N}
	{}
};

struct parse::context
{
	const char *&parsed;
	char *&read;
	char *stop;

	size_t unparsed() const                      { return read - parsed;                           }
	size_t remaining() const                     { return stop - read;                             }

	read_closure reader;
	void operator()(const parse_closure &);

	context(const char *&parsed, char *&read, char *const &max, const decltype(reader) &reader = nullptr);
	context(buffer &, const decltype(reader) &reader = nullptr);
};

} // namespace ircd

inline
ircd::parse::context::context(buffer &buffer,
                              const decltype(reader) &reader)
:parsed{buffer.parsed}
,read{buffer.read}
,stop{buffer.stop}
,reader{reader}
{
}

inline
ircd::parse::context::context(const char *&parsed,
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
ircd::parse::context::operator()(const parse_closure &pc)
{
	while(!pc(parsed, const_cast<const char *>(read)))
		reader(read, stop);
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
