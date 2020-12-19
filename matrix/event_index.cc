// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::idx
ircd::m::index(const event &event)
try
{
	return index(event.event_id);
}
catch(const json::not_found &)
{
	throw m::NOT_FOUND
	{
		"Cannot find index for event without an event_id."
	};
}

ircd::m::event::idx
ircd::m::index(std::nothrow_t,
               const event &event)
{
	return index(std::nothrow, event.event_id);
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	assert(event_id);
	const auto ret
	{
		index(std::nothrow, event_id)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"no index found for %s",
			string_view{event_id}

		};

	return ret;
}

ircd::m::event::idx
ircd::m::index(std::nothrow_t,
               const event::id &event_id)
{
	static auto &column
	{
		dbs::event_idx
	};

	bool found {false};
	event::idx ret {0};
	if(likely(event_id))
	{
		const mutable_buffer buf
		{
			reinterpret_cast<char *>(&ret), sizeof(ret)
		};

		const string_view val
		{
			read(column, event_id, found, buf)
		};
	}

	return ret & boolmask<event::idx>(found);
}

bool
ircd::m::index(std::nothrow_t,
               const event::id &event_id,
               const event::closure_idx &closure)
{
	auto &column
	{
		dbs::event_idx
	};

	return event_id && column(event_id, std::nothrow, [&closure]
	(const string_view &value)
	{
		const event::idx event_idx
		{
			byte_view<event::idx>(value)
		};

		closure(event_idx);
	});
}

size_t
ircd::m::index(const vector_view<event::idx> &out,
               const event::auth &auth)
{
	event::id ids[event::auth::MAX];
	const auto &event_ids
	{
		auth.ids(ids)
	};

	const auto &found
	{
		index(out, event_ids)
	};

	assert(found <= event_ids.size());
	return event_ids.size();
}

size_t
ircd::m::index(const vector_view<event::idx> &out,
               const event::prev &prev)
{
	event::id ids[event::prev::MAX];
	const auto &event_ids
	{
		prev.ids(ids)
	};

	const auto &found
	{
		index(out, event_ids)
	};

	assert(found <= event_ids.size());
	return event_ids.size();
}

size_t
ircd::m::index(const vector_view<event::idx> &out_,
               const vector_view<const event::id> &in_)
{
	static const size_t batch_max
	{
		64
	};

	const size_t max
	{
		std::min(out_.size(), in_.size())
	};

	auto &column
	{
		dbs::event_idx
	};

	size_t ret(0);
	for(size_t i(0); i < max; i += batch_max)
	{
		const size_t num
		{
			i + batch_max > max? max - i: batch_max
		};

		const vector_view<event::idx> out
		(
			out_.data() + i, num
		);

		const vector_view<const event::id> in
		(
			in_.data() + i, num
		);

		const vector_view<const string_view> keys
		(
			static_cast<const string_view *>(in.data()), in.size()
		);

		mutable_buffer buf[num];
		for(size_t j(0); j < num; ++j)
		{
			buf[j] = mutable_buffer
			{
				reinterpret_cast<char *>(out.data() + j),
				sizeof(out[j])
			};

			zero(buf[j]);
		}

		const vector_view<mutable_buffer> bufs
		(
			buf, num
		);

		const uint64_t found_mask
		{
			db::read(column, keys, bufs)
		};

		ret += __builtin_popcountl(found_mask);
	}

	return ret;
}
