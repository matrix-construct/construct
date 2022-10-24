// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_STEP_H

/// Perform one task step on the device. The step is a sequence of cycles
/// which generate tokens until satisfying a halting condition. The number of
/// cycles for the step is limited by the size of the context buffer.
///
struct ircd::gpt::step
{
	gpt::epoch &epoch;
	pipe::desc &desc;

	const gpt::opts &opts;
	gpt::ctrl &ctrl;

	const uint id;
	const uint start;

	pipe::prof profile;

	void profile_accumulate(const pipe::prof &);

  public:
	bool done() const noexcept;
	bool operator()();

	step(gpt::epoch &);
	~step() noexcept;
};
