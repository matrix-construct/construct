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
#define HAVE_IRCD_GPT_PIPE_PROF_H

namespace ircd::gpt::pipe
{
	string_view debug(const mutable_buffer &, const prof &, const size_t &pos);
	string_view debug(const mutable_buffer &, const prof &);
}

/// Extract profiling information for a cycle. This object contains timing
/// state integers for each corresponding stage of the cycle.
///
/// Default constructions initialize to zero and the state can also be used
/// as an accumulator.
struct ircd::gpt::pipe::prof
{
	static constexpr size_t stages
	{
		cycle::stages
	};

	static constexpr size_t phases
	{
		num_of<cl::work::prof::phase>()
	};

	using phase = cl::work::prof::phase;
	using phase_array = cl::work::prof;
	using stage_array = std::array<phase_array, stages>;
	using info_type = std::tuple<string_view, int>;
	using info_array = std::array<info_type, stages>;
	using info_name_array = std::array<char[64], stages>;

	static info_name_array name;
	static info_array info;

  private:
	static bool init;
	static void init_info(const pipe::cycle &);

  public:
	stage_array ts;

	prof(const pipe::cycle &);
	prof() noexcept;
};
