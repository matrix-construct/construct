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
#define HAVE_IRCD_GPT_EPOCH_H

/// Perform one task epoch on the device.
///
struct ircd::gpt::epoch
{
	gpt::task &task;
	pipe::desc &desc;

	const gpt::opts &opts;
	gpt::ctrl &ctrl;

	const uint id;
	const size_t start, stop;
	f32 *moment[2];

	pipe::prof profile;

	void profile_accumulate(const pipe::prof &);

  public:
	bool done() const noexcept;
	bool operator()();

	epoch(gpt::task &);
	~epoch() noexcept;
};
