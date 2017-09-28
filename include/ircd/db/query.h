/*
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
#define HAVE_IRCD_DB_QUERY_H

namespace ircd::db
{
	/// Types of query clauses.
	enum class where
	{
		noop,
		test,
		equal,
		not_equal,
		logical_or,
		logical_and,
		logical_not,
	};

	string_view reflect(const where &);

	/// The query provides a decision tree oriented around the structure of
	/// a tuple. All queries inherit from query<> which can execute the
	/// derived's test via the virtual operator(). The abstract query instance
	/// stores the type of its derived portion for downcasting. Downcasting is
	/// used to get more information from the query to get a result faster, ex.
	/// where::equal, the keys being tested might impact the db fetch pattern.
	/// or searching a logic tree of where::logical_* for the most efficient
	/// db fetches to make next.
	template<class tuple, enum where = where::noop> struct query;

	template<class tuple> struct query<tuple, where::noop>;
	template<class tuple> struct query<tuple, where::test>;
	template<class tuple> struct query<tuple, where::equal>;
	template<class tuple> struct query<tuple, where::not_equal>;
	template<class tuple> struct query<tuple, where::logical_or>;
	template<class tuple> struct query<tuple, where::logical_and>;
	template<class tuple> struct query<tuple, where::logical_not>;
}

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::noop>
{
	virtual bool operator()(const tuple &) const
	{
		return true;
	}

	// Stores the type of the derived class for downcasting. This is important
	// in order to evaluate details of the query.
	where type;

	query(const enum where &type = where::noop)
	:type{type}
	{}

	virtual ~query() noexcept {}
};

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::test>
:query<tuple>
{
	using function = std::function<bool (const tuple &)>;

	function closure;

	bool operator()(const tuple &t) const override
	{
		return closure(t);
	}

	query(function closure)
	:query<tuple>{where::test}
	,closure{std::move(closure)}
	{}
};

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::equal>
:query<tuple>
{
	tuple value;

	bool operator()(const tuple &) const override;

	query(const tuple &value)
	:query<tuple>{where::equal}
	,value{value}
	{}

	query(const json::members &members)
	:query<tuple>{where::equal}
	,value{members}
	{}
};

template<class tuple>
bool
ircd::db::query<tuple, ircd::db::where::equal>::operator()(const tuple &value)
const
{
	return json::until(this->value, value, []
	(const auto &key, const auto &a, const auto &b)
	{
		if(!a)
			return true;

		return a == b;
	});
}

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::not_equal>
:query<tuple>
{
	tuple value;

	bool operator()(const tuple &) const override;

	query(const tuple &value)
	:query<tuple>{where::not_equal}
	,value{value}
	{}

	query(const json::members &members)
	:query<tuple>{where::not_equal}
	,value{members}
	{}
};

template<class tuple>
bool
ircd::db::query<tuple, ircd::db::where::not_equal>::operator()(const tuple &value)
const
{
	return !json::until(this->value, value, []
	(const auto &key, const auto &a, const auto &b)
	{
		if(!a)
			return true;

		return a == b;
	});
}

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::logical_or>
:query<tuple>
{
	const query<tuple> *a, *b;

	bool operator()(const tuple &t) const override
	{
		return (*a)(t) || (*b)(t);
	}

	query(const query<tuple> &a, const query<tuple> &b)
	:query<tuple>{where::logical_or}
	,a{&a}
	,b{&b}
	{}
};

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::logical_and>
:query<tuple>
{
	const query<tuple> *a, *b;

	bool operator()(const tuple &t) const override
	{
		return (*a)(t) && (*b)(t);
	}

	query(const query<tuple> &a, const query<tuple> &b)
	:query<tuple>{where::logical_and}
	,a{&a}
	,b{&b}
	{}
};

template<class tuple>
struct ircd::db::query<tuple, ircd::db::where::logical_not>
:query<tuple>
{
	const where *a;

	bool operator()(const tuple &t) const override
	{
		return !(*a)(t);
	}

	query(const where &a)
	:query<tuple>{where::logical_not}
	,a{&a}
	{}
};

namespace ircd::db
{
	template<class T> typename db::query<T, where::logical_or> operator||(const query<T> &a, const query<T> &b)
	{
		return { a, b };
	}

	template<class T> typename db::query<T, where::logical_and> operator&&(const query<T> &a, const query<T> &b)
	{
		return { a, b };
	}

	template<class T> typename db::query<T, where::logical_not> operator!(const query<T> &a)
	{
		return { a };
	}
}
