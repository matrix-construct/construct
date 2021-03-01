// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#define HAVE_IRCD_SPIRIT_GENERATE_H

/// This file is not part of the IRCd standard include list (stdinc.h) because
/// it involves extremely expensive boost headers for creating formal spirit
/// grammars. This file is automatically included in the spirit.h group.

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
	IRCD_EXCEPTION(error, generator_error);
	IRCD_EXCEPTION(generator_error, buffer_overrun);

	constexpr size_t
	generator_buffer_size {64_KiB},
	generator_buffer_count {8};

	extern thread_local struct generator_state *
	generator_state;

	extern thread_local char
	generator_buffer[generator_buffer_count][generator_buffer_size];
}}

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	using prop_mask = mpl_::int_
	<
		karma::generator_properties::no_properties
		| karma::generator_properties::buffering
		| karma::generator_properties::counting
		| karma::generator_properties::tracking
		| karma::generator_properties::disabling
	>;

	using sink_type = karma::detail::output_iterator
	<
		char *, prop_mask, unused_type
	>;

	template<bool truncation = false,
	         class gen,
	         class... attr>
	bool generate(mutable_buffer &out, gen&&, attr&&...);
}}

/// This structure is a shadow for the default spirit::karma buffering
/// stack. They conduct buffering by allocating and copying standard strings
/// to implement certain karma directives like "right_align[]" etc. Instead
/// we reimplement optimized buffering with assumptions specific to our
/// application's tolerances. We assume a maximum size of a buffer and maximum
/// height of any stack growing from an ircd::spirit::generate() call without
/// need for reentrancy. This gives us the ability to pre-allocate thread_local
/// buffers.
struct [[gnu::visibility("internal")]]
ircd::spirit::generator_state
{
	/// The number of instances stacked behind the current state. This should
	/// never be greater than the generator_buffer_count
	static uint depth() noexcept;

	/// Destination buffer for this level of the stack. For depth=0 this will
	/// be the user's buffer, otherwise it will be the thread-local generator
	/// buffer at depth - 1.
	///
	/// N.B. The `begin()` of this buffer will be advanced by spirit when the
	/// depth=0 as part of the output iterator process. It is never touched when
	/// depth>0 because it directly refers to one of the static buffers.
	mutable_buffer &out;

	/// Previous in buffer stack. This is null for the instance created in the
	/// frame for ircd::spirit::generate(), which is the base of the stack.
	struct generator_state *prev {nullptr};

	/// The number of characters we're storing in buffer at `out`. When
	/// `depth>0` this will be the number of characters stored in one of the
	/// static buffers between `begin(out), begin(out)+consumed` and safely
	/// saturates at the maximum size of the buffer. When `depth=0` this
	/// indicates the number of characters *behind* `begin(out)` it still
	/// safely saturates at the maximum size of the user's buffer.
	uint consumed {0};

	/// The number of characters attempted, which may be larger than the buffer
	/// capacity and value stored in `consumed`. Thus the difference between
	/// generated and consumed is an overflow value which estimates the number
	/// of characters which would've been required for the generation.
	uint generated {0};
};

template<bool truncation,
         class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline]]
extern inline bool
ircd::spirit::generate(mutable_buffer &out,
                       gen&& g,
                       attr&&... a)

{
	// Save the user buffer as originally provided in case we need to restore it.
	const mutable_buffer user
	{
		out
	};

	// Base instance for the buffering stack (see `struct generator_state`).
	struct generator_state state
	{
		out
	};

	// Set the thread-local generator_state pointer to our stack instance. This
	// will cooperatively restore whatever value was previously there, though
	// it should be null.
	assert(!generator_state);
	const scope_restore _state
	{
		generator_state, &state
	};

	sink_type sink
	{
		begin(out)
	};

	// Enter into `spirit::karma` to execute the generator
	const auto ret
	{
		karma::generate(sink, std::forward<gen>(g), std::forward<attr>(a)...)
	};

	// Determine if the user's buffer was enough to store the result; if not
	// there will be an overflow value.
	assert(state.generated >= state.consumed);
	const auto overflow
	{
		state.generated - state.consumed
	};

	// Compile-time branch for generators that tolerate truncation (i.e. fmt::)
	if constexpr(truncation)
	{
		begin(out) = begin(out) > end(out)? end(out): begin(out);
		assert(begin(out) <= end(out));
		return ret;
	}

	// Branch to throw an exception for generators that do not tolerate
	// truncated results (i.e. json::).
	if(unlikely(overflow || begin(out) > end(out)))
	{
		begin(out) = begin(user);
		assert(begin(out) <= end(out));

		char pbuf[2][48];
		throw buffer_overrun
		{
			"Insufficient buffer of %s; required at least %s",
			pretty(pbuf[0], iec(state.consumed)),
			pretty(pbuf[1], iec(state.generated)),
		};
	}

	assert(begin(out) >= end(out) - size(user));
	assert(begin(out) <= end(out));
	return ret;
}

inline uint
ircd::spirit::generator_state::depth()
noexcept
{
	uint ret(0);
	for(auto p(ircd::spirit::generator_state); p; p = p->prev)
		++ret;

	return ret;
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
			sizeof(ircd::spirit::generator_buffer[depth - 1]):
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

	template<typename OutputIterator>
	bool buffer_copy_to(OutputIterator& sink, std::size_t maxwidth = std::size_t(-1)) const = delete;

	template<typename RestIterator>
	bool buffer_copy_rest(RestIterator& sink, std::size_t start_at = 0) const = delete;

	bool buffer_copy(std::size_t maxwidth = std::size_t(-1));

	void disable();

	enable_buffering(ircd::spirit::sink_type &sink, std::size_t width = std::size_t(-1));
	~enable_buffering() noexcept;
};

inline
boost::spirit::karma::detail::enable_buffering<ircd::spirit::sink_type>::enable_buffering(ircd::spirit::sink_type &sink,
                                                                                          std::size_t width)
:width
{
	uint(width)
}
{
	assert(size(buffer) != 0);
	assert(this->depth <= 8);
}

inline
boost::spirit::karma::detail::enable_buffering<ircd::spirit::sink_type>::~enable_buffering()
noexcept
{
	assert(ircd::spirit::generator_state == &state);
	//disable();
}

inline void
boost::spirit::karma::detail::enable_buffering<ircd::spirit::sink_type>::disable()
{
	const auto width
	{
		this->width != -1U? this->width: state.consumed
	};

	const uint off
	{
		width > state.consumed? width - state.consumed: 0U
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
	state.consumed = !off? state.consumed: 0U;
}

inline bool
boost::spirit::karma::detail::enable_buffering<ircd::spirit::sink_type>::buffer_copy(std::size_t maxwidth)
{
	assert(state.prev);
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
		this->width != -1U?
			this->width:
			state.consumed
	};

	const auto dst_disp
	{
		prev_base?
			-ssize_t(state.consumed):
			ssize_t(prev.consumed)
	};

	const auto dst_over
	{
		std::begin(prev.out) > std::end(prev.out)?
			std::distance(std::end(prev.out), std::begin(prev.out)):
			0L
	};

	const auto dst_size
	{
		prev_base?
			std::max(state.consumed - dst_over, 0L):
			size(prev.out) - prev.consumed
	};

	const ircd::mutable_buffer dst
	{
		data(prev.out) + dst_disp, dst_size
	};

	const ircd::const_buffer src
	{
		state.out, std::max(state.consumed, width)
	};

	const auto copied
	{
		ircd::copy(dst, src)
	};

	prev.generated += state.generated;
	prev.consumed += copied;
	return true; // sink.good();
}

template<>
inline bool
boost::spirit::karma::detail::buffering_policy::output(const char &value)
{
	assert(ircd::spirit::generator_state);
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
	assert(count == nullptr);
}
#endif

template<>
template<>
inline bool
boost::spirit::karma::detail::disabling_output_iterator
<
	boost::spirit::karma::detail::buffering_policy,
	boost::spirit::karma::detail::counting_policy<ircd::spirit::sink_type>,
	boost::spirit::karma::detail::position_policy
>
::output(const char &value)
{
	assert(do_output);
	this->counting_policy::output(value);
	this->tracking_policy::output(value);
	return this->buffering_policy::output(value);
}

template<>
template<>
inline void
ircd::spirit::sink_type::operator=(const char &value)
{
	this->base_iterator::output(value);
}

template<>
inline ircd::spirit::sink_type &
ircd::spirit::sink_type::operator++()
{
	const bool has_buffer
	{
		this->base_iterator::has_buffer()
	};

	(*sink) += !has_buffer;
	return *this;
}

template<>
inline ircd::spirit::sink_type
ircd::spirit::sink_type::operator++(int)
{
	auto copy(*this);
	++(*this);
	return copy;
}

template<>
inline bool
ircd::spirit::sink_type::good()
const
{
	return true;
}
