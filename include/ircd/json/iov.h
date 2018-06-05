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
#define HAVE_IRCD_JSON_IOV_H

namespace ircd::json
{
	struct iov;

	template<class node, size_t SIZE, class T> iov make_iov(node (&)[SIZE], T&& t);
	template<class node, class T> iov make_iov(node *const &, const size_t &, T&& t);
}

/// A forward list to compose JSON efficiently on the stack.
///
/// The IOV gathers members for a JSON object being assembled from various
/// sources and presents an iteration to a generator. This prevents the need
/// for multiple generations and copying to occur before the final JSON is
/// realized, if ever.
///
/// Add and remove items on the IOV by construction and destruction one of
/// the node objects. The IOV has a standard forward list interface, only use
/// that to observe and sort/rearrange the IOV. Do not add or remove things
/// that way.
///
/// Nodes support a single member each. To support initializer_list syntax
/// the iov allocates and internally manages the iov node that should have
/// been on your stack.
///
struct ircd::json::iov
:ircd::iov<ircd::json::member>
{
	struct push;
	struct add;
	struct add_if;
	struct set;
	struct set_if;
	struct defaults;
	struct defaults_if;

	IRCD_EXCEPTION(json::error, error);
	IRCD_EXCEPTION(error, exists);
	IRCD_EXCEPTION(error, oversize);

	static const size_t MAX_SIZE;

  public:
	bool has(const string_view &key) const;
	const value &at(const string_view &key) const;
	value &at(const string_view &key);

	iov() = default;

	friend string_view stringify(mutable_buffer &, const iov &);
	friend std::ostream &operator<<(std::ostream &, const iov &);
	friend size_t serialized(const iov &);
};

/// Unconditionally append a member to the object vector
struct ircd::json::iov::push
:protected ircd::json::iov::node
{
	operator const member &() const;
	operator member &();

	push(iov &iov, member m)
	:node(iov, std::move(m))
	{}

	push() = default;
};

/// Add a new member to the object vector; throws if exists
struct ircd::json::iov::add
:protected ircd::json::iov::node
{
	add(iov &, member);
	add() = default;
};

/// iov::add only if the bool argument is true for your condition
struct ircd::json::iov::add_if
:protected ircd::json::iov::node
{
	add_if(iov &, const bool &, const string_view &, const std::function<json::value ()> &);
	add_if(iov &, const bool &, member);
	add_if() = default;
};

/// Add or overwrite a member in the object vector.
struct ircd::json::iov::set
:protected ircd::json::iov::node
{
	set(iov &, member);
	set() = default;
};

/// iov::set only if the bool argument is true for your condition
struct ircd::json::iov::set_if
:protected ircd::json::iov::node
{
	set_if(iov &, const bool &, member);
	set_if() = default;
};

/// Add member to the object vector if doesn't exist; otherwise ignored
struct ircd::json::iov::defaults
:protected ircd::json::iov::node
{
	defaults(iov &, member);
	defaults() = default;
};

/// iov::defaults only if the bool argument is true for your condition
struct ircd::json::iov::defaults_if
:protected ircd::json::iov::node
{
	defaults_if(iov &, const bool &, member);
	defaults_if() = default;
};

inline ircd::json::iov::push::operator
ircd::json::member &()
{
	auto &node(static_cast<iov::node &>(*this));
	return static_cast<member &>(node);
}

inline ircd::json::iov::push::operator
const ircd::json::member &()
const
{
	const auto &node(static_cast<const iov::node &>(*this));
	return static_cast<const member &>(node);
}

/// Conversion/Generator template. This reduces boilerplate when converting
/// some iterable collection of members to an iov. You have to pre-place the
/// nodes for the iov ahead of this function and they will be filled in.
template<class node,
         size_t size,
         class T>
ircd::json::iov &
ircd::json::make_iov(iov &ret,
                     node (&nodes)[size],
                     T&& members)
{
	return make_iov<node, T>(ret, nodes, size, std::forward<T>(members));
}

/// Conversion/Generator template. This reduces boilerplate when converting
/// some iterable collection of members to an iov. You have to pre-place the
/// nodes for the iov ahead of this function. This overload takes a dynamic
/// sized array, you have to pass the size.
template<class node,
         class T>
ircd::json::iov &
ircd::json::make_iov(iov &ret,
                     node *const nodes,
                     const size_t &size,
                     T&& members)
{
	size_t i{0};
	for(auto&& member : members)
		if(likely(i < size))
			new (nodes + i++) node(ret, json::member(member));

	return ret;
}
