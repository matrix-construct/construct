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
#define HAVE_IRCD_JSON_VALUE_H

/// A primitive of the ircd::json system representing a value at runtime.
///
/// This holds state for values apropos a JSON object.
///
/// Value's data can either be in the form of a JSON string or it can be
/// native machine state. The serial flag indicates the former.
///
/// Value is capable of allocation and ownership of its internal data if
/// necessary with move semantics, but copying may be not implemented in
/// exceptional cases.
///
/// Value can can hold any of the JSON types in either of these states.
/// This is accomplished with runtime switching and branching but this is
/// still lightweight. The structure is just the size of two pointers, the
/// same as a string_view.
///
struct ircd::json::value
{
	union // xxx std::variant
	{
		int64_t integer;
		double floating;
		const char *string;
		const struct index *object;
		const struct array *array;
	};

	uint64_t len     : 57;      ///< length indicator
	enum type type   : 3;       ///< json::type indicator
	uint64_t serial  : 1;       ///< only string* is used. type indicates JSON
	uint64_t alloc   : 1;       ///< indicates the pointer for type is owned
	uint64_t floats  : 1;       ///< for NUMBER type, integer or floating

  public:
	bool null() const;
	bool empty() const;
	bool undefined() const;

	operator string_view() const;
	explicit operator double() const;
	explicit operator int64_t() const;
	explicit operator std::string() const;

	template<class T> explicit value(const T &specialized);
	template<size_t N> value(const char (&)[N]);
	value(const char *const &s);
	value(const string_view &sv, const enum type &);
	value(const string_view &sv);
	value(const index *const &);    // alloc = false
	value(std::unique_ptr<index>);  // alloc = true
	value();
	value(value &&) noexcept;
	value(const value &);
	value &operator=(value &&) noexcept;
	value &operator=(const value &) = delete;
	~value() noexcept;

	friend enum type type(const value &a);
	friend bool operator==(const value &a, const value &b);
	friend bool operator!=(const value &a, const value &b);
	friend bool operator<=(const value &a, const value &b);
	friend bool operator>=(const value &a, const value &b);
	friend bool operator<(const value &a, const value &b);
	friend bool operator>(const value &a, const value &b);

	friend size_t serialized(const value &);
	friend string_view stringify(mutable_buffer &, const value &);
	friend std::ostream &operator<<(std::ostream &, const value &);
};

namespace ircd::json
{
	template<> value::value(const double &floating);
	template<> value::value(const int64_t &integer);
	template<> value::value(const float &floating);
	template<> value::value(const int32_t &integer);
	template<> value::value(const int16_t &integer);
	template<> value::value(const bool &boolean);
	template<> value::value(const std::string &str);
}

static_assert(sizeof(ircd::json::value) == 16, "");

inline
ircd::json::value::value()
:string{""}
,len{0}
,type{STRING}
,serial{true}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const string_view &sv,
                         const enum type &type)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{true}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const index *const &object)
:object{object}
,len{0}
,type{OBJECT}
,serial{false}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(std::unique_ptr<index> object)
:object{object.get()}
,len{0}
,type{OBJECT}
,serial{false}
,alloc{true}
,floats{false}
{
	object.release();
}

template<> inline
ircd::json::value::value(const int64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{false}
{}

template<> inline
ircd::json::value::value(const double &floating)
:floating{floating}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{true}
{}

template<> inline
ircd::json::value::value(const float &floating)
:value{double(floating)}
{}

template<> inline
ircd::json::value::value(const int32_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const int16_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const bool &boolean)
:value
{
	boolean? "true" : "false",
	type::LITERAL
}{}

template<> inline
ircd::json::value::value(const std::string &str)
:value{string_view{str}}
{}

template<class T>
ircd::json::value::value(const T &t)
:value{static_cast<string_view>(t)}
{
}

template<size_t N>
ircd::json::value::value(const char (&str)[N])
:string{str}
,len{strnlen(str, N)}
,type{json::type(str, std::nothrow)}
,serial{true}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const char *const &s)
:string{s}
,len{strlen(s)}
,type{json::type(s, std::nothrow)}
,serial{true}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const string_view &sv)
:value{sv, json::type(sv, std::nothrow)}
{}

inline
ircd::json::value::value(value &&other)
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

inline
ircd::json::value::value(const value &other)
:integer{other.integer}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
,floats{other.floats}
{
	switch(type)
	{
		case NUMBER:
			break;

		case LITERAL:
		case STRING:
			assert(serial);
			assert(!alloc);
			break;

		case OBJECT:
		case ARRAY:
			assert(serial);
			break;
	}
}

inline ircd::json::value &
ircd::json::value::operator=(value &&other)
noexcept
{
	this->~value();
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
ircd::json::type(const value &a)
{
	return a.type;
}
