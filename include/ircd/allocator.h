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
#define HAVE_IRCD_ALLOCATOR_H

namespace ircd
{
	struct allocator;
}

struct ircd::allocator
{
	struct state;
	template<class T = char> struct dynamic;
	template<class T = char, size_t = 512> struct fixed;
};

struct ircd::allocator::state
{
	using word_t                                 = unsigned long long;
	using size_type                              = std::size_t;

	size_t size                                  { 0                                               };
	word_t *avail                                { nullptr                                         };
	size_t last                                  { 0                                               };

	static uint byte(const uint &i)              { return i / (sizeof(word_t) * 8);                }
	static uint bit(const uint &i)               { return i % (sizeof(word_t) * 8);                }
	static word_t mask(const uint &pos)          { return word_t(1) << bit(pos);                   }

	bool test(const uint &pos) const             { return avail[byte(pos)] & mask(pos);            }
	void bts(const uint &pos)                    { avail[byte(pos)] |= mask(pos);                  }
	void btc(const uint &pos)                    { avail[byte(pos)] &= ~mask(pos);                 }

	uint next(const size_t &n) const;
	void deallocate(const uint &p, const size_t &n);
	uint allocate(const size_t &n, const uint &hint = -1);

	state(const size_t &size = 0,
	      word_t *const &avail = nullptr)
	:size{size}
	,avail{avail}
	,last{0}
	{}
};

template<class T,
         size_t max>
struct ircd::allocator::fixed
:state
{
	struct allocator;

	std::array<word_t, max / 8> avail            {{ 0                                             }};
	std::array<T, max> buf alignas(16);

  public:
	allocator operator()();
	operator allocator();

	fixed()
	:state{max, avail.data()}
	{}
};

template<class T>
struct ircd::allocator::dynamic
:state
{
	struct allocator;

	size_t head_size, data_size;
	std::unique_ptr<uint8_t[]> arena;
	T *buf;

  public:
	allocator operator()();
	operator allocator();

	dynamic(const size_t &size)
	:state{size}
	,head_size{size / 8}
	,data_size{sizeof(T) * size + 16}
	,arena
	{
		new uint8_t[head_size + data_size]
	}
	,buf
	{
		reinterpret_cast<T *>(arena.get() + head_size + (head_size % 16))
	}
	{
		state::avail = reinterpret_cast<word_t *>(arena.get());
	}
};

template<class T,
         size_t size>
struct ircd::allocator::fixed<T, size>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	fixed *s;

  public:
	template<class U, size_t S> struct rebind
	{
		using other = typename fixed<U, S>::allocator;
	};

	size_type max_size() const                   { return size;                                    }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		const uint hintpos(hint? hint - s->buf.data() : -1);
		return s->buf.data() + s->state::allocate(n, hint);
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		const uint pos(p - s->buf.data());
		s->state::deallocate(pos, n);
	}

	template<class U, size_t S>
	allocator(const typename fixed<U, S>::allocator &) noexcept
	:s{reinterpret_cast<fixed *>(s.s)}
	{}

	allocator(fixed &s) noexcept
	:s{&s}
	{}

	allocator(allocator &&) = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

template<class T>
struct ircd::allocator::dynamic<T>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	dynamic *s;

  public:
	template<class U> struct rebind
	{
		using other = typename dynamic<U>::allocator;
	};

	size_type max_size() const                   { return s->size;                                 }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr)
	{
		const uint hintpos(hint? hint - s->buf : -1);
		return s->buf + s->state::allocate(n, hintpos);
	}

	void deallocate(const pointer &p, const size_type &n)
	{
		const uint pos(p - s->buf);
		s->state::deallocate(pos, n);
	}

	template<class U>
	allocator(const typename dynamic<U>::allocator &) noexcept
	:s{reinterpret_cast<dynamic *>(s.s)}
	{}

	allocator(dynamic &s) noexcept
	:s{&s}
	{}

	allocator(allocator &&) = default;
	allocator(const allocator &) = default;

	friend bool operator==(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}

	friend bool operator!=(const allocator &a, const allocator &b)
	{
		return &a == &b;
	}
};

template<class T>
typename ircd::allocator::dynamic<T>::allocator
ircd::allocator::dynamic<T>::operator()()
{
	return { *this };
}

template<class T>
ircd::allocator::dynamic<T>::operator
allocator()
{
	return { *this };
}

template<class T,
         size_t size>
typename ircd::allocator::fixed<T, size>::allocator
ircd::allocator::fixed<T, size>::operator()()
{
	return { *this };
}

template<class T,
         size_t size>
ircd::allocator::fixed<T, size>::operator
allocator()
{
	return { *this };
}

inline void
ircd::allocator::state::deallocate(const uint &pos,
                                   const size_type &n)
{
	for(size_t i(0); i < n; ++i)
		btc(pos + i);
}

inline uint
ircd::allocator::state::allocate(const size_type &n,
                                 const uint &hint)
{
	const auto next(this->next(n));
	if(unlikely(next >= size))         // No block of n was found anywhere (next is past-the-end)
		throw std::bad_alloc();

	for(size_t i(0); i < n; ++i)
		bts(next + i);

	last = next + n;
	return next;
}

inline uint
ircd::allocator::state::next(const size_t &n)
const
{
	uint ret(last), rem(n);
	for(; ret < size && rem; ++ret)
		if(test(ret))
			rem = n;
		else
			--rem;

	if(likely(!rem))
		return ret - n;

	for(ret = 0, rem = n; ret < last && rem; ++ret)
		if(test(ret))
			rem = n;
		else
			--rem;

	if(unlikely(rem))                  // The allocator should throw std::bad_alloc if !rem
		return size;

	return ret - n;
}
