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
#define HAVE_IRCD_DB_OBJECT_H

// handler register(username):
//
// (let database value registered = 0)
//
//  context A                   | context B
//                              |
//0 enter                       |
//1 if(registered)              | enter                         ; A yields on cache-miss/IO read
//2                             | if(registered)                ; B resumes hitting cached value A fetched
//3                             |     bnt return;               ; B continues without yield
//4                             | registered = time(nullptr);   ; B assigns B's value to database
//5    b?t return;              | leave                         ; A resumes [what does if() see?]
//6 registered = time(nullptr); |                               ; A overwrites B's value
//7 leave                       |                               ; ???

namespace ircd {
namespace db   {

template<class T,
         database *const &d>
struct value
{
};

template<database *const &d>
struct value<void, d>
:cell
{
	value(const string_view &name,
	      const string_view &index)
	:cell{*d, name, index}
	{}

	using cell::cell;
};

template<database *const &d>
struct object
:row
{
	struct const_iterator;

	string_view prefix;
	string_view index;

	object(const string_view &prefix, const string_view &index);
	object() = default;
};

template<database *const &d>
object<d>::object(const string_view &prefix,
	              const string_view &index)
:row{[&prefix, &index]() -> row
{
	// The prefix is the name of the object we want to find members in.
	// This function has to find columns starting with the prefix but not
	// containing any additional '.' except the one directly after the prefix,
	// as more '.' indicates sub-members which we don't fetch here.

	auto &columns(d->columns);
	auto low(columns.lower_bound(prefix)), hi(low);
	for(; hi != std::end(columns); ++hi)
	{
		const auto &name(hi->first);
		if(!startswith(name, prefix))
			break;
	}

	string_view names[std::distance(low, hi)];
	std::transform(low, hi, names, [&prefix](const auto &pair)
	{
		const auto &path(pair.first);

		// Find members of this object by removing the prefix and then removing
		// any members which have a '.' indicating they are not at this level.
		string_view name(path);
		name = lstrip(name, prefix);
		name = lstrip(name, '.');
		if(tokens_count(name, ".") != 1)
			return string_view{};

		return string_view{path};
	});

	// Clear empty names from the array before passing up to row{}
	const auto end(std::remove(names, names + std::distance(low, hi), string_view{}));
	const auto count(std::distance(names, end));
	return row
	{
		*d, index, vector_view<string_view>(names, count)
	};
}()}
,index{index}
{
}

template<database *const &d>
struct value<string_view ,d>
:value<void, d>
{
	operator string_view() const
	{
		return string_view{static_cast<const cell &>(*this)};
	}

	operator string_view()
	{
		return string_view{static_cast<cell &>(*this)};
	}

	value &operator=(const string_view &val)
	{
		static_cast<cell &>(*this) = val;
		return *this;
	}

	value(const string_view &col,
	      const string_view &row)
	:value<void, d>{col, row}
	{}

	friend std::ostream &operator<<(std::ostream &s, const value<string_view, d> &v)
	{
		s << string_view{v};
		return s;
	}
};

template<class T,
         database *const &d>
struct arithmetic_value
:value<void, d>
{
	bool compare_exchange(T &expected, const T &desired)
	{
		const auto ep(reinterpret_cast<const char *>(&expected));
		const auto dp(reinterpret_cast<const char *>(&desired));

		string_view s{ep, expected? sizeof(T) : 0};
		const auto ret(cell::compare_exchange(s, string_view{dp, sizeof(T)}));
		expected = !s.empty()? *reinterpret_cast<const T *>(s.data()) : 0;
		return ret;
	}

	T exchange(const T &desired)
	{
		const auto dp(reinterpret_cast<const char *>(&desired));
		const auto ret(cell::exchange(string_view{dp, desired? sizeof(T) : 0}));
		return !ret.empty()? *reinterpret_cast<const T *>(ret.data()) : 0;
	}

	operator T() const
	{
		const auto val(this->val());
		return !val.empty()? *reinterpret_cast<const T *>(val.data()) : 0;
	}

	operator T()
	{
		const auto val(this->val());
		return !val.empty()? *reinterpret_cast<const T *>(val.data()) : 0;
	}

	arithmetic_value &operator=(const T &val)
	{
		cell &cell(*this);
		const auto ptr(reinterpret_cast<const char *>(&val));
		cell = string_view{ptr, val? sizeof(T) : 0};
		return *this;
	}

	friend std::ostream &operator<<(std::ostream &s, const arithmetic_value &v)
	{
		s << T(v);
		return s;
	}

	arithmetic_value(const string_view &col,
	                 const string_view &row)
	:value<void, d>{col, row}
	{}
};

#define IRCD_ARITHMETIC_VALUE(_type_)                    \
template<database *const &d>                             \
struct value<_type_, d>                                  \
:arithmetic_value<_type_, d>                             \
{                                                        \
	using arithmetic_value<_type_, d>::arithmetic_value; \
}

IRCD_ARITHMETIC_VALUE(uint64_t);
IRCD_ARITHMETIC_VALUE(int64_t);
IRCD_ARITHMETIC_VALUE(uint32_t);
IRCD_ARITHMETIC_VALUE(int32_t);
IRCD_ARITHMETIC_VALUE(uint16_t);
IRCD_ARITHMETIC_VALUE(int16_t);

} // namespace db
} // namespace ircd
