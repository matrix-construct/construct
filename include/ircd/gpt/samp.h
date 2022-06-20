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
#define HAVE_IRCD_GPT_SAMP_H

/// Perform one task step on the device. The step is a sequence of cycles
/// which generate tokens until satisfying a halting condition. The number of
/// cycles for the step is limited by the size of the context buffer.
///
struct ircd::gpt::samp
{
	gpt::step &step;
	pipe::desc &desc;

	const gpt::opts &opts;
	gpt::ctrl &ctrl;

	const uint id;

	int accept;
	uint dispatch;
	uint cycle;
	uint tokens;
	uint count;

	pipe::prof profile;
	std::deque<pipe::cycle> queue;

  public:
	void profile_accumulate(const pipe::prof &);
	bool retire(pipe::cycle &, const gpt::ctrl &);
	bool evaluate(pipe::cycle &);
	uint tokenize();

  public:
	bool done() const noexcept;
	bool operator()();

	samp(gpt::step &);
	~samp() noexcept;
};
