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
	struct state;

	using value_type         = T;
	using pointer            = T *;
	using const_pointer      = const T *;
	using reference          = T &;
	using const_reference    = const T &;
	using size_type          = std::size_t;
	using difference_type    = std::ptrdiff_t;

	template<class U> struct rebind
	{
		typedef allocator::fixed<U, size> other;
	};

	size_type max_size() const                   { return size;                                    }
	auto address(reference x) const              { return &x;                                      }
	auto address(const_reference x) const        { return &x;                                      }

	state *s;

	pointer allocate(const size_type &n, const const_pointer &hint = nullptr);
	void deallocate(const pointer &p, const size_type &n);

	fixed(state &) noexcept;
	fixed(fixed &&) = default;
	fixed(const fixed &) = default;
	template<class U, size_t S> fixed(const fixed<U, S> &) noexcept;
};

template<class T,
         size_t size>
struct allocator::fixed<T, size>::state
{
	using word_t = unsigned long long;

	size_t last                                  { 0                                               };
	std::array<word_t, size / 8> avail           { 0                                               };
	std::array<T, size> buf;

	static constexpr uint word_bytes             { sizeof(word_t)                                  };
	static constexpr uint word_bits              { word_bytes * 8                                  };
	static uint byte(const uint &i)              { return i / word_bits;                           }
	static uint bit(const uint &i)               { return i % word_bits;                           }

	bool bt(const uint &pos) const               { return avail[byte(pos)] & (1ULL << bit(pos));   }
	void bts(const uint &pos)                    { avail[byte(pos)] |= (1ULL << bit(pos));         }
	void btc(const uint &pos)                    { avail[byte(pos)] &= ~(1ULL << bit(pos));        }

	uint next(const size_t &n) const;

	allocator::fixed<T, size> operator()()       { return { *this };                               }
};

template<class T,
         size_t size>
allocator::fixed<T, size>::fixed(state &s)
noexcept
:s{&s}
{
}

template<class T,
         size_t size>
template<class U,
         size_t S>
allocator::fixed<T, size>::fixed(const fixed<U, S> &)
noexcept
{
	assert(0);
}

template<class T,
         size_t size>
void
allocator::fixed<T, size>::deallocate(const pointer &p,
                                      const size_type &n)
{
	const uint pos(p - s->buf.data());
	for(size_t i(0); i < n; ++i)
		s->btc(pos + i);
}

template<class T,
         size_t size>
typename allocator::fixed<T, size>::pointer
allocator::fixed<T, size>::allocate(const size_type &n,
                                    const const_pointer &hint)
{
	const auto next(s->next(n));
	if(unlikely(next >= size))
		throw std::bad_alloc();

	for(size_t i(0); i < n; ++i)
		s->bts(next + i);

	s->last = next + n;
	return s->buf.data() + next;
}

template<class T,
         size_t size>
uint
allocator::fixed<T, size>::state::next(const size_t &n)
const
{
	// Search for a contiguous block of n, starting from after the last alloc.
	uint ret(last), rem(n);
	for(; ret < size && rem; ++ret)
		if(bt(ret))
			rem = n;
		else
			--rem;

	// A block of n was found between the last alloc and the end.
	if(likely(!rem))
		return ret - n;

	// Circle around and search between 0 to last
	for(ret = 0, rem = n; ret < last && rem; ++ret)
		if(bt(ret))
			rem = n;
		else
			--rem;

	// No block of n was found anywhere, report size past-the-end
	// The allocator should throw std::bad_alloc with this value.
	if(unlikely(rem))
		return size;

	return ret - n;
}

template<class... T1,
         class... T2>
bool operator==(const allocator::fixed<T1...> &a, const allocator::fixed<T2...> &b)
{
	return &a == &b;
}

template<class... T1,
         class... T2>
bool operator!=(const allocator::fixed<T1...> &a, const allocator::fixed<T2...> &b)
{
	return &a != &b;
}

} // namespace ircd
