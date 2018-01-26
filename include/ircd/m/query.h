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
#define HAVE_IRCD_M_VM_QUERY_H

namespace ircd::m::dbs
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
	/// an event. All queries inherit from query<> which can execute the
	/// derived's test via the virtual operator(). The abstract query instance
	/// stores the type of its derived portion for downcasting. Downcasting is
	/// used to get more information from the query to get a result faster, ex.
	/// where::equal, the keys being tested might impact the db fetch pattern.
	/// or searching a logic tree of where::logical_* for the most efficient
	/// db fetches to make next.
	template<enum where = where::noop> struct query;

	struct query<where::noop>;
	struct query<where::test>;
	struct query<where::equal>;
	struct query<where::not_equal>;
	struct query<where::logical_or>;
	struct query<where::logical_and>;
	struct query<where::logical_not>;

	query<where::logical_or> operator||(const query<> &a, const query<> &b);
	query<where::logical_and> operator&&(const query<> &a, const query<> &b);
	query<where::logical_not> operator!(const query<> &a);

	extern const query<> noop;
}

struct ircd::m::dbs::query<ircd::m::dbs::where::noop>
{
	virtual bool operator()(const event &) const
	{
		return true;
	}

	// Stores the type of the derived class for downcasting. This is important
	// in order to evaluate details of the query.
	where type;

	query(const enum where &type = where::noop)
	:type{type}
	{}

	virtual ~query() noexcept;
};

struct ircd::m::dbs::query<ircd::m::dbs::where::test>
:query<>
{
	using function = std::function<bool (const event &)>;

	function closure;

	bool operator()(const event &t) const override
	{
		return closure(t);
	}

	query(function closure)
	:query<>{where::test}
	,closure{std::move(closure)}
	{}
};

struct ircd::m::dbs::query<ircd::m::dbs::where::equal>
:query<>
{
	event value;

	bool operator()(const event &) const override;

	query(const event &value)
	:query<>{where::equal}
	,value{value}
	{}

	query(const json::members &members)
	:query<>{where::equal}
	,value{members}
	{}
};

inline bool
ircd::m::dbs::query<ircd::m::dbs::where::equal>::operator()(const event &value)
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

struct ircd::m::dbs::query<ircd::m::dbs::where::not_equal>
:query<>
{
	event value;

	bool operator()(const event &) const override;

	query(const event &value)
	:query<>{where::not_equal}
	,value{value}
	{}

	query(const json::members &members)
	:query<>{where::not_equal}
	,value{members}
	{}
};

inline bool
ircd::m::dbs::query<ircd::m::dbs::where::not_equal>::operator()(const event &value)
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

struct ircd::m::dbs::query<ircd::m::dbs::where::logical_or>
:query<>
{
	const query<> *a, *b;

	bool operator()(const event &t) const override
	{
		return (*a)(t) || (*b)(t);
	}

	query(const query<> &a, const query<> &b)
	:query<>{where::logical_or}
	,a{&a}
	,b{&b}
	{}
};

struct ircd::m::dbs::query<ircd::m::dbs::where::logical_and>
:query<>
{
	const query<> *a, *b;

	bool operator()(const event &t) const override
	{
		return (*a)(t) && (*b)(t);
	}

	query(const query<> &a, const query<> &b)
	:query<>{where::logical_and}
	,a{&a}
	,b{&b}
	{}
};

struct ircd::m::dbs::query<ircd::m::dbs::where::logical_not>
:query<>
{
	const query<> *a;

	bool operator()(const event &t) const override
	{
		return !(*a)(t);
	}

	query(const query<> &a)
	:query<>{where::logical_not}
	,a{&a}
	{}
};

inline ircd::m::dbs::query<ircd::m::dbs::where::logical_or>
ircd::m::dbs::operator||(const query<> &a, const query<> &b)
{
	return { a, b };
}

inline ircd::m::dbs::query<ircd::m::dbs::where::logical_and>
ircd::m::dbs::operator&&(const query<> &a, const query<> &b)
{
	return { a, b };
}

inline ircd::m::dbs::query<ircd::m::dbs::where::logical_not>
ircd::m::dbs::operator!(const query<> &a)
{
	return { a };
}
