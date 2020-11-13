// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
ircd::indexof(const string_view &in,
              const string_views &tab)
noexcept
{
	const auto it
	{
		std::find(tab.begin(), tab.end(), in)
	};

	const auto ret
	{
		std::distance(begin(tab), it)
	};

	assert(size_t(ret) <= tab.size());
	return ret;
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

#if defined(__SSE2__)
ircd::string_view
ircd::tolower(const mutable_buffer &out,
              const string_view &in)
noexcept
{
	const auto stop
	{
		std::next(begin(in), std::min(size(in), size(out)))
	};

	auto *src_
	{
		reinterpret_cast<const __m128i_u *>(begin(in))
	};

	auto *dst
	{
		reinterpret_cast<__m128i_u *>(begin(out))
	};

	while(intptr_t(src_) < intptr_t(stop) - ssize_t(sizeof(m128u)))
	{
		const auto lit_A1      { _mm_set1_epi8('A' - 1)          };
		const auto lit_Z1      { _mm_set1_epi8('Z' + 1)          };
		const auto addend      { _mm_set1_epi8('a' - 'A')        };
		const auto src         { _mm_loadu_si128(src_++)         };
		const auto gte_A       { _mm_cmpgt_epi8(src, lit_A1)     };
		const auto lte_Z       { _mm_cmplt_epi8(src, lit_Z1)     };
		const auto mask        { _mm_and_si128(gte_A, lte_Z)     };
		const auto ctrl_mask   { _mm_and_si128(mask, addend)     };
		const auto result      { _mm_add_epi8(src, ctrl_mask)    };
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

ircd::string_view
ircd::replace(const mutable_buffer &out,
              const string_view &in,
              const char &before,
              const char &after)
noexcept
{
	const size_t cpsz
	{
		std::min(size(in), size(out))
	};

	// Branch for in-place copy if in and out are the same. Note that
	// if this branch is not taken they cannot overlap in any way.
	if(data(in) == data(out))
		return replace(mutable_buffer(data(out), cpsz), before, after);

	assert(!overlap(out, in));
	char *const __restrict__ outp
	{
		begin(out)
	};

	const char *const __restrict__ inp
	{
		begin(in)
	};

	for(size_t i(0); i < cpsz; ++i)
		outp[i] = inp[i] != before? inp[i]: after;

	return string_view
	{
		begin(out), cpsz
	};
}

ircd::string_view
ircd::replace(const mutable_buffer &s,
              const char &before,
              const char &after)
noexcept
{
	char *const __restrict__ p
	{
		begin(s)
	};

	for(size_t i(0); i < size(s); ++i)
		p[i] = p[i] != before? p[i]: after;

	return string_view
	{
		data(s), size(s)
	};
}
