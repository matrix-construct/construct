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
              const char before,
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
		return replace(buf, s, before, after);
	});
}

ircd::string_view
ircd::replace(const mutable_buffer &out_,
              const string_view &in_,
              const string_view &before,
              const string_view &after)
{
	string_view in(in_);
	mutable_buffer out(out_);
	size_t produced(0), consumed(0);
	size_t p(in.find(before)); do
	{
		const string_view prologue
		{
			in.substr(0, p)
		};

		const auto prologue_copied
		{
			move(out, prologue)
		};

		consumed += consume(out, prologue_copied);
		produced += size(prologue);
		if(p != in.npos)
		{
			const auto after_copied
			{
				move(out, after)
			};

			consumed += consume(out, after_copied);
			produced += size(after);
			in = in.substr(p + size(after));
			p = in.find(before);
		}
		else break;
	}
	while(1);

	if(unlikely(produced > consumed))
		throw std::out_of_range
		{
			"Insufficient buffer to replace string with string"
		};

    return string_view
    {
		data(out_), data(out)
    };
}

ircd::string_view
ircd::replace(const mutable_buffer &out_,
              const string_view &in_,
              const char before,
              const string_view &after)
{
	string_view in(in_);
	mutable_buffer out(out_);
	size_t produced(0), consumed(0);
	size_t p(in.find(before)); do
	{
		const string_view prologue
		{
			in.substr(0, p)
		};

		const auto prologue_copied
		{
			move(out, prologue)
		};

		consumed += consume(out, prologue_copied);
		produced += size(prologue);
		if(p != in.npos)
		{
			const auto after_copied
			{
				move(out, after)
			};

			consumed += consume(out, after_copied);
			produced += size(after);
			in = in.substr(p + 1);
			p = in.find(before);
		}
		else break;
	}
	while(1);

	if(unlikely(produced > consumed))
		throw std::out_of_range
		{
			"Insufficient buffer to replace character with string"
		};

    return string_view
    {
		data(out_), data(out)
    };
}

ircd::string_view
ircd::replace(const mutable_buffer &out,
              const string_view &in,
              const char before,
              const char after)
noexcept
{
	char *const __restrict__ outp
	{
		begin(out)
	};

	const char *const __restrict__ inp
	{
		begin(in)
	};

	const size_t cpsz
	{
		std::min(size(out), size(in))
	};

	std::transform(inp, inp + cpsz, outp, [&before, &after]
	(const char p) -> char
	{
		return 0
		| (boolmask<char>(p == before) & after)
		| (boolmask<char>(p != before) & p)
		;
	});

	return string_view
	{
		begin(out), cpsz
	};
}

ircd::string_view
ircd::replace(const mutable_buffer &s,
              const char before,
              const char after)
noexcept
{
	char *const __restrict__ p
	{
		begin(s)
	};

	std::transform(p, p + size(s), p, [&before, &after]
	(const char p) -> char
	{
		return 0
		| (boolmask<char>(p == before) & after)
		| (boolmask<char>(p != before) & p)
		;
	});

	return string_view
	{
		data(s), size(s)
	};
}
