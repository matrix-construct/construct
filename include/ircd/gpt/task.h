// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_TASK_H

#ifdef __cplusplus
namespace ircd::gpt
{
	void seed(task &, const uint64_t &) noexcept;
	void seed(task &) noexcept;
	void clear(task &) noexcept;
	void reset(task &) noexcept;
}

/// Task Context
///
/// State for a task.
struct ircd::gpt::task
{
	enum status :char;

	/// Reference to the attached options.
	const gpt::opts *opts {nullptr};

	/// Reference to user's control block.
	gpt::ctrl *ctrl {nullptr};

	/// Pipe code
	std::shared_ptr<pipe::code> code;

	/// Pipe model
	std::unique_ptr<pipe::model> model;

	/// Pipe state
	pipe::desc desc;

  public:
	bool done() const noexcept;

	bool
	operator()();

	vector_view<u16>
	operator()(const vector_view<u16> &out,
	           const vector_view<const u16> &in);

	string_view
	operator()(const mutable_buffer &out,
	           const string_view &in);

	task(const gpt::opts *     = nullptr,
	     gpt::ctrl *           = nullptr);

	~task() noexcept;
};
#endif
