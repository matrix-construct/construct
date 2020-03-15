// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/simd.h>

namespace ircd
{
	template<class i8xN,
	         size_t N>
	static size_t indexof(const string_view &, const std::array<string_view, N> &);
}

size_t
ircd::indexof(const string_view &s,
              const string_views &tab)
{
	#if defined(__AVX__)
		static const size_t N {32};
		using i8xN = i8x32;
	#elif defined(__SSE__)
		static const size_t N {16};
		using i8xN = i8x16;
	#else
		static const size_t N {1};
		using i8xN = char __attribute__((vector_size(1)));
	#endif

	size_t i, j, ret;
	std::array<string_view, N> a;
	for(i = 0; i < tab.size() / N; ++i)
	{
		for(j = 0; j < N; ++j)
			a[j] = tab[i * N + j];

		if((ret = indexof<i8xN, N>(s, a)) != N)
			return i * N + ret;
	}

	#pragma clang loop unroll (disable)
	for(j = 0; j < N; ++j)
		a[j] = i * N + j < tab.size()?
			tab[i * N + j]:
			string_view{};

	return i * N + indexof<i8xN, N>(s, a);
}

template<class i8xN,
         size_t N>
size_t
ircd::indexof(const string_view &s,
              const std::array<string_view, N> &st)
{
	i8xN ct, res;
	size_t i, j, k;
	for(i = 0; i < N; ++i)
		res[i] = true;

	const size_t max(s.size());
	for(i = 0, j = 0; i < max; i = (j < N)? i + 1 : max)
	{
		#pragma clang loop unroll (disable)
		for(k = 0; k < N; ++k)
		{
			res[k] &= size(st[k]) > i;
			ct[k] = res[k]? st[k][i]: 0;
		}

		res &= ct == s[i];
		for(; j < N && !res[j]; ++j);
	}

	for(i = 0; i < N && !res[i]; ++i);
	return i;
}

ircd::string_view
ircd::toupper(const mutable_buffer &out,
              const string_view &in)
noexcept
{
	const auto stop
	{
		std::next(begin(in), std::min(size(in), size(out)))
	};

	const auto end
	{
		std::transform(begin(in), stop, begin(out), []
		(auto&& c)
		{
			return std::toupper(c);
		})
	};

	assert(intptr_t(begin(out)) <= intptr_t(end));
	return string_view
	{
		data(out), size_t(std::distance(begin(out), end))
	};
}

#if defined(IRCD_SIMD) && defined(__SSE2__)
ircd::string_view
ircd::tolower(const mutable_buffer &out,
              const string_view &in)
noexcept
{
	const auto stop
	{
		std::next(begin(in), std::min(size(in), size(out)))
	};

	const u128x1 *src_
	{
		reinterpret_cast<const u128x1 *>(begin(in))
	};

	u128x1 *dst
	{
		reinterpret_cast<u128x1 *>(begin(out))
	};

	while(intptr_t(src_) < intptr_t(stop) - ssize_t(sizeof(u128x1)))
	{
		const u128x1 lit_A1      { _mm_set1_epi8('A' - 1)          };
		const u128x1 lit_Z1      { _mm_set1_epi8('Z' + 1)          };
		const u128x1 addend      { _mm_set1_epi8('a' - 'A')        };
		const u128x1 src         { _mm_loadu_si128(src_++)         };
		const u128x1 gte_A       { _mm_cmpgt_epi8(src, lit_A1)     };
		const u128x1 lte_Z       { _mm_cmplt_epi8(src, lit_Z1)     };
		const u128x1 mask        { _mm_and_si128(gte_A, lte_Z)     };
		const u128x1 ctrl_mask   { _mm_and_si128(mask, addend)     };
		const u128x1 result      { _mm_add_epi8(src, ctrl_mask)    };
		                            _mm_storeu_si128(dst++, result);
	}

	const auto end{std::transform
	(
		reinterpret_cast<const char *>(src_),
		stop,
		reinterpret_cast<char *>(dst),
		::tolower
	)};

	assert(intptr_t(begin(out)) <= intptr_t(end));
	return string_view{begin(out), end};
}
#else
ircd::string_view
ircd::tolower(const mutable_buffer &out,
              const string_view &in)
noexcept
{
	const auto stop
	{
		std::next(begin(in), std::min(size(in), size(out)))
	};

	const auto end
	{
		std::transform(begin(in), stop, begin(out), ::tolower)
	};

	assert(intptr_t(begin(out)) <= intptr_t(end));
	return string_view
	{
		data(out), size_t(std::distance(begin(out), end))
	};
}
#endif

std::string
ircd::replace(const string_view &s,
              const char &before,
              const string_view &after)
{
	const uint32_t occurs
	(
		std::count(begin(s), end(s), before)
	);

	const size_t size
	{
		occurs? s.size() + (occurs * after.size()):
		        s.size() - occurs
	};

	return string(size, [&s, &before, &after]
	(const mutable_buffer &buf)
	{
		char *p{begin(buf)};
		std::for_each(begin(s), end(s), [&before, &after, &p]
		(const char &c)
		{
			if(c == before)
			{
				memcpy(p, after.data(), after.size());
				p += after.size();
			}
			else *p++ = c;
		});

		return std::distance(begin(buf), p);
	});
}
