// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_X86INTRIN_H

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

#if defined(HAVE_X86INTRIN_H) && defined(__SSE2__)
ircd::string_view
ircd::tolower(const mutable_buffer &out,
              const string_view &in)
noexcept
{
	const __m128i *src_
	{
		reinterpret_cast<const __m128i *>(begin(in))
	};

	__m128i *dst
	{
		reinterpret_cast<__m128i *>(begin(out))
	};

	const auto stop
	{
		std::next(begin(in), std::min(size(in), size(out)))
	};

	while(reinterpret_cast<const char *>(src_) + sizeof(__m128i)  < stop)
	{
		const __m128i lit_A1      { _mm_set1_epi8('A' - 1)          };
		const __m128i lit_Z1      { _mm_set1_epi8('Z' + 1)          };
		const __m128i addend      { _mm_set1_epi8('a' - 'A')        };
		const __m128i src         { _mm_loadu_si128(src_++)         };
		const __m128i gte_A       { _mm_cmpgt_epi8(src, lit_A1)     };
		const __m128i lte_Z       { _mm_cmplt_epi8(src, lit_Z1)     };
		const __m128i mask        { _mm_and_si128(gte_A, lte_Z)     };
		const __m128i ctrl_mask   { _mm_and_si128(mask, addend)     };
		const __m128i result      { _mm_add_epi8(src, ctrl_mask)    };

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
