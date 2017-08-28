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
#define HAVE_IRCD_DB_VALUE_H

namespace ircd::db
{
	template<database *const &d, class T = string_view> struct value;
	template<database *const &d> struct value<d, void>;
	template<database *const &d> struct value<d, string_view>;
	template<database *const &d, class T> struct arithmetic_value;
}

template<ircd::db::database *const &d,
         class T>
struct ircd::db::value
{
};

template<ircd::db::database *const &d>
struct ircd::db::value<d, void>
:cell
{
	using cell::cell;

	value(const string_view &name,
	      const string_view &index)
	:cell{*d, name, index}
	{}
};

template<ircd::db::database *const &d>
struct ircd::db::value<d, ircd::string_view>
:value<d, void>
{
	operator string_view() const
	{
		return string_view{static_cast<const cell &>(*this)};
	}

	operator string_view()
	{
		return string_view{static_cast<cell &>(*this)};
	}

	using cell::cell;

	value(const string_view &col,
	      const string_view &row)
	:value<d, void>{col, row}
	{}

	value &operator=(const string_view &val)
	{
		static_cast<cell &>(*this) = val;
		return *this;
	}

	friend std::ostream &operator<<(std::ostream &s, const value<d, string_view> &v)
	{
		s << string_view{v};
		return s;
	}
};

template<ircd::db::database *const &d,
         class T>
struct ircd::db::arithmetic_value
:value<d, void>
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

	using cell::cell;

	arithmetic_value(const string_view &col,
	                 const string_view &row)
	:value<d, void>{col, row}
	{}

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
};

#define IRCD_ARITHMETIC_VALUE(_type_)                    \
template<database *const &d>                             \
struct value<d, _type_>                                  \
:arithmetic_value<d, _type_>                             \
{                                                        \
	using arithmetic_value<d, _type_>::arithmetic_value; \
}

namespace ircd::db
{
	IRCD_ARITHMETIC_VALUE(uint64_t);
	IRCD_ARITHMETIC_VALUE(int64_t);
	IRCD_ARITHMETIC_VALUE(uint32_t);
	IRCD_ARITHMETIC_VALUE(int32_t);
	IRCD_ARITHMETIC_VALUE(uint16_t);
	IRCD_ARITHMETIC_VALUE(int16_t);
}
