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
#define HAVE_IRCD_M_ID_H

namespace ircd::m
{
	IRCD_M_EXCEPTION(error, INVALID_MXID, http::BAD_REQUEST)
	IRCD_M_EXCEPTION(INVALID_MXID, BAD_SIGIL, http::BAD_REQUEST)

	IRCD_OVERLOAD(generate)

	struct id;
}

//
// Interface to a string representing an mxid. The m::id itself is just a
// string_view over some existing data. m::id::buf is an m::id with an
// internal array providing the buffer.
//
struct ircd::m::id
:string_view
{
	struct event;
	struct user;
	struct room;
	struct alias;
	template<class T, size_t SIZE = 256> struct buf;

	enum sigil
	{
		EVENT  = '$',
		USER   = '@',
		ROOM   = '!',
		ALIAS  = '#',
	}
	sigil;

	// Checks
	bool valid() const;                          // Fully qualified mxid
	bool valid_local() const;                    // Local part is valid (may or may not have host)

	// Extract elements
	string_view local() const                    { return split(*this, ':').first;                 }
	string_view host() const                     { return split(*this, ':').second;                }
	string_view name() const                     { return lstrip(local(), '@');                    }

  private:
	static string_view generate_random_timebased(const enum sigil &, char *const &buf, const size_t &max);
	static string_view generate_random_prefixed(const enum sigil &, const string_view &prefix, char *const &buf, const size_t &max);

  public:
	IRCD_USING_OVERLOAD(generate, m::generate);

	id() = default;
	id(const string_view &id);
	id(const enum sigil &, const string_view &id);
	id(const enum sigil &, char *const &buf, const size_t &max, const string_view &id);
	id(const enum sigil &, char *const &buf, const size_t &max, const string_view &name, const string_view &host);
	id(const enum sigil &, char *const &buf, const size_t &max, const generate_t &, const string_view &host);
};

namespace ircd::m
{
	bool valid_sigil(const char &c);
	bool valid_sigil(const string_view &id);
	enum id::sigil sigil(const char &c);
	enum id::sigil sigil(const string_view &id);
	const char *reflect(const enum id::sigil &);
}

//
// ID object backed by an internal buffer. Useful for creating or composing
// a new ID.
//
template<class T,
         size_t MAX = 256>
struct ircd::m::id::buf
:T
{
	static constexpr const size_t &SIZE{MAX};

  private:
	std::array<char, SIZE> b;

  public:
	template<class... args>
	buf(args&&... a)
	:T{b.data(), b.size(), std::forward<args>(a)...}
	{}

	buf() = default;
};

//
// convenience typedefs
//

struct ircd::m::id::event
:ircd::m::id
{
	using buf = m::id::buf<event>;
	template<class... args> event(args&&... a) :m::id{EVENT, std::forward<args>(a)...} {}
	event() = default;
};

struct ircd::m::id::user
:ircd::m::id
{
	using buf = m::id::buf<user>;
	template<class... args> user(args&&... a) :m::id{USER, std::forward<args>(a)...} {}
	user() = default;
};

struct ircd::m::id::room
:ircd::m::id
{
	using buf = m::id::buf<room>;
	template<class... args> room(args&&... a) :m::id{ROOM, std::forward<args>(a)...} {}
	room() = default;
};

struct ircd::m::id::alias
:ircd::m::id
{
	using buf = m::id::buf<alias>;
	template<class... args> alias(args&&... a) :m::id{ALIAS, std::forward<args>(a)...} {}
	alias() = default;
};
