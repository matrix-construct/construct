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
/// data; a mutable_buffer is a pair of iterators meant for receiving. These
/// templates offer tools for individual buffers as well as tools for
/// iterations of buffers.
///
/// An iteration of buffers is an iovector that is passed to our sockets etc.
/// The ircd::iov template can host an iteration of buffers. The
/// `template template` functions are tools for a container of buffers of
/// either type.
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
	template<class it> size_t copy(mutable_buffer &dst, const buffer<it> &src);
	size_t copy(mutable_buffer &dst, const string_view &src);
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

template<class it>
struct ircd::buffer::buffer
:std::tuple<it, it>
{
	using value_type = it;

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
	buffer(typename std::remove_pointer<it>::type (&buf)[SIZE])
	:buffer{buf, SIZE}
	{}

	buffer()
	:buffer{nullptr, nullptr}
	{}
};

struct ircd::buffer::mutable_buffer
:buffer<char *>
{
	operator boost::asio::mutable_buffer() const;

	using buffer<value_type>::buffer;
};

struct ircd::buffer::mutable_raw_buffer
:buffer<unsigned char *>
{
	operator boost::asio::mutable_buffer() const;

	using buffer<value_type>::buffer;

	mutable_raw_buffer(const mutable_buffer &b)
	:buffer<value_type>{reinterpret_cast<value_type>(data(b)), size(b)}
	{}
};

struct ircd::buffer::const_buffer
:buffer<const char *>
{
	operator boost::asio::const_buffer() const;

	using buffer<value_type>::buffer;

	const_buffer(const mutable_buffer &b)
	:buffer<value_type>{data(b), size(b)}
	{}

	const_buffer(const string_view &s)
	:buffer<value_type>{std::begin(s), std::end(s)}
	{}

	explicit const_buffer(const std::string &s)
	:buffer<value_type>{s.data(), s.size()}
	{}
};

struct ircd::buffer::const_raw_buffer
:buffer<const unsigned char *>
{
	operator boost::asio::const_buffer() const;

	using buffer<value_type>::buffer;

	const_raw_buffer(const const_buffer &b)
	:buffer<value_type>{reinterpret_cast<value_type>(data(b)), size(b)}
	{}

	const_raw_buffer(const mutable_raw_buffer &b)
	:buffer<value_type>{data(b), size(b)}
	{}

	const_raw_buffer(const mutable_buffer &b)
	:const_raw_buffer{mutable_raw_buffer{b}}
	{}
};

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
		using iterator = typename buffer::value_type;

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
ircd::buffer::copy(mutable_buffer &dst,
                   const string_view &s)
{
	return copy(dst, const_buffer{s});
}

template<class it>
size_t
ircd::buffer::copy(mutable_buffer &dst,
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
	typename buffer::value_type(b.release()), size
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
