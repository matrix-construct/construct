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
	template<class buffer, size_t SIZE> struct fixed_buffer;
	template<class buffer, uint align = 16> struct unique_buffer;
	struct stream_buffer;

	template<size_t SIZE> using fixed_const_buffer = fixed_buffer<const_buffer, SIZE>;
	template<size_t SIZE> using fixed_mutable_buffer = fixed_buffer<mutable_buffer, SIZE>;
	template<size_t SIZE> using fixed_const_raw_buffer = fixed_buffer<const_raw_buffer, SIZE>;
	template<size_t SIZE> using fixed_mutable_raw_buffer = fixed_buffer<mutable_raw_buffer, SIZE>;

	template<template<class> class I> using const_buffers = I<const_buffer>;
	template<template<class> class I> using mutable_buffers = I<mutable_buffer>;
	template<template<class> class I> using const_raw_buffers = I<const_raw_buffer>;
	template<template<class> class I> using mutable_raw_buffers = I<mutable_raw_buffer>;

	// Preconstructed null buffers
	extern const mutable_buffer null_buffer;
	extern const ilist<mutable_buffer> null_buffers;

	// Single buffer iteration of contents
	template<class it> const it &begin(const buffer<it> &buffer);
	template<class it> const it &end(const buffer<it> &buffer);
	template<class it> it &begin(buffer<it> &buffer);
	template<class it> it &end(buffer<it> &buffer);
	template<class it> it rbegin(const buffer<it> &buffer);
	template<class it> it rend(const buffer<it> &buffer);

	// Single buffer tools
	template<class it> bool null(const buffer<it> &buffer);
	template<class it> bool full(const buffer<it> &buffer);
	template<class it> bool empty(const buffer<it> &buffer);
	template<class it> bool operator!(const buffer<it> &buffer);
	template<class it> size_t size(const buffer<it> &buffer);
	template<class it> const it &data(const buffer<it> &buffer);
	template<class it> size_t consume(buffer<it> &buffer, const size_t &bytes);
	template<class it> it copy(it &dest, const it &stop, const const_raw_buffer &);
	template<class it> size_t copy(const it &dest, const size_t &max, const const_raw_buffer &buffer);
	size_t copy(const mutable_raw_buffer &dst, const const_raw_buffer &src);
	size_t reverse(const mutable_raw_buffer &dst, const const_raw_buffer &src);
	void reverse(const mutable_raw_buffer &buf);
	void zero(const mutable_raw_buffer &buf);

	// Iterable of buffers tools
	template<template<class> class I, class T> size_t size(const I<T> &buffers);
	template<template<class> class I, class T> size_t copy(const mutable_raw_buffer &, const I<T> &buffer);
	template<template<class> class I, class T> size_t consume(I<T> &buffers, const size_t &bytes);

	// Convenience copy to std stream
	template<class it> std::ostream &operator<<(std::ostream &s, const buffer<it> &buffer);
	template<template<class> class I, class T> std::ostream &operator<<(std::ostream &s, const I<T> &buffers);
}

// Export these important aliases down to main ircd namespace
namespace ircd
{
	using buffer::const_buffer;
	using buffer::mutable_buffer;
	using buffer::const_raw_buffer;
	using buffer::mutable_raw_buffer;
	using buffer::fixed_buffer;
	using buffer::unique_buffer;
	using buffer::null_buffer;
	using buffer::stream_buffer;
	using buffer::fixed_const_buffer;
	using buffer::fixed_mutable_buffer;
	using buffer::fixed_const_raw_buffer;
	using buffer::fixed_mutable_raw_buffer;

	using buffer::const_buffers;
	using buffer::mutable_buffers;

	using buffer::size;
	using buffer::data;
	using buffer::copy;
	using buffer::consume;
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

	mutable_buffer_base()
	:buffer<iterator>{}
	{}

	template<size_t SIZE>
	mutable_buffer_base(value_type (&buf)[SIZE])
	:buffer<iterator>{buf, SIZE}
	{}

	template<size_t SIZE>
	mutable_buffer_base(std::array<value_type, SIZE> &buf)
	:buffer<iterator>{reinterpret_cast<iterator>(buf.data()), SIZE}
	{}

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

	mutable_buffer(const std::function<void (const mutable_buffer &)> &closure)
	{
		closure(*this);
	}

	mutable_buffer() = default;
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

	using mutable_buffer_base<unsigned char *>::mutable_buffer_base;

	mutable_raw_buffer(const mutable_buffer &b)
	:mutable_buffer_base<iterator>{reinterpret_cast<iterator>(data(b)), size(b)}
	{}

	mutable_raw_buffer(const std::function<void (const mutable_raw_buffer &)> &closure)
	{
		closure(*this);
	}

	mutable_raw_buffer() = default;
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
	using mutable_value_type = typename std::remove_const<value_type>::type;

	using buffer<T>::buffer;

	const_buffer_base()
	:buffer<iterator>{}
	{}

	template<size_t SIZE>
	const_buffer_base(const value_type (&buf)[SIZE])
	:buffer<iterator>{buf, SIZE}
	{}

	template<size_t SIZE>
	const_buffer_base(const std::array<mutable_value_type, SIZE> &buf)
	:buffer<iterator>{reinterpret_cast<iterator>(buf.data()), SIZE}
	{}

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

	explicit const_buffer(const std::string &s)
	:const_buffer_base{s}
	{}
};

struct ircd::buffer::const_raw_buffer
:const_buffer_base<const unsigned char *>
{
	operator boost::asio::const_buffer() const;

	using const_buffer_base<iterator>::const_buffer_base;

	const_raw_buffer(const const_buffer &b)
	:const_buffer_base{reinterpret_cast<iterator>(data(b)), size(b)}
	{}

	const_raw_buffer(const mutable_raw_buffer &b)
	:const_buffer_base{reinterpret_cast<iterator>(data(b)), size(b)}
	{}

	explicit const_raw_buffer(const std::string &s)
	:const_buffer_base{reinterpret_cast<iterator>(s.data()), s.size()}
	{}
};

/// fixed_buffer wraps an std::array with construction and conversions apropos
/// the ircd::buffer suite. fixed_buffer should be punnable. Its only memory
/// footprint is the array itself and
///
template<class buffer,
         size_t SIZE>
struct ircd::buffer::fixed_buffer
:std::array<typename std::remove_const<typename buffer::value_type>::type, SIZE>
{
	using mutable_type = typename std::remove_const<typename buffer::value_type>::type;
	using const_type = typename std::add_const<mutable_type>::type;
	using array_type = std::array<mutable_type, SIZE>;

	operator buffer() const
	{
		return { std::begin(*this), std::end(*this) };
	}

	operator buffer()
	{
		return { std::begin(*this), std::end(*this) };
	}

	using array_type::array_type;
	fixed_buffer(const nullptr_t &)
	:array_type{{0}}
	{}

	fixed_buffer(const std::function<void (const mutable_buffer &)> &closure)
	{
		closure(mutable_buffer{reinterpret_cast<mutable_buffer::iterator>(this->data()), this->size()});
	}

	fixed_buffer(buffer b)
	:array_type{std::begin(b), std::end(b)}
	{}

	fixed_buffer() = default;
};

static_assert
(
	// Assertion over an arbitrary but common template configuration.
	std::is_standard_layout<ircd::buffer::fixed_buffer<ircd::buffer::const_buffer, 32>>::value,
	"ircd::buffer::fixed_buffer must be standard layout"
);

struct ircd::buffer::stream_buffer
:mutable_buffer
{
	mutable_buffer base;

	/// Bytes remaining for writes to the stream buffer (same as size(*this))
	size_t remaining() const
	{
		assert(begin() <= base.end());
		const size_t ret(std::distance(begin(), base.end()));
		assert(ret == size(*this));
		return ret;
	}

	/// Bytes used by writes to the stream buffer
	size_t consumed() const
	{
		assert(begin() >= base.begin());
		assert(begin() <= base.end());
		return std::distance(base.begin(), begin());
	}

	/// View the completed portion of the stream
	const_buffer completed() const
	{
		assert(base.begin() <= begin());
		assert(base.begin() + consumed() <= base.end());
		return { base.begin(), base.begin() + consumed() };
	}

	/// Convenience closure presenting the writable window and advancing the
	/// window with a consume() for the bytes written in the closure.
	using closure = std::function<size_t (const mutable_buffer &)>;
	void operator()(const closure &closure)
	{
		consume(*this, closure(*this));
	}

	stream_buffer(const mutable_buffer &base)
	:mutable_buffer{base}
	,base{base}
	{}
};

/// Like unique_ptr, this template holds ownership of an allocated buffer
///
///
template<class buffer,
         uint alignment>
struct ircd::buffer::unique_buffer
:buffer
{
	unique_buffer(std::unique_ptr<uint8_t[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	unique_buffer();
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	unique_buffer &operator=(unique_buffer &&) noexcept;
	unique_buffer &operator=(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer()
:buffer
{
	nullptr, nullptr
}
{}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(std::unique_ptr<uint8_t[]> &&b,
                                                              const size_t &size)
:buffer
{
	typename buffer::iterator(b.release()), size
}
{}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(const size_t &size)
:unique_buffer<buffer, alignment>
{
	std::unique_ptr<uint8_t[]>
	{
		//TODO: Can't use a template parameter to the attribute even though
		// it's known at compile time. Hardcoding this until fixed with better
		// aligned dynamic memory.
		//new __attribute__((aligned(alignment))) uint8_t[size]
		new __attribute__((aligned(16))) uint8_t[size]
	},
	size
}
{
	// Alignment can only be 16 bytes for now
	assert(alignment == 16);
}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(unique_buffer &&other)
noexcept
:buffer
{
	std::move(static_cast<buffer &>(other))
}
{
	get<0>(other) = nullptr;
}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment> &
ircd::buffer::unique_buffer<buffer, alignment>::operator=(unique_buffer &&other)
noexcept
{
	this->~unique_buffer();

	static_cast<buffer &>(*this) = std::move(static_cast<buffer &>(other));
	get<0>(other) = nullptr;

	return *this;
}

template<class buffer,
         uint alignment>
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
	ssize_t remain(bytes);
	for(auto it(std::begin(b)); it != std::end(b) && remain > 0; ++it)
	{
		using buffer = typename buffers<T>::value_type;
		using iterator = typename buffer::iterator;

		buffer &b(const_cast<buffer &>(*it));
		const ssize_t bsz(size(b));
		const ssize_t csz{std::min(remain, bsz)};
		remain -= consume(b, csz);
		assert(remain >= 0);
	}

	assert(ssize_t(bytes) >= remain);
	return bytes - remain;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::copy(const mutable_raw_buffer &dest,
                   const buffers<T> &b)
{
	size_t ret(0);
	for(const T &b : b)
		ret += copy(data(dest) + ret, size(dest) - ret, b);

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
	assert(!null(buffer) || get<1>(buffer) == nullptr);
	s.write(data(buffer), size(buffer));
	return s;
}

inline void
ircd::buffer::reverse(const mutable_raw_buffer &buf)
{
	std::reverse(data(buf), data(buf) + size(buf));
}

inline size_t
ircd::buffer::reverse(const mutable_raw_buffer &dst,
                      const const_raw_buffer &src)
{
	const size_t ret{std::min(size(dst), size(src))};
	std::reverse_copy(data(src), data(src) + ret, data(dst));
	return ret;
}

inline size_t
ircd::buffer::copy(const mutable_raw_buffer &dst,
                   const const_raw_buffer &src)
{
	auto e{begin(dst)};
	copy(e, end(dst), src);
	assert(std::distance(begin(dst), e) >= 0);
	return std::distance(begin(dst), e);
}

template<class it>
size_t
ircd::buffer::copy(const it &dest,
                   const size_t &max,
                   const const_raw_buffer &src)
{
	if(!max)
		return 0;

	auto start{dest};
	const auto stop{dest + max - 1};
	assert(stop >= start);
	copy<it>(start, stop, src);
	assert(start <= stop);
	assert(start < dest + max);
	*start = '\0';
	assert(std::distance(dest, start) >= 1);
	return std::distance(dest, start);
}

template<class it>
it
ircd::buffer::copy(it &dest,
                   const it &stop,
                   const const_raw_buffer &src)
{
	const it ret{dest};
	const ssize_t srcsz(size(src));
	assert(ret <= stop);
	const ssize_t remain{std::distance(ret, stop)};
	const ssize_t cpsz{std::min(srcsz, remain)};
	assert(cpsz <= srcsz);
	assert(cpsz <= remain);
	assert(remain >= 0);
	memcpy(ret, data(src), cpsz);
	dest += cpsz;
	assert(dest <= stop);
	return ret;
}

template<class it>
size_t
ircd::buffer::consume(buffer<it> &buffer,
                      const size_t &bytes)
{
	assert(!null(buffer));
	assert(bytes <= size(buffer));
	get<0>(buffer) += bytes;
	assert(get<0>(buffer) <= get<1>(buffer));
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
	assert(!null(buffer) || get<1>(buffer) == nullptr);
	return std::distance(get<0>(buffer), get<1>(buffer));
}

template<class it>
bool
ircd::buffer::operator!(const buffer<it> &buffer)
{
	return empty(buffer);
}

template<class it>
bool
ircd::buffer::empty(const buffer<it> &buffer)
{
	return null(buffer) || std::distance(get<0>(buffer), get<1>(buffer)) == 0;
}

template<class it>
bool
ircd::buffer::full(const buffer<it> &buffer)
{
	return std::distance(get<0>(buffer), get<1>(buffer)) == 0;
}

template<class it>
bool
ircd::buffer::null(const buffer<it> &buffer)
{
	return get<0>(buffer) == nullptr;
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
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}
