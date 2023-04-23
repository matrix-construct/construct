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
#define HAVE_IRCD_JSON_STACK_MEMBER_H

/// stack::member is an intermediary that is constructed under the scope of
/// a parent stack::object. It takes a name argument. It then requires one
/// object or array be constructed under its scope as its value, or a
/// json::value / already strung JSON must be appended as its value.
///
/// If the value is supplied in the constructor argument an instance of
/// this class does not have to be held (use constructor as function).
///
struct ircd::json::stack::member
{
	stack *s {nullptr};               ///< root stack ref
	object *po {nullptr};             ///< parent object
	string_view name;                 ///< member name state
	object *co {nullptr};             ///< current child object
	array *ca {nullptr};              ///< current child array
	bool vc {false};                  ///< value witnessed

	void _pre_append();
	void _post_append();

  public:
	template<class... T> void append(const json::tuple<T...> &);
	void append(const json::value &);

	member(object &po, const string_view &name);
	member(stack &s, const string_view &name);
	member(object &po, const string_view &name, const json::value &v);
	member(stack &s, const string_view &name, const json::value &);
	template<class... T> member(object &po, const string_view &name, const json::tuple<T...> &t);
	template<class... T> member(stack &s, const string_view &name, const json::tuple<T...> &t);
	explicit member(object &, const json::object::member &);
	explicit member(stack &, const json::object::member &);
	explicit member(object &, const json::member &);
	explicit member(stack &, const json::member &);
	member() = default;
	member(const member &) = delete;
	member(member &&) noexcept;
	~member() noexcept;

	static const member &top(const stack &);
	static member &top(stack &);
};

template<class... T>
inline
ircd::json::stack::member::member(stack &s,
                                  const string_view &name,
                                  const json::tuple<T...> &t)
:member
{
	stack::top<object>(s), name, t
}
{}

template<class... T>
inline
ircd::json::stack::member::member(object &po,
                                  const string_view &name,
                                  const json::tuple<T...> &t)
:member{po, name}
{
	append(t);
}

template<class... T>
inline void
ircd::json::stack::member::append(const json::tuple<T...> &t)
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
ircd::json::stack::member::_pre_append()
{
	assert(!vc);
}

inline void
ircd::json::stack::member::_post_append()
{
	vc |= true;
}
