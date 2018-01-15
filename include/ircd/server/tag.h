// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_SERVER_TAG_H

/// Internal portion of the request
//
struct ircd::server::request::tag
{
	server::request *request;
	ctx::promise<http::code> p;
	size_t head_written {0};
	size_t content_written {0};
	size_t head_read {0};
	size_t content_read {0};

	mutable_buffer make_read_content_buffer() const;
	mutable_buffer make_read_head_buffer() const;

	const_buffer read_content(const const_buffer &, bool &done);
	const_buffer read_head(const const_buffer &, bool &done);

  public:
	const_buffer make_write_buffer() const;
	void wrote_buffer(const const_buffer &);

	mutable_buffer make_read_buffer() const;
	const_buffer read_buffer(const const_buffer &, bool &done);

	tag(server::request &);
	tag(tag &&) noexcept;
	tag(const tag &) = delete;
	tag &operator=(tag &&) noexcept;
	tag &operator=(const tag &) = delete;
	~tag() noexcept;
};
