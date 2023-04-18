// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_STACK_H

namespace ircd::json
{
	struct stack;
}

/// Output stack machine for stringifying JSON as-you-go. This device allows
/// the user to create JSON without knowing the contents when it is first
/// constructed. An object or array is opened and the user can append to the
/// stack creating the members or values or recursing further. The JSON is
/// then closed automatically with exception safety. Partial JSON is written
/// to the buffer as soon as possible.
///
/// The target buffer is not required to maintain earlier output from the same
/// stack or even earlier members and values of the same object or array. The
/// buffer may be smaller than the final JSON output and reused when the user
/// chooses to flush it to some storage or socket. If the buffer becomes full
/// a flush callback is attempted to make space and continue. This can occur
/// while the output is still incomplete JSON.
///
/// The user first creates a master json::stack instance with some reasonable
/// backing buffer. A suite of classes is provided to aid with building the
/// JSON which attach to each other stackfully, and eventually lead to the
/// root. There should only be one "active path" of instances at any given
/// time, ideally following the scope of your code itself. You must force
/// instances to go out of scope to continue at the same recursion depth.
/// This way the json::stack can "follow" your code and "record" the final
/// JSON output while allowing you to free the original resources required
/// for each value.
///
struct ircd::json::stack
{
	struct array;
	struct object;
	struct member;
	struct chase;
	struct const_chase;
	struct checkpoint;
	using flush_callback = std::function<const_buffer (const const_buffer &)>;

	window_buffer buf;
	flush_callback flusher;
	std::exception_ptr eptr;
	checkpoint *cp {nullptr};
	size_t appended {0};
	size_t flushed {0};
	size_t level {0};
	size_t hiwat;                      ///< autoflush watermark
	size_t lowat;                      ///< flush(false) call min watermark

	object *co {nullptr};              ///< The root object instance.
	array *ca {nullptr};               ///< Could be union with top_object but

	void rethrow_exception();
	void append(const size_t expect, const window_buffer::closure &) noexcept;
	void append(const string_view &) noexcept;
	void append(const char &) noexcept;

  public:
	bool opened() const;               ///< Current stacking in progress.
	bool closed() const;               ///< No stacking in progress.
	bool failed() const;               ///< Exception pending in eptr.
	bool clean() const;                ///< Never opened.
	bool done() const;                 ///< Opened and closed.
	size_t remaining() const;
	string_view completed() const;

	size_t invalidate_checkpoints() noexcept;
	bool flush(const bool force = false) noexcept;
	size_t rewind(const size_t bytes) noexcept;
	void clear() noexcept;

	stack(const mutable_buffer &,
	      flush_callback           = {},
	      const size_t &hiwat      = -1,
	      const size_t &lowat      = 0);

	stack(stack &&) noexcept;
	stack(const stack &) = delete;
	~stack() noexcept;

	template<class T> static const T &top(const stack &);
	template<class T> static T &top(stack &);
};

#include "member.h"
#include "object.h"
#include "array.h"
#include "chase.h"
#include "checkpoint.h"

template<>
inline ircd::json::stack::array &
ircd::json::stack::stack::top<ircd::json::stack::array>(stack &s)
{
	return array::top(s);
}

template<>
inline const ircd::json::stack::array &
ircd::json::stack::stack::top<ircd::json::stack::array>(const stack &s)
{
	return array::top(s);
}

template<>
inline ircd::json::stack::object &
ircd::json::stack::stack::top<ircd::json::stack::object>(stack &s)
{
	return object::top(s);
}

template<>
inline const ircd::json::stack::object &
ircd::json::stack::stack::top<ircd::json::stack::object>(const stack &s)
{
	return object::top(s);
}

template<>
inline ircd::json::stack::member &
ircd::json::stack::stack::top<ircd::json::stack::member>(stack &s)
{
	return member::top(s);
}

template<>
inline const ircd::json::stack::member &
ircd::json::stack::stack::top<ircd::json::stack::member>(const stack &s)
{
	return member::top(s);
}

inline ircd::string_view
ircd::json::stack::completed()
const
{
	return buf.completed();
}

inline size_t
ircd::json::stack::remaining()
const
{
	return buf.remaining();
}

inline bool
ircd::json::stack::failed()
const
{
	return bool(eptr);
}

inline bool
ircd::json::stack::done()
const
{
	assert((opened() && level) || !level);
	return closed() && buf.consumed();
}

inline bool
ircd::json::stack::clean()
const
{
	return closed() && !buf.consumed();
}

inline bool
ircd::json::stack::closed()
const
{
	return !opened();
}

inline bool
ircd::json::stack::opened()
const
{
	return co || ca;
}
