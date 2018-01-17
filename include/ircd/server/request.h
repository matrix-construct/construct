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
#define HAVE_IRCD_SERVER_REQUEST_H

/// The interface for when IRCd plays the role of client to other servers
///
namespace ircd::server
{
	struct in;
	struct out;
	struct request;

	size_t size(const in &);
	size_t size(const out &);

	void submit(const hostport &, request &);
	bool cancel(request &);
}

/// Request data and options related to transmitting the request. This
/// is where buffers must be supplied to send data to the server.
///
struct ircd::server::out
{
	const_buffer head;
	const_buffer content;
};

/// Request data and options related to the receive side of the request.
/// This is where buffers are supplied to receive data from the remote
/// server.
///
struct ircd::server::in
{
	mutable_buffer head;
	mutable_buffer content;
};

/// This is a handle for being a client to another server. This handle will
/// attempt to find an existing connection pool for the remote server otherwise
/// one will be created. Then it will multiplex your request and demultiplex
/// your response with all the other requests pending in the pipelines to
/// the remote.
///
struct ircd::server::request
:ctx::future<http::code>
{
	struct opts;

	static const opts opts_default;

	server::tag *tag {nullptr};

  public:
	/// Transmission data
	server::out out;

	/// Reception data
	server::in in;

	/// Options
	const struct opts *opts { &opts_default };

	request(const net::hostport &, server::out out, server::in in);
	request() = default;
	request(request &&) noexcept;
	request(const request &) = delete;
	request &operator=(request &&) noexcept;
	request &operator=(const request &) = delete;
	~request() noexcept;
};

struct ircd::server::request::opts
{
	/// When true, HTTP responses above the 200's are thrown as exceptions
	/// from the future::get() on this object. Otherwise, if false any code
	/// received is returned in the value and exceptions are thrown when no
	/// code can be returned.
	bool http_exceptions {true};
};

inline
ircd::server::request::request(const net::hostport &hostport,
                               server::out out,
                               server::in in)
:tag{nullptr}
,out{std::move(out)}
,in{std::move(in)}
{
	submit(hostport, *this);
}

inline
ircd::server::request::request(request &&o)
noexcept
:ctx::future<http::code>{std::move(o)}
,tag{std::move(o.tag)}
,out{std::move(o.out)}
,in{std::move(o.in)}
,opts{std::move(o.opts)}
{
	if(tag)
		associate(*this, *tag, std::move(o));
}

inline ircd::server::request &
ircd::server::request::operator=(request &&o)
noexcept
{
	this->~request();

	ctx::future<http::code>::operator=(std::move(o));
	out = std::move(o.out);
	in = std::move(o.in);
	tag = std::move(o.tag);
	opts = std::move(o.opts);

	if(tag)
		associate(*this, *tag, std::move(o));

	return *this;
}

inline
ircd::server::request::~request()
noexcept
{
	if(tag && !tag->committed())
		cancel(*this);

	if(tag)
		disassociate(*this, *tag);
}

inline size_t
ircd::server::size(const in &in)
{
	return size(in.head) + size(in.content);
}

inline size_t
ircd::server::size(const out &out)
{
	return size(out.head) + size(out.content);
}
