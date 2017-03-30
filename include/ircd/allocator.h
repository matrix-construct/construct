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

namespace ircd {

struct allocator
{
	template<class T = char, size_t = 512> struct fixed;
};

template<class T,
         size_t size>
struct allocator::fixed
{
	struct allocator;
	using word_t = unsigned long long;

	size_t last                                  { 0                                               };
	std::array<word_t, size / 8> avail           {{ 0                                             }};
	std::array<T, size> buf alignas(16);

	static constexpr uint word_bytes             { sizeof(word_t)                                  };
	static constexpr uint word_bits              { word_bytes * 8                                  };
	static uint byte(const uint &i)              { return i / word_bits;                           }
	static uint bit(const uint &i)               { return i % word_bits;                           }
	static word_t mask(const uint &pos)          { return word_t(1) << bit(pos);                   }

	bool test(const uint &pos) const             { return avail[byte(pos)] & mask(pos);            }
	void bts(const uint &pos)                    { avail[byte(pos)] |= mask(pos);                  }
	void btc(const uint &pos)                    { avail[byte(pos)] &= ~mask(pos);                 }

	uint next(const size_t &n) const;

	allocator operator()();
	operator allocator();
};

template<class T,
         size_t size>
struct allocator::fixed<T, size>::allocator
{
	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	template<class U> struct rebind
	{
		using other = typename fixed<U, size>::allocator;
	};

	size_type max_size() const                   { return size;                                    }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	fixed *s;

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr);
	void deallocate(const pointer &p, const size_type &n);

	allocator(fixed &) noexcept;
	allocator(allocator &&) = default;
	allocator(const allocator &) = default;
	template<class U, size_t S> allocator(const typename fixed<U, S>::allocator &) noexcept;
};

template<class... T1,
         class... T2>
bool operator==(const typename allocator::fixed<T1...>::allocator &,
                const typename allocator::fixed<T2...>::allocator &);

template<class... T1,
         class... T2>
bool operator!=(const typename allocator::fixed<T1...>::allocator &,
                const typename allocator::fixed<T2...>::allocator &);

} // namespace ircd


template<class T,
         size_t size>
ircd::allocator::fixed<T, size>::allocator::allocator(fixed &s)
noexcept
:s{&s}
{}

template<class T,
         size_t size>
template<class U,
         size_t S>
ircd::allocator::fixed<T, size>::allocator::allocator(const typename fixed<U, S>::allocator &s)
noexcept
:s
{
	reinterpret_cast<typename fixed<T, size>::state *>(s.s)
}
{}

template<class T,
         size_t size>
void
ircd::allocator::fixed<T, size>::allocator::deallocate(const pointer &p,
                                                       const size_type &n)
{
	const uint pos(p - s->buf.data());
	for(size_t i(0); i < n; ++i)
		s->btc(pos + i);
}

template<class T,
         size_t size>
typename ircd::allocator::fixed<T, size>::allocator::pointer
ircd::allocator::fixed<T, size>::allocator::allocate(const size_type &n,
                                                     const const_pointer &hint)
{
	const auto next(s->next(n));
	if(unlikely(next >= size))         // No block of n was found anywhere (next is past-the-end)
		throw std::bad_alloc();

	for(size_t i(0); i < n; ++i)
		s->bts(next + i);

	s->last = next + n;
	return s->buf.data() + next;
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

template<class T,
         size_t size>
uint
ircd::allocator::fixed<T, size>::next(const size_t &n)
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

template<class... T1,
         class... T2>
bool
ircd::operator==(const typename allocator::fixed<T1...>::allocator &a,
                 const typename allocator::fixed<T2...>::allocator &b)
{
	return &a == &b;
}

template<class... T1,
         class... T2>
bool
ircd::operator!=(const typename allocator::fixed<T1...>::allocator &a,
                 const typename allocator::fixed<T2...>::allocator &b)
{
	return &a != &b;
}
