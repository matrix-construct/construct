/*
 * charybdis: 21st Century IRC++d
 * util.h: Miscellaneous utilities
 *
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
#define HAVE_IRCD_STRING_VIEW_H

namespace ircd
{
	struct string_view;

	template<class T> struct vector_view;

	template<class T = string_view> struct byte_view;
	template<> struct byte_view<string_view>;

	template<int (&test)(int) = std::isprint> auto ctype(const string_view &s);

	size_t size(const string_view &);
	bool empty(const string_view &);
	bool operator!(const string_view &);
	bool defined(const string_view &);
	bool null(const string_view &);

	constexpr string_view operator ""_sv(const char *const literal, const size_t size);
}

/// Customized std::string_view (experimental TS / C++17)
///
/// This class adds iterator-based (char*, char*) construction to std::string_view which otherwise
/// takes traditional (char*, size_t) arguments. This allows boost::spirit grammars to create
/// string_view's using the raw[] directive achieving zero-copy/zero-allocation parsing.
///
struct ircd::string_view
:std::string_view
{
	// (non-standard)
	explicit operator bool() const
	{
		return !empty();
	}

	/// (non-standard) When data() != nullptr we consider the string defined
	/// downstream in this project wrt JS/JSON. This is the bit of information
	/// we're deciding on for defined|undefined. If this string_view is
	/// constructed from a literal "" we must assert that inputs a valid pointer
	/// in the std::string_view with length 0; stdlib can't optimize that with
	/// a nullptr replacement.
	bool undefined() const
	{
		return data() == nullptr;
	}

	bool defined() const
	{
		return !undefined();
	}

	/// (non-standard) After using data() == nullptr for undefined, we're fresh
	/// out of legitimate bits here to represent the null type string. In this
	/// case we expect a hack pointer of 0x1 which will mean JS null
	bool null() const
	{
		return data() == reinterpret_cast<const char *>(0x1);
	}

	// (non-standard) our faux insert stub
	// Tricks boost::spirit into thinking this is mutable string (hint: it's not).
	// Instead, the raw[] directive in Qi grammar will use the iterator constructor only.
	// __attribute__((error("string_view is not insertable (hint: use raw[] directive)")))
	void insert(const iterator &, const char &)
	{
		assert(0);
	}

	// (non-standard) our iterator-based assign
	string_view &assign(const char *const &begin, const char *const &end)
	{
		this->~string_view();
		new (this) string_view{begin, size_t(std::distance(begin, end))};
		return *this;
	}

	// (non-standard) intuitive wrapper for remove_suffix.
	// Unlike std::string, we can cheaply involve a reference to the removed character
	// which still exists.
	const char &pop_back()
	{
		const char &ret(back());
		remove_suffix(1);
		return ret;
	}

	// (non-standard) intuitive wrapper for remove_prefix.
	// Unlike std::string, we can cheaply involve a reference to the removed character
	// which still exists.
	const char &pop_front()
	{
		const char &ret(front());
		remove_prefix(1);
		return ret;
	}

	/// (non-standard) resize viewer
	void resize(const size_t &count)
	{
		*this = string_view{data(), data() + count};
	}

	// (non-standard) our iterator-based constructor
	string_view(const char *const &begin, const char *const &end)
	:std::string_view{begin, size_t(std::distance(begin, end))}
	{}

	// (non-standard) our iterator-based constructor
	string_view(const std::string::const_iterator &begin, const std::string::const_iterator &end)
	:string_view{&*begin, &*end}
	{}

	// (non-standard) our array based constructor
	template<size_t SIZE>
	constexpr string_view(const std::array<char, SIZE> &array)
	:string_view
	{
		array.data(), std::find(array.begin(), array.end(), '\0')
	}{}

	// (non-standard) our buffer based constructor
	template<size_t SIZE>
	constexpr string_view(const char (&buf)[SIZE])
	:string_view
	{
		buf, std::find(buf, buf + SIZE, '\0')
	}{}

	// Required due to current instability in stdlib
//	string_view(const std::experimental::string_view &esv)
//	:std::string_view{esv}
//	{}

	// Required due to instability in stdlib
//	constexpr string_view(const std::experimental::fundamentals_v1::basic_string_view<char> &bsv)
//	:std::string_view{bsv}
//	{}

//	constexpr string_view(const char *const &start, const size_t &size)
//	:std::string_view{start, size}
//	{}

	explicit string_view(const std::string &string)
	:std::string_view{string.data(), string.size()}
	{}

	constexpr string_view(const std::string_view &sv)
	:std::string_view{sv}
	{}

	/// Our default constructor sets the elements to 0 for best behavior by
	/// defined() and null() et al.
	constexpr string_view()
	:std::string_view{nullptr, 0}
	{}

	using std::string_view::string_view;
};

/// Compile-time conversion from a string literal into a string_view.
constexpr ircd::string_view
ircd::operator ""_sv(const char *const literal, const size_t size)
{
	return string_view{literal, size};
}

template<class T>
struct ircd::vector_view
{
	using value_type = T;
	using pointer = T *;
	using reference = T &;
	using difference_type = size_t;
	using iterator = T *;
	using const_iterator = const T *;

	T *_data                                     { nullptr                                         };
	T *_stop                                     { nullptr                                         };

  public:
	const T *data() const                        { return _data;                                   }
	T *data()                                    { return _data;                                   }

	size_t size() const                          { return std::distance(_data, _stop);             }
	bool empty() const                           { return !size();                                 }

	const_iterator begin() const                 { return data();                                  }
	const_iterator end() const                   { return _stop;                                   }
	const_iterator cbegin()                      { return data();                                  }
	const_iterator cend()                        { return _stop;                                   }
	iterator begin()                             { return data();                                  }
	iterator end()                               { return _stop;                                   }

	const T &operator[](const size_t &pos) const
	{
		return *(data() + pos);
	}

	T &operator[](const size_t &pos)
	{
		return *(data() + pos);
	}

	const T &at(const size_t &pos) const
	{
		if(unlikely(pos >= size()))
			throw std::out_of_range("vector_view::range_check");

		return operator[](pos);
	}

	T &at(const size_t &pos)
	{
		if(unlikely(pos >= size()))
			throw std::out_of_range("vector_view::range_check");

		return operator[](pos);
	}

	vector_view(T *const &start, T *const &stop)
	:_data{start}
	,_stop{stop}
	{}

	vector_view(T *const &start, const size_t &size)
	:vector_view(start, start + size)
	{}

	vector_view(const std::initializer_list<T> &list)
	:vector_view(std::begin(list), std::end(list))
	{}

	template<class U,
	         class A>
	vector_view(std::vector<U, A> &v)
	:vector_view(v.data(), v.size())
	{}

	template<size_t SIZE>
	vector_view(T (&buffer)[SIZE])
	:vector_view(buffer, SIZE)
	{}

	template<size_t SIZE>
	vector_view(std::array<T, SIZE> &array)
	:vector_view(array.data(), array.size())
	{}

	vector_view() = default;
};

/// string_view -> bytes
template<class T>
struct ircd::byte_view
{
	string_view s;

	operator const T &() const
	{
		if(unlikely(sizeof(T) > s.size()))
			throw std::bad_cast();

		return *reinterpret_cast<const T *>(s.data());
	}

	byte_view(const string_view &s = {})
	:s{s}
	{
		if(unlikely(sizeof(T) > s.size()))
			throw std::bad_cast();
	}

	// bytes -> bytes (completeness)
	byte_view(const T &t)
	:s{byte_view<string_view>{t}}
	{}
};

/// bytes -> string_view. A byte_view<string_view> is raw data of byte_view<T>.
///
/// This is an important specialization to take note of. When you see
/// byte_view<string_view> know that another type's bytes are being represented
/// by the string_view if that type is not string_view family itself.
template<>
struct ircd::byte_view<ircd::string_view>
:string_view
{
	template<class T,
	         typename std::enable_if<!std::is_base_of<std::string_view, T>::value, int *>::type = nullptr>
	byte_view(const T &t)
	:string_view{reinterpret_cast<const char *>(&t), sizeof(T)}
	{}

	/// string_view -> string_view (completeness)
	byte_view(const string_view &t)
	:string_view{t}
	{}
};

inline bool
ircd::operator!(const string_view &str)
{
	return empty(str);
}

inline bool
ircd::empty(const string_view &str)
{
	return str.empty();
}

inline bool
ircd::null(const string_view &str)
{
	return str.null();
}

inline bool
ircd::defined(const string_view &str)
{
	return str.defined();
}

inline size_t
ircd::size(const string_view &str)
{
	return str.size();
}

template<int (&test)(int)>
auto
ircd::ctype(const string_view &s)
{
    return ctype<test>(std::begin(s), std::end(s));
}
