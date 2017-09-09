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

	bool operator!(const string_view &);
}

//
// Customized std::string_view (experimental TS / C++17)
//
// This class adds iterator-based (char*, char*) construction to std::string_view which otherwise
// takes traditional (char*, size_t) arguments. This allows boost::spirit grammars to create
// string_view's using the raw[] directive achieving zero-copy/zero-allocation parsing.
//
struct ircd::string_view
:std::string_view
{
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
		*this = std::string_view{begin, size_t(std::distance(begin, end))};
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

	// (non-standard) our iterator-based constructor
	string_view(const char *const &begin, const char *const &end)
	:std::string_view{begin, size_t(std::distance(begin, end))}
	{}

	// (non-standard) our array based constructor
	template<size_t SIZE>
	string_view(const std::array<char, SIZE> &array)
	:string_view
	{
		array.data(), std::find(array.begin(), array.end(), '\0')
	}{}

	// (non-standard) our buffer based constructor
	template<size_t SIZE>
	string_view(const char (&buf)[SIZE])
	:string_view
	{
		buf, std::find(buf, buf + SIZE, '\0')
	}{}

	// Required due to current instability in stdlib
	//string_view(const std::experimental::string_view &esv)
	//:std::string_view{esv}
	//{}

	// Required due to current instability in stdlib
	string_view(const std::experimental::fundamentals_v1::basic_string_view<char> &bsv)
	:std::string_view{bsv}
	{}

	string_view()
	:std::string_view{}
	{}

	using std::string_view::string_view;
};

template<class T>
struct ircd::vector_view
{
	T *_data                                     { nullptr                                         };
	T *_stop                                     { nullptr                                         };

  public:
	T *data() const                              { return _data;                                   }
	size_t size() const                          { return std::distance(_data, _stop);             }
	bool empty() const                           { return !size();                                 }

	const T *begin() const                       { return data();                                  }
	const T *end() const                         { return _stop;                                   }
	const T *cbegin()                            { return data();                                  }
	const T *cend()                              { return _stop;                                   }
	T *begin()                                   { return data();                                  }
	T *end()                                     { return _stop;                                   }

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
			throw std::out_of_range();

		return operator[](pos);
	}

	T &at(const size_t &pos)
	{
		if(unlikely(pos >= size()))
			throw std::out_of_range();

		return operator[](pos);
	}

	vector_view(T *const &start, T *const &stop)
	:_data{start}
	,_stop{stop}
	{}

	vector_view(T *const &start, const size_t &size)
	:_data{start}
	,_stop{start + size}
	{}

	template<class U,
	         class A>
	vector_view(std::vector<U, A> &v)
	:vector_view{v.data(), v.size()}
	{}

	template<size_t SIZE>
	vector_view(T (&buffer)[SIZE])
	:vector_view{std::addressof(buffer), SIZE}
	{}

	template<size_t SIZE>
	vector_view(std::array<T, SIZE> &array)
	:vector_view{array.data(), array.size()}
	{}

	vector_view() = default;
};

// string_view -> bytes
template<class T>
struct ircd::byte_view
{
	string_view s;

	operator const T &() const
	{
		return *reinterpret_cast<const T *>(s.data());
	}

	byte_view(const string_view &s)
	:s{s}
	{
		if(unlikely(sizeof(T) != s.size()))
			throw std::bad_cast();
	}

	// bytes -> bytes (completeness)
	byte_view(const T &t)
	:s{byte_view<string_view>{t}}
	{}
};

// bytes -> string_view
template<>
struct ircd::byte_view<ircd::string_view>
:string_view
{
	template<class T>
	byte_view(const T &t)
	:string_view{reinterpret_cast<const char *>(&t), sizeof(T)}
	{}
};

// string_view -> string_view (completeness)
namespace ircd
{
	template<> inline
	byte_view<string_view>::byte_view(const string_view &t)
	:string_view{t}
	{}
}

inline bool
ircd::operator!(const string_view &str)
{
	return str.empty();
}
