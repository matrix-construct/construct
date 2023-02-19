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
#define HAVE_IRCD_JSON_STACK_OBJECT_H

/// stack::object is constructed under the scope of either a stack::member,
/// or a stack::array, or a stack itself. Only stack::member can be
/// constructed directly under its scope.
///
/// For a stack::member parent, the named member is waiting for this value
/// after leaving the stack at ':' after the name, this object will then
/// print '{' and dtor with '}' and then return to the stack::member which
/// will then return to its parent object.
///
/// For a stack::array parent, the stack may have been left at '[' or ','
/// but either way this object will then print '{' and dtor with '}' and
/// then return to the stack::array.
///
/// For a stack itself, this object is considered the "top object" and will
/// open the stack with '{' and accept member instances under its scope
/// until closing the stack with '}' after which the stack is done()
///
struct ircd::json::stack::object
{
	member m;                          ///< optional internal member
	stack *s {nullptr};                ///< root stack ref
	member *pm {nullptr};              ///< parent member (if value of one)
	array *pa {nullptr};               ///< parent array (if value in one)
	member *cm {nullptr};              ///< current child member
	size_t mc {0};                     ///< members witnessed (monotonic)

  public:
	template<class... T> void append(const json::tuple<T...> &);
	void append(const json::object &);

	object(stack &s);                  ///< Object is top
	object(array &pa);                 ///< Object is value in the array
	object(member &pm);                ///< Object is value of named member
	object(object &po, const string_view &name);
	object(stack &s, const string_view &name);
	object(object &&) noexcept;
	object(const object &) = delete;
	~object() noexcept;

	static const object &top(const stack &);
	static object &top(stack &);
};

template<class... T>
inline void
ircd::json::stack::object::append(const json::tuple<T...> &t)
{
	for_each(t, [this](const auto &name, const auto &_value)
	{
		const json::value value
		{
			_value
		};

		if(defined(value))
			json::stack::member
			{
				*this, name, value
			};
	});
}
