// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_INSTRUCTIONS_H

namespace ircd::prof
{
	struct instructions;
}

/// Gadget for hardware profiling of instructions for a scope.
///
struct ircd::prof::instructions
{
	prof::group group;
	uint64_t retired {0};

  public:
	const uint64_t &at() const;
	const uint64_t &sample();

	instructions();
	instructions(instructions &&) = delete;
	instructions(const instructions &) = delete;
	~instructions() noexcept;
};
