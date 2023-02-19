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
#define HAVE_IRCD_JSON_STACK_ARRAY_H

/// stack::array is constructed under the scope of either a stack::member,
/// or a stack::array, or a stack itself. stack::object and stack::array
/// can be constructed directly under its scope, but not stack::member.
///
/// The same behavior as described by stack::object documentation applies
/// here translated to arrays.
///
struct ircd::json::stack::array
{
	member m;                          ///< optional internal member
	stack *s {nullptr};                ///< root stack ref
	member *pm {nullptr};              ///< parent member (if value of one)
	array *pa {nullptr};               ///< parent array (if value in one)
	object *co {nullptr};              ///< current child object
	array *ca {nullptr};               ///< current child array
	size_t vc {0};                     ///< values witnessed (monotonic)

	void _pre_append();
	void _post_append();

  public:
	template<class... T> void append(const json::tuple<T...> &);
	void append(const json::value &);

	array(member &pm);                 ///< Array is value of the named member
	array(array &pa);                  ///< Array is value in the array
	array(object &po, const string_view &name);
	array(stack &s, const string_view &name);
	array(stack &s);
	array(const array &) = delete;
	array(array &&) noexcept;
	~array() noexcept;

	static const array &top(const stack &);
	static array &top(stack &);
};

template<class... T>
inline void
ircd::json::stack::array::append(const json::tuple<T...> &t)
{
	_pre_append();
	const unwind post{[this]
	{
		_post_append();
	}};

	s->append(serialized(t), [&t](mutable_buffer buf)
	{
		return ircd::size(stringify(buf, t));
	});
}

inline void
ircd::json::stack::array::_post_append()
{
	++vc;
}
