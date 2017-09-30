/*
 * charybdis5
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
#define HAVE_IRCD_BUFFER_H

// Forward declarations from boost::asio because it is not included here. IRCd
// buffers are not based directly on the boost ones but are easily converted
// when passing our buffer to an asio function.
namespace boost::asio
{
	struct const_buffer;
	struct mutable_buffer;
}

/// Lightweight buffer interface compatible with boost::asio IO buffers and vectors
///
/// A const_buffer is a pair of iterators like `const char *` meant for sending
/// data; a mutable_buffer is a pair of iterators meant for receiving. In fact,
/// those types store signed char* and our convention is to represent readable
/// data with them. The const_raw_buffer and mutable_raw_buffer use unsigned
/// char* pairs and our convention is to represent unreadable/binary data with
/// them. Remember, these are conventions, not guarantees from these types. The
/// const_buffer is analogous (but doesn't inherit from) a string_view, so they
/// play well and convert easily between each other.
///
/// These templates offer tools for individual buffers as well as tools for
/// iterations of buffers.  An iteration of buffers is an iovector that is
/// passed to our sockets etc. The ircd::iov template can host an iteration of
/// buffers. The `template template` functions are tools for a container of
/// buffers of any permutation.
///
namespace ircd::buffer
{
	template<class it> struct buffer;
	struct const_buffer;
	struct mutable_buffer;
	struct const_raw_buffer;
	struct mutable_raw_buffer;
	template<class buffer, size_t align> struct unique_buffer;

	template<template<class> class I> using const_buffers = I<const_buffer>;
	template<template<class> class I> using mutable_buffers = I<mutable_buffer>;
	template<template<class> class I> using const_raw_buffers = I<const_raw_buffer>;
	template<template<class> class I> using mutable_raw_buffers = I<mutable_raw_buffer>;

	// Single buffer iteration of contents
	template<class it> const it &begin(const buffer<it> &buffer);
	template<class it> const it &end(const buffer<it> &buffer);
	template<class it> it &begin(buffer<it> &buffer);
	template<class it> it &end(buffer<it> &buffer);
	template<class it> it rbegin(const buffer<it> &buffer);
	template<class it> it rend(const buffer<it> &buffer);

	// Single buffer tools
	template<class it> size_t size(const buffer<it> &buffer);
	template<class it> const it &data(const buffer<it> &buffer);
	template<class it> size_t consume(buffer<it> &buffer, const size_t &bytes);
	template<class it> char *copy(char *&dest, char *const &stop, const buffer<it> &buffer);
	template<class it> size_t copy(char *const &dest, const size_t &max, const buffer<it> &buffer);
	template<class it> size_t copy(const mutable_buffer &dst, const buffer<it> &src);
	size_t copy(const mutable_buffer &dst, const string_view &src);
	template<class it> std::ostream &operator<<(std::ostream &s, const buffer<it> &buffer);

	// Iterable of buffers tools
	template<template<class> class I, class T> size_t size(const I<T> &buffers);
	template<template<class> class I, class T> char *copy(char *&dest, char *const &stop, const I<T> &buffer);
	template<template<class> class I, class T> size_t copy(char *const &dest, const size_t &max, const I<T> &buffer);
	template<template<class> class I, class T> size_t consume(I<T> &buffers, const size_t &bytes);
	template<template<class> class I, class T> std::ostream &operator<<(std::ostream &s, const I<T> &buffers);

	// Preconstructed null buffers
	extern const mutable_buffer null_buffer;
	extern const ilist<mutable_buffer> null_buffers;
}

// Export these important aliases down to main ircd namespace
namespace ircd
{
	using buffer::const_buffer;
	using buffer::mutable_buffer;
	using buffer::const_raw_buffer;
	using buffer::mutable_raw_buffer;
	using buffer::unique_buffer;
	using buffer::null_buffer;

	using buffer::const_buffers;
	using buffer::mutable_buffers;
}

/// Base for all buffer types
///
template<class it>
struct ircd::buffer::buffer
:std::tuple<it, it>
{
	using iterator = it;
	using value_type = typename std::remove_pointer<iterator>::type;

	operator string_view() const;
	operator std::string_view() const;
	explicit operator std::string() const;

	auto &begin() const                { return std::get<0>(*this);            }
	auto &begin()                      { return std::get<0>(*this);            }
	auto &end() const                  { return std::get<1>(*this);            }
	auto &end()                        { return std::get<1>(*this);            }

	auto &operator[](const size_t &i) const
	{
		return *(begin() + i);
	}

	auto &operator[](const size_t &i)
	{
		return *(begin() + i);
	}

	buffer(const it &start, const it &stop)
	:std::tuple<it, it>{start, stop}
	{}

	buffer(const it &start, const size_t &size)
	:buffer{start, start + size}
	{}

	template<size_t SIZE>
	buffer(value_type (&buf)[SIZE])
	:buffer{buf, SIZE}
	{}

	template<size_t SIZE>
	buffer(std::array<value_type, SIZE> &buf)
	:buffer{buf.data(), SIZE}
	{}

	buffer()
	:buffer{nullptr, nullptr}
	{}
};

namespace ircd::buffer
{
	template<class T> struct mutable_buffer_base;
}

/// Base for mutable buffers, or buffers which can be written to because they
/// are not const.
///
template<class T>
struct ircd::buffer::mutable_buffer_base
:buffer<T>
{
	using iterator = typename buffer<T>::iterator;
	using value_type = typename buffer<T>::value_type;

	// Allows boost::spirit to append to the buffer; this means the size() of
	// this buffer becomes a consumption counter and the real size of the buffer
	// must be kept separately. This is the lowlevel basis for a stream buffer.
	void insert(const iterator &it, const value_type &v)
	{
		assert(it >= this->begin() && it <= this->end());
		memmove(it + 1, it, std::distance(it, this->end()));
		*it = v;
		++std::get<1>(*this);
	}

	using buffer<T>::buffer;
	mutable_buffer_base(): buffer<T>{} {}

	// lvalue string reference offered to write through to a std::string as
	// the buffer. not explicit; should be hard to bind by accident...
	mutable_buffer_base(std::string &buf)
	:mutable_buffer_base<iterator>{const_cast<iterator>(buf.data()), buf.size()}
	{}
};

/// A writable buffer of signed char data. Convention is for this buffer to
/// represent readable strings, which may or may not be null terminated. This
/// is just a convention; not a gurantee of the type.
///
struct ircd::buffer::mutable_buffer
:mutable_buffer_base<char *>
{
	// Conversion offered for the analogous asio buffer
	operator boost::asio::mutable_buffer() const;

	using mutable_buffer_base<char *>::mutable_buffer_base;
};

/// A writable buffer of unsigned signed char data. Convention is for this
/// buffer to represent unreadable binary data. It may also be constructed
/// from a mutable_buffer because a buffer of any data (this one) can also
/// be readable data. The inverse is not true, mutable_buffer cannot be
/// constructed from this class.
///
struct ircd::buffer::mutable_raw_buffer
:mutable_buffer_base<unsigned char *>
{
	// Conversion offered for the analogous asio buffer
	operator boost::asio::mutable_buffer() const;

	using mutable_buffer_base<iterator>::mutable_buffer_base;

	mutable_raw_buffer(const mutable_buffer &b)
	:mutable_buffer_base<iterator>{reinterpret_cast<iterator>(data(b)), size(b)}
	{}
};

namespace ircd::buffer
{
	template<class T> struct const_buffer_base;
}

template<class T>
struct ircd::buffer::const_buffer_base
:buffer<T>
{
	using iterator = typename buffer<T>::iterator;
	using value_type = typename buffer<T>::value_type;

	using buffer<iterator>::buffer;
	const_buffer_base(): buffer<T>{} {}

	const_buffer_base(const mutable_buffer &b)
	:buffer<iterator>{reinterpret_cast<iterator>(data(b)), size(b)}
	{}

	const_buffer_base(const string_view &s)
	:buffer<iterator>{reinterpret_cast<iterator>(s.data()), s.size()}
	{}

	explicit const_buffer_base(const std::string &s)
	:buffer<iterator>{reinterpret_cast<iterator>(s.data()), s.size()}
	{}
};

struct ircd::buffer::const_buffer
:const_buffer_base<const char *>
{
	operator boost::asio::const_buffer() const;

	using const_buffer_base<iterator>::const_buffer_base;
};

struct ircd::buffer::const_raw_buffer
:const_buffer_base<const unsigned char *>
{
	operator boost::asio::const_buffer() const;

	using const_buffer_base<iterator>::const_buffer_base;

	const_raw_buffer(const const_buffer &b)
	:const_buffer_base{reinterpret_cast<const unsigned char *>(data(b)), size(b)}
	{}

	const_raw_buffer(const mutable_raw_buffer &b)
	:const_buffer_base{reinterpret_cast<const unsigned char *>(data(b)), size(b)}
	{}
};

/// Like unique_ptr, this template holds ownership of an allocated buffer
///
///
template<class buffer,
         size_t align = 16>
struct ircd::buffer::unique_buffer
:buffer
{
	unique_buffer(std::unique_ptr<uint8_t[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	unique_buffer() = default;
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(std::unique_ptr<uint8_t[]> &&b,
                                                              const size_t &size)
:buffer
{
	typename buffer::iterator(b.release()), size
}
{
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(const size_t &size)
:unique_buffer<buffer, alignment>
{
	std::unique_ptr<uint8_t[]>
	{
		new __attribute__((aligned(alignment))) uint8_t[size]
	},
	size
}
{
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(unique_buffer &&other)
noexcept
:buffer
{
	std::move(other)
}
{
	get<0>(other) = nullptr;
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::~unique_buffer()
noexcept
{
	delete[] data(*this);
}

template<template<class>
         class buffers,
         class T>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffers<T> &b)
{
	std::for_each(std::begin(b), std::end(b), [&s]
	(const T &b)
	{
		s << b;
	});

	return s;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::consume(buffers<T> &b,
                      const size_t &bytes)
{
	size_t remain(bytes);
	for(auto it(std::begin(b)); it != std::end(b) && remain > 0; ++it)
	{
		using buffer = typename buffers<T>::value_type;
		using iterator = typename buffer::iterator;

		buffer &b(const_cast<buffer &>(*it));
		remain -= consume(b, remain);
	}

	return bytes - remain;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::copy(char *const &dest,
                   const size_t &max,
                   const buffers<T> &b)
{
	size_t ret(0);
	for(const T &b : b)
		ret += copy(dest + ret, max - ret, b);

	return ret;
}

template<template<class>
         class buffers,
         class T>
char *
ircd::buffer::copy(char *&dest,
                   char *const &stop,
                   const buffers<T> &b)
{
	char *const ret(dest);
	for(const T &b : b)
		copy(dest, stop, b);

	return ret;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::size(const buffers<T> &b)
{
	return std::accumulate(std::begin(b), std::end(b), size_t(0), []
	(auto ret, const T &b)
	{
		return ret += size(b);
	});
}

template<class it>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffer<it> &buffer)
{
	s.write(data(buffer), size(buffer));
	return s;
}

inline size_t
ircd::buffer::copy(const mutable_buffer &dst,
                   const string_view &s)
{
	return copy(dst, const_buffer{s});
}

template<class it>
size_t
ircd::buffer::copy(const mutable_buffer &dst,
                   const buffer<it> &src)
{
	auto e(begin(dst));
	auto b(copy(e, end(dst), src));
	return std::distance(b, e);
}

template<class it>
size_t
ircd::buffer::copy(char *const &dest,
                   const size_t &max,
                   const buffer<it> &buffer)
{
	if(!max)
		return 0;

	char *out(dest);
	char *const stop(dest + max - 1);
	copy(out, stop, buffer);
	*out = '\0';
	return std::distance(dest, out);
}

template<class it>
char *
ircd::buffer::copy(char *&dest,
                   char *const &stop,
                   const buffer<it> &buffer)
{
	char *const ret(dest);
	const size_t remain(stop - dest);
	dest += std::min(size(buffer), remain);
	memcpy(ret, data(buffer), dest - ret);
	return ret;
}

template<class it>
size_t
ircd::buffer::consume(buffer<it> &buffer,
                      const size_t &bytes)
{
	get<0>(buffer) += std::min(size(buffer), bytes);
	return size(buffer);
}

template<class it>
const it &
ircd::buffer::data(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
size_t
ircd::buffer::size(const buffer<it> &buffer)
{
	assert(get<0>(buffer) <= get<1>(buffer));
	return std::distance(get<0>(buffer), get<1>(buffer));
}

template<class it>
it
ircd::buffer::rend(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer));
}

template<class it>
it
ircd::buffer::rbegin(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer) + size(buffer));
}

template<class it>
it &
ircd::buffer::end(buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
it &
ircd::buffer::begin(buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
const it &
ircd::buffer::end(const buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
const it &
ircd::buffer::begin(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
ircd::buffer::buffer<it>::operator std::string()
const
{
	return { get<0>(*this), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string_view()
const
{
	return { get<0>(*this), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { get<0>(*this), get<1>(*this) };
}
