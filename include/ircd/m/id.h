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
#define HAVE_IRCD_M_ID_H

namespace ircd::m
{
	struct id;

	IRCD_M_EXCEPTION(error, INVALID_MXID, http::BAD_REQUEST)
	IRCD_M_EXCEPTION(INVALID_MXID, BAD_SIGIL, http::BAD_REQUEST)

	bool my(const id &);
	bool is_sigil(const char &c) noexcept;
	bool has_sigil(const string_view &) noexcept;
}

/// (Appendix 4.2) Common Identifier Format
///
/// The Matrix protocol uses a common format to assign unique identifiers to
/// a number of entities, including users, events and rooms. Each identifier
/// takes the form: `&localpart:domain` where & represents a 'sigil' character;
/// domain is the server name of the homeserver which allocated the identifier,
/// and localpart is an identifier allocated by that homeserver. The precise
/// grammar defining the allowable format of an identifier depends on the type
/// of identifier.
///
/// This structure is an interface to a string representing an mxid. The m::id
/// itself is just a string_view over some existing data. m::id::buf is an
/// m::id with an internal array providing the buffer.
///
struct ircd::m::id
:string_view
{
	struct event;
	struct user;
	struct room;
	struct room_alias;
	struct group;
	struct origin;
	struct device;

	enum sigil :char;
	template<class T, size_t SIZE = 256> struct buf;
	template<class it> struct input;
	template<class it> struct output;
	struct parser;
	struct printer;
	struct validator;

	struct parser static const parser;
	struct printer static const printer;
	struct validator static const validator;

  public:
	// Extract elements
	string_view local() const;
	string_view host() const;
	string_view name() const;
	string_view hostname() const;
	uint16_t hostport() const;

	IRCD_USING_OVERLOAD(generate, m::generate);

	id(const enum sigil &, const mutable_buffer &, const generate_t &, const string_view &host);
	id(const enum sigil &, const mutable_buffer &, const string_view &local, const string_view &host);
	id(const enum sigil &, const mutable_buffer &, const string_view &id);
	id(const enum sigil &, const string_view &id);
	id(const string_view &id);
	id() = default;
};

/// (4.2) The sigil characters
enum ircd::m::id::sigil
:char
{
	USER        = '@',     ///< User ID (4.2.1)
	EVENT       = '$',     ///< Event ID (4.2.2)
	ROOM        = '!',     ///< Room ID (4.2.2)
	ROOM_ALIAS  = '#',     ///< Room alias (4.2.3)
	GROUP       = '+',     ///< Group ID (experimental)
	ORIGIN      = ':',     ///< Origin ID (experimental)
	DEVICE      = '%',     ///< Device ID (experimental)
};

/// (Appendix 4.2.1) User Identifiers
///
/// Users within Matrix are uniquely identified by their Matrix user ID. The
/// user ID is namespaced to the homeserver which allocated the account and
/// has the form: `@localpart:domain` The localpart of a user ID is an opaque
/// identifier for that user. It MUST NOT be empty, and MUST contain only the
/// characters a-z, 0-9, ., _, =, -, and /. The domain of a user ID is the
/// server name of the homeserver which allocated the account. The length of
/// a user ID, including the @ sigil and the domain, MUST NOT exceed 255
/// characters.
///
struct ircd::m::id::user
:ircd::m::id
{
	using buf = m::id::buf<user>;
	template<class... args> user(args&&... a) :m::id{USER, std::forward<args>(a)...} {}
	user() = default;
};

/// (Appendix 4.2.2) Room IDs and Event IDs
///
/// An event has exactly one event ID. An event ID has the format:
/// `$opaque_id:domain` The domain of an event ID is the server name of the
/// homeserver which created the event. The domain is used only for namespacing
/// to avoid the risk of clashes of identifiers between different homeservers.
/// There is no implication that the event in question is still available at
/// the corresponding homeserver. Event IDs are case-sensitive. They are not
/// meant to be human readable.
///
struct ircd::m::id::event
:ircd::m::id
{
	using closure = std::function<void (const id::event &)>;
	using closure_bool = std::function<bool (const id::event &)>;

	using buf = m::id::buf<event>;
	template<class... args> event(args&&... a) :m::id{EVENT, std::forward<args>(a)...} {}
	event() = default;
};

/// (Appendix 4.2.2) Room IDs and Event IDs
///
/// A room has exactly one room ID. A room ID has the format:
/// `!opaque_id:domain` The domain of a room ID is the server name of the
/// homeserver which created the room. The domain is used only for namespacing
/// to avoid the risk of clashes of identifiers between different homeservers.
/// There is no implication that the room in question is still available at
/// the corresponding homeserver. Room IDs are case-sensitive. They are not
/// meant to be human readable.
///
struct ircd::m::id::room
:ircd::m::id
{
	using closure = std::function<void (const id::room &)>;
	using closure_bool = std::function<bool (const id::room &)>;

	using buf = m::id::buf<room>;
	template<class... args> room(args&&... a) :m::id{ROOM, std::forward<args>(a)...} {}
	room() = default;
};

/// (Appendix 4.2.3) Room Aliases
/// A room may have zero or more aliases. A room alias has the format:
/// `#room_alias:domain` The domain of a room alias is the server name of the
/// homeserver which created the alias. Other servers may contact this
/// homeserver to look up the alias. Room aliases MUST NOT exceed 255 bytes
/// (including the # sigil and the domain).
///
struct ircd::m::id::room_alias
:ircd::m::id
{
	using buf = m::id::buf<room_alias>;
	template<class... args> room_alias(args&&... a) :m::id{ROOM_ALIAS, std::forward<args>(a)...} {}
	room_alias() = default;
};

/// Group ID (EXPERIMENTAL)
///
struct ircd::m::id::group
:ircd::m::id
{
	using buf = m::id::buf<group>;
	template<class... args> group(args&&... a) :m::id{GROUP, std::forward<args>(a)...} {}
	group() = default;
};

/// Origin ID (EXPERIMENTAL)
///
struct ircd::m::id::origin
:ircd::m::id
{
	using buf = m::id::buf<origin>;
	template<class... args> origin(args&&... a) :m::id{ORIGIN, std::forward<args>(a)...} {}
	origin() = default;
};

/// Device ID (EXPERIMENTAL)
///
struct ircd::m::id::device
:ircd::m::id
{
	using buf = m::id::buf<device>;
	template<class... args> device(args&&... a) :m::id{DEVICE, std::forward<args>(a)...} {}
	device() = default;
};

// Utilities
namespace ircd::m
{
	id::sigil sigil(const char &c);
	id::sigil sigil(const string_view &id);
	string_view reflect(const id::sigil &);

	// Checks
	bool valid(const id::sigil &, const string_view &) noexcept;
	bool valid_local(const id::sigil &, const string_view &) noexcept;  // Local part is valid
	bool valid_local_only(const id::sigil &, const string_view &) noexcept;  // No :host
	void validate(const id::sigil &, const string_view &);    // valid() | throws
}

/// ID object backed by an internal buffer of default worst-case size.
///
template<class T,
         size_t MAX = 256>
struct ircd::m::id::buf
:T
{
	static constexpr const size_t SIZE
	{
		MAX
	};

  private:
	fixed_buffer<mutable_buffer, SIZE> b;

  public:
	operator const fixed_buffer<mutable_buffer, SIZE> &() const
	{
		return b;
	}

	operator mutable_buffer()
	{
		return b;
	}

	template<class... args>
	buf(args&&... a)
	:T{b, std::forward<args>(a)...}
	{}

	buf() = default;

	buf(buf &&other) noexcept
	:T{b, std::move(other)}
	{}

	buf(const buf &other)
	:T{b, other}
	{}

	buf &operator=(const buf &other)
	{
		static_cast<T &>(*this) = T{b, other};
		return *this;
	}

	buf &operator=(buf &&other) noexcept
	{
		static_cast<T &>(*this) = T{b, std::move(other)};
		return *this;
	}

	buf &operator=(const string_view &s)
	{
		static_cast<T &>(*this) = T{b, s};
		return *this;
	}
};
