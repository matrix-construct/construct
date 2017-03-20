/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
 */

#pragma once
#define HAVE_IRCD_JSON_VAL_H

namespace ircd {
namespace json {

struct val
{
	union // xxx std::variant
	{
		int64_t integer;
		double floating;
		const char *string;
		const struct obj *object;
		const struct array *array;
	};

	uint64_t len     : 58;
	enum type type   : 3;
	uint64_t serial  : 1;
	uint64_t alloc   : 1;
	uint64_t floats  : 1;

  public:
	size_t size() const;
	operator string_view() const;
	explicit operator std::string() const;

	template<class T> explicit val(const T &specialized);
	template<size_t N> val(const char (&)[N]);
	val(const string_view &sv, const enum type &);
	val(const string_view &sv);
	val(const char *const &s);
	val(const struct obj *const &);  // alloc = false
	val(std::unique_ptr<obj>);       // alloc = true
	val() = default;
	val(val &&) noexcept;
	val(const val &) = delete;
	val &operator=(val &&) noexcept;
	val &operator=(const val &) = delete;
	~val() noexcept;

	friend enum type type(const val &a);
	friend bool operator==(const val &a, const val &b);
	friend bool operator!=(const val &a, const val &b);
	friend bool operator<=(const val &a, const val &b);
	friend bool operator>=(const val &a, const val &b);
	friend bool operator<(const val &a, const val &b);
	friend bool operator>(const val &a, const val &b);
	friend std::ostream &operator<<(std::ostream &, const val &);
};
template<> val::val(const double &floating);
template<> val::val(const int64_t &integer);
template<> inline val::val(const float &floating): val { double(floating) } {}
template<> inline val::val(const int32_t &integer): val { int64_t(integer) } {}
template<> inline val::val(const int16_t &integer): val { int64_t(integer) } {}
template<> inline val::val(const std::string &str): val { string_view{str} } {}

static_assert(sizeof(val) == 16, "");

} // namespace json
} // namespace ircd

template<size_t N>
ircd::json::val::val(const char (&str)[N])
:val{string_view{str}}
{}

inline
ircd::json::val::val(const char *const &s)
:val{string_view{s}}
{}

inline
ircd::json::val::val(const string_view &sv)
:val{sv, json::type(sv, std::nothrow)}
{}

inline
ircd::json::val::val(const string_view &sv,
                     const enum type &type)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{true}
,alloc{false}
,floats{false}
{}

template<> inline
ircd::json::val::val(const int64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{false}
{}

template<> inline
ircd::json::val::val(const double &floating)
:floating{floating}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{true}
{}

inline
ircd::json::val::val(const struct obj *const &object)
:object{object}
,len{0}
,type{OBJECT}
,serial{false}
,alloc{false}
,floats{false}
{}

inline
ircd::json::val::val(std::unique_ptr<obj> object)
:object{object.get()}
,len{0}
,type{OBJECT}
,serial{false}
,alloc{true}
,floats{false}
{
	object.release();
}

inline
ircd::json::val::val(val &&other)
noexcept
:integer{other.integer}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
,floats{other.floats}
{
	other.alloc = false;
}

inline ircd::json::val &
ircd::json::val::operator=(val &&other)
noexcept
{
	this->~val();
	integer = other.integer;
	len = other.len;
	type = other.type;
	serial = other.serial;
	alloc = other.alloc;
	other.alloc = false;
	floats = other.floats;
	return *this;
}

inline enum ircd::json::type
ircd::json::type(const val &a)
{
	return a.type;
}
