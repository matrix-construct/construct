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
#define HAVE_IRCD_DB_WHERE_H

namespace ircd::db
{
	template<class tuple> struct where;
}

template<class tuple>
struct ircd::db::where
{
	struct equal;
	struct not_equal;
	struct logical_not;
	struct logical_and;
	struct logical_or;
	struct test;
	struct noop;

	virtual bool operator()(const tuple &) const = 0;
	virtual ~where() noexcept = default;
};

namespace ircd::db
{
	template<class T> typename where<T>::logical_or operator||(const where<T> &, const where<T> &);
	template<class T> typename where<T>::logical_and operator&&(const where<T> &, const where<T> &);
	template<class T> typename where<T>::logical_not operator!(const where<T> &);
}

template<class tuple>
struct ircd::db::where<tuple>::equal
:ircd::db::where<tuple>
{
	tuple value;

	bool operator()(const tuple &) const override;

	equal(const tuple &value)
	:value{value}
	{}

	equal(json::members members)
	:value{std::move(members)}
	{}
};

template<class tuple>
bool
ircd::db::where<tuple>::equal::operator()(const tuple &t)
const
{
	return json::until(this->value, [&t]
	(const auto &key, auto&& where_value)
	{
		if(!where_value)
			return true;

		bool equal(true);
		at(t, key, [&where_value, &equal]
		(auto&& value)
		{
			//equal = value == where_value;
			equal = (byte_view<>(value) == byte_view<>(where_value));
		});

		return equal;
	});
}

template<class tuple>
struct ircd::db::where<tuple>::not_equal
:ircd::db::where<tuple>
{
	tuple value;

	bool operator()(const tuple &) const override;

	not_equal(const tuple &value)
	:value{value}
	{}

	not_equal(json::members members)
	:value{std::move(members)}
	{}
};

template<class tuple>
bool
ircd::db::where<tuple>::not_equal::operator()(const tuple &t)
const
{
	return json::until(this->value, [&t]
	(const auto &key, auto&& where_value)
	{
		if(!where_value)
			return true;

		bool not_equal(true);
		at(t, key, [&where_value, &not_equal]
		(auto&& value)
		{
			//equal = value == where_value;
			not_equal = (byte_view<>(value) != byte_view<>(where_value));
		});

		return not_equal;
	});
}

template<class tuple>
struct ircd::db::where<tuple>::logical_and
:ircd::db::where<tuple>
{
	const where *a, *b;

	bool operator()(const tuple &t) const override
	{
		return (*a)(t) && (*b)(t);
	}

	logical_and(const where &a, const where &b)
	:a{&a}
	,b{&b}
	{}
};

template<class tuple>
typename ircd::db::where<tuple>::logical_and
ircd::db::operator&&(const where<tuple> &a, const where<tuple> &b)
{
	return { a, b };
}

template<class tuple>
struct ircd::db::where<tuple>::logical_or
:ircd::db::where<tuple>
{
	const where *a, *b;

	bool operator()(const tuple &t) const override
	{
		return (*a)(t) || (*b)(t);
	}

	logical_or(const where &a, const where &b)
	:a{&a}
	,b{&b}
	{}
};

template<class tuple>
typename ircd::db::where<tuple>::logical_or
ircd::db::operator||(const where<tuple> &a, const where<tuple> &b)
{
	return { a, b };
}

template<class tuple>
struct ircd::db::where<tuple>::logical_not
:ircd::db::where<tuple>
{
	const where *a;

	bool operator()(const tuple &t) const override
	{
		return !(*a)(t);
	}

	logical_not(const where &a)
	:a{&a}
	{}
};

template<class tuple>
typename ircd::db::where<tuple>::logical_not
ircd::db::operator!(const where<tuple> &a)
{
	return { a };
}

template<class tuple>
struct ircd::db::where<tuple>::test
:ircd::db::where<tuple>
{
	using function = std::function<bool (const tuple &)>;

	function closure;

	bool operator()(const tuple &t) const override
	{
		return closure(t);
	}

	test(function closure)
	:closure{std::move(closure)}
	{}
};

template<class tuple>
struct ircd::db::where<tuple>::noop
:ircd::db::where<tuple>
{
	bool operator()(const tuple &) const override
	{
		return true;
	}
};
