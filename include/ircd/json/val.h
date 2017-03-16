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
		const char *string;
		const struct obj *object;
		uint64_t integer;
	};

	uint64_t len     : 59;
	enum type type   : 3;
	uint64_t serial  : 1;
	uint64_t alloc   : 1;

	size_t size() const;

	operator string_view() const;
	explicit operator std::string() const;

	val(const uint64_t &integer);

	val(const struct obj &object,
	    const bool &alloc         = false);

	val(const string_view &sv,
	    const enum type &type     = STRING,
	    const bool &serial        = true);

	val() = default;
	val(val &&) noexcept;
	val(const val &) = delete;
	val &operator=(val &&) = default;
	val &operator=(const val &) = delete;
	~val() noexcept;

	friend bool operator==(const val &a, const val &b);
	friend bool operator<(const val &a, const val &b);

	friend std::ostream &operator<<(std::ostream &, const val &);
};

} // namespace json
} // namespace ircd

inline
ircd::json::val::val(const string_view &sv,
                     const enum type &type,
                     const bool &serial)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{serial}
,alloc{false}
{
}

inline
ircd::json::val::val(const uint64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
{
}

inline
ircd::json::val::val(const struct obj &object,
                     const bool &alloc)
:object{&object}
,len{0}
,type{OBJECT}
,serial{false}
,alloc{alloc}
{
}

inline
ircd::json::val::val(val &&other)
noexcept
:string{other.string}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
{
	other.alloc = false;
}

inline bool
ircd::json::operator<(const val &a, const val &b)
{
	if(unlikely(a.type != STRING || b.type != STRING))
		throw type_error("cannot compare values");

	return static_cast<string_view>(a) < static_cast<string_view>(b);
}

inline bool
ircd::json::operator==(const val &a, const val &b)
{
	if(unlikely(a.type != STRING || b.type != STRING))
		throw type_error("cannot compare values");

	return static_cast<string_view>(a) == static_cast<string_view>(b);
}
