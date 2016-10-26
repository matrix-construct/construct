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
 */

#pragma once
#define HAVE_IRCD_JS_STRING_H

namespace ircd {
namespace js   {

bool external(const JSString *const &);
size_t size(const JSString *const &);
char16_t at(const JSString *const &, const size_t &);

struct string
:JS::Rooted<JSString *>
{
	using handle = JS::HandleString;
	using handle_mutable = JS::MutableHandleString;

	static constexpr const size_t CBUFS = 8;
	static const size_t CBUFSZ;
	const char *c_str() const;                   // Copy into rotating buf
	size_t native_size() const;
	size_t size() const;
	bool empty() const;

	explicit operator std::string() const;
	operator JS::Value() const;

	char16_t operator[](const size_t &at) const;

	string(const char16_t *const &, const size_t &len);
	string(const char16_t *const &);
	string(const std::u16string &);
	string(const char *const &, const size_t &len);
	string(const std::string &);
	string(const char *const &);
	string(const value &);
	string(JSString *const &);
	string(JSString &);
	string();
	string(string &&) noexcept;
	string(const string &) = delete;

	friend int cmp(const string &, const string &);
	friend int cmp(const string &, const char *const &);
	friend int cmp(const char *const &, const string &);
	friend int cmp(const string &, const std::string &);
	friend int cmp(const std::string &, const string &);

	struct less
	{
		using is_transparent = std::true_type;
		template<class A, class B> bool operator()(const A &, const B &) const;
	};

	friend std::ostream &operator<<(std::ostream &, const string &);
};

string operator+(const string::handle &left, const string::handle &right);
string substr(const string::handle &, const size_t &pos, const size_t &len = -1);
std::pair<string, string> split(const string::handle &, const char16_t &);
std::pair<string, string> split(const string::handle &, const char &);

inline
string::string()
:JS::Rooted<JSString *>
{
	*cx,
	JS_GetEmptyString(*rt)
}
{
}

inline
string::string(string &&other)
noexcept
:JS::Rooted<JSString *>{*cx, other}
{
}

inline
string::string(JSString &val)
:JS::Rooted<JSString *>
{
	*cx, &val
}
{
}

inline
string::string(JSString *const &val)
:JS::Rooted<JSString *>
{
	*cx,
	likely(val)? val : throw internal_error("NULL string")
}
{
}

string::string(const value &val)
:JS::Rooted<JSString *>
{
	*cx,
	JS::ToString(*cx, val)?: throw type_error("Failed to convert value to string")
}
{
}

inline
string::string(const std::string &s)
:string(s.data(), s.size())
{
}

inline
string::string(const char *const &s)
:string(s, strlen(s))
{
}

inline
string::string(const char *const &s,
               const size_t &len)
:JS::Rooted<JSString *>
{
	*cx, [&s, &len]
	{
		auto buf(native_external_copy(s, len));
		return JS_NewExternalString(*cx, buf.release(), len, &native_external_deleter);
	}()
}
{
	if(unlikely(!get()))
		throw type_error("Failed to construct string from character array");
}

inline
string::string(const std::u16string &s)
:string(s.data(), s.size())
{
}

inline
string::string(const char16_t *const &s)
:string(s, std::char_traits<char16_t>::length(s))
{
}

inline
string::string(const char16_t *const &s,
               const size_t &len)
:JS::Rooted<JSString *>
{
	*cx, [&s, &len]
	{
		// JS_NewExternalString does not require a null terminated buffer, but we are going
		// to terminate anyway in case the deleter ever wants to iterate a canonical vector.
		auto buf(std::make_unique<char16_t[]>(len+1));
		memcpy(buf.get(), s, len * 2);
		buf.get()[len] = char16_t(0);
		return JS_NewExternalString(*cx, buf.release(), len, &native_external_deleter);
	}()
}
{
	if(unlikely(!get()))
		throw type_error("Failed to construct string from character array");
}

inline
char16_t
string::operator[](const size_t &pos)
const
{
	return at(get(), pos);
}

inline
string::operator JS::Value()
const
{
	return JS::StringValue(get());
}

inline
string::operator std::string()
const
{
	return native(get());
}

inline bool
string::empty()
const
{
	return size() == 0;
}

inline size_t
string::size()
const
{
	return js::size(get());
}

inline size_t
string::native_size()
const
{
	return js::native_size(get());
}

inline
std::ostream &operator<<(std::ostream &os, const string &s)
{
	os << std::string(s);
	return os;
}

template<class A,
         class B>
constexpr bool
string_comparison()
{
	return std::is_base_of<string, A>() || std::is_base_of<string, B>();
}

template<class A,
         class B>
bool
string::less::operator()(const A &a, const B &b)
const
{
	return cmp(a, b) < 0;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator>(const A &a, const B &b)
{
	return cmp(a, b) > 0;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator<(const A &a, const B &b)
{
	return cmp(a, b) < 0;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator>=(const A &a, const B &b)
{
	return cmp(a, b) >= 0;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator<=(const A &a, const B &b)
{
	return cmp(a, b) <= 0;
}

inline bool
operator==(const string &a, const char *const &b)
{
	bool ret;
	if(unlikely(!JS_StringEqualsAscii(*cx, a, b, &ret)))
		throw internal_error("Failed to compare string to native");

	return ret;
}

inline bool
operator==(const char *const &a, const string &b)
{
	bool ret;
	if(unlikely(!JS_StringEqualsAscii(*cx, b, a, &ret)))
		throw internal_error("Failed to compare string to native");

	return ret;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator==(const A &a, const B &b)
{
	return cmp(a, b) == 0;
}

template<class A,
         class B>
typename std::enable_if<string_comparison<A, B>(), bool>::type
operator!=(const A &a, const B &b)
{
	return !(operator==(a, b));
}

inline int
cmp(const string &a, const std::string &b)
{
	return cmp(a, b.c_str());
}

inline int
cmp(const std::string &a, const string &b)
{
	return cmp(a.c_str(), b);
}

inline int
cmp(const string &a, const char *const &b)
{
	return cmp(a, string(b));
}

inline int
cmp(const char *const &a, const string &b)
{
	return cmp(string(a), b);
}

inline int
cmp(const string &a, const string &b)
{
	int32_t ret;
	if(unlikely(!JS_CompareStrings(*cx, a, b, &ret)))
		throw internal_error("Failed to compare strings");

	return ret;
}

inline std::pair<string, string>
split(const string::handle &s,
      const char &c)
{
	return {};
}

inline std::pair<string, string>
split(const string::handle &s,
      const char16_t &c)
{
	size_t i(0);
	for(; i < size(s) && at(s, i) != c; ++i);
	return
	{
		substr(s, 0, i),
		i < size(s)? substr(s, i + 1, size(s) - i) : string()
	};
}

inline string
substr(const string::handle &s,
       const size_t &pos,
       const size_t &len)
{
	const auto _len(len == size_t(-1)? size(s) - pos : len);
	const auto ret(JS_NewDependentString(*cx, s, pos, _len));
	if(!ret)
		throw std::out_of_range("substr(): invalid arguments");

	return ret;
}

inline string
operator+(const string::handle &left,
          const string::handle &right)
{
	return JS_ConcatStrings(*cx, left, right);
}

inline char16_t
at(const JSString *const &s,
   const size_t &pos)
{
	char16_t ret;
	if(unlikely(!JS_GetStringCharAt(*cx, const_cast<JSString *>(s), pos, &ret)))
		throw range_error("index %zu is out of range", pos);

	return ret;
}

inline size_t
size(const JSString *const &s)
{
	return JS_GetStringLength(const_cast<JSString *>(s));
}

inline bool
external(const JSString *const &s)
{
	return JS_IsExternalString(const_cast<JSString *>(s));
}

} // namespace js
} // namespace ircd
