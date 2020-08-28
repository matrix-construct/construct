// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifndef HAVE_IRCD_SPIRIT_H
#define HAVE_IRCD_SPIRIT_H

/// This file is not part of the IRCd standard include list (stdinc.h) because
/// it involves extremely expensive boost headers for creating formal spirit
/// grammars. Include this in a definition file which defines such grammars.
///
/// Note that directly sharing elements of a grammar between two compilation
/// units can be achieved with forward declarations in `ircd/grammar.h`.

// ircd.h is included here so that it can be compiled into this header. Then
// this becomes the single leading precompiled header.
#include <ircd/ircd.h>

// Disables asserts in spirit headers even when we're NDEBUG due to
// some false asserts around boolean character tests in spirit.
#define BOOST_DISABLE_ASSERTS

// This prevents spirit grammar rules from generating a very large and/or deep
// call-graph where rules compose together using wrapped indirect calls through
// boost::function -- this is higly inefficient as the grammar's logic ends up
// being a fraction of the generated code and the rest is invocation related
// overhead. By force-flattening here we can allow each entry-point into
// spirit to compose rules at once and eliminate the wrapping complex.
#pragma clang attribute push ([[gnu::always_inline]], apply_to = function)
#pragma clang attribute push ([[gnu::flatten]], apply_to = function)

#include <boost/config.hpp>
#include <boost/function.hpp>

#pragma GCC visibility push (internal)
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/iterator.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/repository/include/qi_seek.hpp>
#include <boost/spirit/repository/include/qi_subrule.hpp>
#pragma GCC visibility pop

#pragma clang attribute pop
#pragma clang attribute pop

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
	template<class parent_error> struct expectation_failure;

	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, generator_error);
	IRCD_EXCEPTION(generator_error, buffer_overrun);
}}

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	namespace phx = boost::phoenix;
	namespace fusion = boost::fusion;
	namespace spirit = boost::spirit;
	namespace ascii = spirit::ascii;
	namespace karma = spirit::karma;
	namespace qi = spirit::qi;

	using _val_type = phx::actor<spirit::attribute<0>>;
	using _r0_type = phx::actor<spirit::attribute<0>>;
	using _r1_type = phx::actor<spirit::attribute<1>>;
	using _r2_type = phx::actor<spirit::attribute<2>>;
	using _r3_type = phx::actor<spirit::attribute<3>>;

	using spirit::unused_type;
	using spirit::auto_;
	using spirit::_pass;
	using spirit::_val;

	using qi::locals;
	using qi::_a;
	using qi::_a_type;
	using qi::_r1_type;
	using qi::raw;
	using qi::omit;
	using qi::matches;
	using qi::hold;
	using qi::eoi;
	using qi::eps;
	using qi::expect;
	using qi::attr;
	using qi::attr_cast;
	using qi::repeat;
	using qi::lit;
	using qi::char_;
	using qi::byte_;
	using qi::string;
	using qi::short_;
	using qi::ushort_;
	using qi::word;
	using qi::big_word;
	using qi::little_word;
	using qi::int_;
	using qi::uint_;
	using qi::dword;
	using qi::big_dword;
	using qi::little_dword;
	using qi::long_;
	using qi::ulong_;
	using qi::qword;
	using qi::big_qword;
	using qi::little_qword;
	using qi::float_;
	using qi::bin_float;
	using qi::big_bin_float;
	using qi::little_bin_float;
	using qi::double_;
	using qi::bin_double;
	using qi::big_bin_double;
	using qi::little_bin_double;
	using spirit::repository::qi::seek;

	using karma::lit;
	using karma::char_;
	using karma::long_;
	using karma::double_;
	using karma::bool_;
	using karma::eps;
	using karma::attr_cast;
	using karma::maxwidth;
	using karma::buffer;
	using karma::skip;

	using prop_mask = mpl_::int_
	<
		karma::generator_properties::no_properties
		| karma::generator_properties::buffering
		| karma::generator_properties::counting
		| karma::generator_properties::tracking
		| karma::generator_properties::disabling
	>;

	using sink_type = karma::detail::output_iterator<char *, prop_mask, unused_type>;

	template<size_t idx,
	         class semantic_context>
	auto &
	attr_at(semantic_context&&);

	template<size_t idx,
	         class semantic_context>
	auto &
	local_at(semantic_context&&);
}}

namespace ircd::spirit::local
{
	using qi::_0;
	using qi::_1;
	using qi::_2;
	using qi::_3;
}

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
	// parse.cc
	extern thread_local char rule_buffer[64];
	extern thread_local char generator_buffer[8][64_KiB];
	extern thread_local struct generator_state *generator_state;
}}

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	struct substring_view;
	struct custom_parser;
	BOOST_SPIRIT_TERMINAL(custom);

	template<class gen,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, gen&&, attr&&...);

	template<class parent_error,
	         size_t error_show_max  = 48,
	         class gen,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, gen&&, attr&&...);

	template<bool truncation = false,
	         class gen,
	         class... attr>
	bool generate(mutable_buffer &out, gen&&, attr&&...);
}}

namespace ircd
{
	using spirit::generate;
	using spirit::parse;
}

namespace boost {
namespace spirit
__attribute__((visibility("internal")))
{
	namespace qi
	{
		template<class modifiers>
		struct make_primitive<ircd::spirit::tag::custom, modifiers>;
	}

	template<>
	struct use_terminal<qi::domain, ircd::spirit::tag::custom>
	:mpl::true_
	{};
}}

struct [[gnu::visibility("internal")]]
ircd::spirit::custom_parser
:qi::primitive_parser<custom_parser>
{
	template<class context,
	         class iterator>
	struct attribute
	{
		using type = iterator;
	};

	template<class context>
	boost::spirit::info what(context &) const
	{
		return boost::spirit::info("custom");
	}

	template<class iterator,
	         class context,
	         class skipper,
	         class attr>
	bool parse(iterator &, const iterator &, context &, const skipper &, attr &) const;
};

template<class modifiers>
struct [[gnu::visibility("internal")]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom, modifiers>
{
	using result_type = ircd::spirit::custom_parser;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

struct ircd::spirit::substring_view
:ircd::string_view
{
	using _iterator = boost::spirit::karma::detail::indirect_iterator<const char *>;
	using _iterator_range = boost::iterator_range<_iterator>;

	using ircd::string_view::string_view;
	explicit substring_view(const _iterator_range &range)
	:ircd::string_view
	{
		std::addressof(*range.begin()), std::addressof(*range.end())
	}
	{}
};

template<class parent_error>
struct __attribute__((visibility("default")))
ircd::spirit::expectation_failure
:parent_error
{
	template<class it = const char *>
	expectation_failure(const qi::expectation_failure<it> &e,
	                    const ssize_t &show_max = 64);

	template<class it = const char *>
	expectation_failure(const qi::expectation_failure<it> &e,
	                    const it &start,
	                    const ssize_t &show_max = 64);
};

template<class parent>
template<class it>
ircd::spirit::expectation_failure<parent>::expectation_failure(const qi::expectation_failure<it> &e,
		                                                       const ssize_t &show_max)
:parent
{
	"Expected %s. You input %zd invalid characters :%s",
	ircd::string(rule_buffer, e.what_),
	std::distance(e.first, e.last),
	string_view{e.first, e.first + std::min(std::distance(e.first, e.last), show_max)}
}
{}

template<class parent>
template<class it>
ircd::spirit::expectation_failure<parent>::expectation_failure(const qi::expectation_failure<it> &e,
                                                               const it &start,
		                                                       const ssize_t &show_max)
:parent
{
	"Expected %s. You input %zd invalid characters somewhere between position %zd and %zd :%s",
	ircd::string(rule_buffer, e.what_),
	std::distance(e.first, e.last),
	std::distance(start, e.first),
	std::distance(start, e.last),
	string_view{e.first, e.first + std::min(std::distance(e.first, e.last), show_max)}
}
{}

struct [[gnu::visibility("hidden")]]
ircd::spirit::generator_state
{
	/// The number of instances stacked behind the current state
	static uint depth() noexcept;

	/// User's buffer.
	mutable_buffer &out;

	/// Previous in buffer stack
	struct generator_state *prev {nullptr};

	/// The number of characters we're storing in buffer
	uint consumed {0};

	/// The number of characters attempted, which may be larger than
	/// the buffer capacity indicating an overflow amount.
	uint generated {0};
};

inline uint
ircd::spirit::generator_state::depth()
noexcept
{
	uint ret(0);
	for(auto p(ircd::spirit::generator_state); p; p = p->prev)
		++ret;

	return ret;
}

template<bool truncation,
         class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline]]
extern inline bool
ircd::spirit::generate(mutable_buffer &out,
                       gen&& g,
                       attr&&... a)

{
	const auto max(size(out));
	const auto start(data(out));
	struct generator_state state
	{
		out
	};

	const scope_restore _state
	{
		generator_state, &state
	};

	sink_type sink
	{
		begin(out)
	};

	const auto ret
	{
		karma::generate(sink, std::forward<gen>(g), std::forward<attr>(a)...)
	};

	assert(state.generated >= state.consumed);
	const auto overflow
	{
		state.generated - state.consumed
	};

	if constexpr(truncation)
	{
		begin(out) = begin(out) > end(out)? end(out): begin(out);
		assert(begin(out) <= end(out));
		return ret;
	}

	if(unlikely(overflow || begin(out) > end(out)))
	{
		begin(out) = start;
		assert(begin(out) <= end(out));

		char pbuf[2][48];
		throw buffer_overrun
		{
			"Insufficient buffer of %s; required at least %s",
			pretty(pbuf[0], iec(state.consumed)),
			pretty(pbuf[1], iec(state.generated)),
		};
	}

	assert(begin(out) >= end(out) - max);
	assert(begin(out) <= end(out));
	return ret;
}

template<class parent_error,
         size_t error_show_max,
         class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    gen&& g,
                    attr&&... a)
try
{
	return qi::parse(start, stop, std::forward<gen>(g), std::forward<attr>(a)...);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parent_error>
	{
		e, start, error_show_max
	};
}

template<class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    gen&& g,
                    attr&&... a)
{
	return qi::parse(start, stop, std::forward<gen>(g), std::forward<attr>(a)...);
}

template<size_t idx,
         class semantic_context>
inline auto &
ircd::spirit::local_at(semantic_context&& c)
{
	return boost::fusion::at_c<idx>(c.locals);
}

template<size_t idx,
         class semantic_context>
inline auto &
ircd::spirit::attr_at(semantic_context&& c)
{
	return boost::fusion::at_c<idx>(c.attributes);
}

//
// boost::spirit::karma
//

namespace boost::spirit::karma::detail
{
	template<> struct enable_buffering<ircd::spirit::sink_type>;
}

template<>
struct [[gnu::visibility("internal")]]
boost::spirit::karma::detail::enable_buffering<ircd::spirit::sink_type>
{
	uint width
	{
		0
	};

	uint depth
	{
		ircd::spirit::generator_state::depth()
	};

	ircd::mutable_buffer buffer
	{
		depth?
			ircd::spirit::generator_buffer[depth - 1]:
			data(ircd::spirit::generator_state->out),

		depth?
			std::min(width, uint(sizeof(ircd::spirit::generator_buffer[depth - 1]))):
			size(ircd::spirit::generator_state->out),
	};

	struct ircd::spirit::generator_state state
	{
		buffer, ircd::spirit::generator_state
	};

	ircd::scope_restore<struct ircd::spirit::generator_state *> stack_state
	{
		ircd::spirit::generator_state, std::addressof(state)
	};

	std::size_t buffer_size() const
	{
		return state.generated;
	}

	void disable()
	{
		const auto width
		{
			this->width != -1U? this->width: state.consumed
		};

		assert(width >= state.consumed);
		const uint off
		{
			width - state.consumed
		};

		const ircd::const_buffer src
		{
			state.out, state.consumed
		};

		const ircd::mutable_buffer dst
		{
			state.out + off
		};

		const auto moved
		{
			ircd::move(dst, src)
		};

		assert(moved == state.consumed);
		state.consumed = 0;
	}

	bool buffer_copy(std::size_t maxwidth = std::size_t(-1))
	{
		assert(state.prev);
		#if __has_builtin(__builtin_assume)
			__builtin_assume(state.prev != nullptr);
		#endif

		auto &prev
		{
			*state.prev
		};

		const bool prev_base
		{
			prev.prev == nullptr
		};

		const auto width
		{
			this->width != -1U? this->width: state.consumed
		};

		const ircd::const_buffer src
		{
			state.out, std::max(state.consumed, width)
		};

		const ircd::mutable_buffer dst
		{
			data(prev.out) - (prev_base? state.consumed: -prev.consumed),
			prev_base? state.consumed: (size(prev.out) - prev.consumed)
		};

		const auto copied
		{
			ircd::copy(dst, src)
		};

		prev.generated += state.generated;
		prev.consumed += copied;
		return true; // sink.good();
	}

	template<typename OutputIterator>
	bool buffer_copy_to(OutputIterator& sink, std::size_t maxwidth = std::size_t(-1)) const
	{
		ircd::always_assert(false);
		return true;
	}

	template <typename RestIterator>
	bool buffer_copy_rest(RestIterator& sink, std::size_t start_at = 0) const
	{
		ircd::always_assert(false);
		return true;
	}

	enable_buffering(ircd::spirit::sink_type &sink,
	                 std::size_t width = std::size_t(-1))
	:width
	{
		uint(width)
	}
	{
		assert(size(buffer) != 0);
		assert(this->depth <= 8);
	}

	~enable_buffering() noexcept
	{
		assert(ircd::spirit::generator_state == &state);
		//disable();
	}
};

template<>
inline bool
boost::spirit::karma::detail::buffering_policy::output(const char &value)
{
	assert(ircd::spirit::generator_state);
	#if __has_builtin(__builtin_assume)
		__builtin_assume(ircd::spirit::generator_state != nullptr);
	#endif

	auto &state
	{
		*ircd::spirit::generator_state
	};

	const bool buffering
	{
		this->buffer != nullptr
	};

	const bool base
	{
		state.prev == nullptr
	};

	const uint off
	{
		ircd::boolmask<uint>(!base | buffering) & state.consumed
	};

	const auto dst
	{
		state.out + off
	};

	const auto copied
	{
		ircd::copy(dst, value)
	};

	state.consumed += copied;
	state.generated += sizeof(char);
	return !buffering;
}

template<>
inline void
boost::spirit::karma::detail::position_policy::output(const char &value)
{
}

#if 0
template<>
template<>
inline void
boost::spirit::karma::detail::counting_policy<ircd::spirit::sink_type>::output(const char &value)
{
}
#endif

template<>
inline bool
ircd::spirit::sink_type::good()
const
{
	return true;
}

#endif // HAVE_IRCD_SPIRIT_H
