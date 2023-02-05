// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ALLOCATOR_STATE_H

namespace ircd::allocator
{
	struct state;
}

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

  public:
	bool available(const size_t &n = 1) const;
	void deallocate(const uint &p, const size_t &n);
	uint allocate(std::nothrow_t, const size_t &n, const uint &hint = -1);
	uint allocate(const size_t &n, const uint &hint = -1);

	state(const size_t &size = 0,
	      word_t *const &avail = nullptr)
	:size{size}
	,avail{avail}
	,last{0}
	{}
};
