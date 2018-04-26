// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SERVER_TAG_H

namespace ircd::server
{
	struct tag;

	void associate(request &, tag &);
	void associate(request &, tag &, tag &&) noexcept;
	void associate(request &, tag &, request &&) noexcept;
	void disassociate(request &, tag &);
	void cancel(request &, tag &) noexcept;
}

/// Internal portion of the request
///
struct ircd::server::tag
{
	struct state
	{
		size_t written {0};
		size_t head_read {0};          // includes head terminator
		size_t head_rem {0};           // how much of head buf wasn't used.
		size_t content_read {0};       // total content read after head
		size_t content_length {0};     // fixed; or grows monotonic for chunked enc
		size_t chunk_read {0};         // content read after last chunk head
		size_t chunk_length {0};       // -1 for chunk header mode
		http::code status {(http::code)0};
	}
	state;
	ctx::promise<http::code> p;
	server::request *request {nullptr};
	std::unique_ptr<char[]> cancellation;

	void set_exception(std::exception_ptr);
	template<class... args> void set_exception(args&&...);
	template<class... args> void set_value(args&&...);

	const_buffer make_write_content_buffer() const;
	const_buffer make_write_head_buffer() const;

	size_t content_overflow() const;
	size_t content_remaining() const;

	mutable_buffer make_read_discard_buffer() const;
	mutable_buffer make_read_chunk_dynamic_content_buffer() const;
	mutable_buffer make_read_chunk_dynamic_head_buffer() const;
	mutable_buffer make_read_chunk_content_buffer() const;
	mutable_buffer make_read_chunk_head_buffer() const;
	mutable_buffer make_read_content_buffer() const;
	mutable_buffer make_read_head_buffer() const;

	const_buffer read_chunk_dynamic_content(const const_buffer &, bool &done);
	const_buffer read_chunk_dynamic_head(const const_buffer &, bool &done, const uint8_t = 0);
	const_buffer read_chunk_content(const const_buffer &, bool &done);
	const_buffer read_chunk_head(const const_buffer &, bool &done, const uint8_t = 0);
	const_buffer read_content(const const_buffer &, bool &done);
	const_buffer read_head(const const_buffer &, bool &done, link &);

  public:
	size_t write_size() const;
	size_t write_completed() const;
	size_t write_remaining() const;

	size_t read_size() const;                // not accurate until content-length
	size_t read_completed() const;           // reports all received so far
	size_t read_remaining() const;           // not accurate until content-length

	bool committed() const;                  // Tag has revealed data to remote
	bool abandoned() const;                  // User has abandoned their future
	bool canceled() const;                   // User has abandoned their *request

	const_buffer make_write_buffer() const;
	void wrote_buffer(const const_buffer &);

	mutable_buffer make_read_buffer() const;
	const_buffer read_buffer(const const_buffer &, bool &done, link &);

	tag() = default;
	tag(server::request &);
	tag(tag &&) noexcept;
	tag(const tag &) = delete;
	tag &operator=(tag &&) noexcept;
	tag &operator=(const tag &) = delete;
	~tag() noexcept;
};

inline
ircd::server::tag::tag(server::request &request)
{
	associate(request, *this);
}

inline
ircd::server::tag::tag(tag &&o)
noexcept
:state{std::move(o.state)}
,p{std::move(o.p)}
,request{std::move(o.request)}
,cancellation{std::move(o.cancellation)}
{
	if(request)
		associate(*request, *this, std::move(o));

	assert(!o.request);
}

inline ircd::server::tag &
ircd::server::tag::operator=(tag &&o)
noexcept
{
	this->~tag();

	state = std::move(o.state);
	p = std::move(o.p);
	request = std::move(o.request);
	cancellation = std::move(o.cancellation);

	if(request)
		associate(*request, *this, std::move(o));

	assert(!o.request);
	return *this;
}

inline
ircd::server::tag::~tag()
noexcept
{
	if(request)
		disassociate(*request, *this);

	assert(!request);
}
