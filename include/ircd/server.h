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
#define HAVE_IRCD_SERVER_H

/// The interface for when IRCd plays the role of client to other servers
///
namespace ircd::server
{
	struct init;
	struct node;
	struct link;
	struct request;
	struct out;
	struct in;

	IRCD_EXCEPTION(ircd::error, error);

	extern ircd::log::log log;
	extern std::map<string_view, std::shared_ptr<node>> nodes;

	bool exists(const net::hostport &);
	node &find(const net::hostport &);
	node &get(const net::hostport &);
}

struct ircd::server::out
{
	const_buffer head;
	const_buffer content;
};

struct ircd::server::in
{
	mutable_buffer head;
	mutable_buffer content;
};

/// This is a handle for being a client to another server. This handle will
/// attempt to find an existing connection pool for the remote server otherwise
/// one will be created. Then it will multiplex your requests and demultiplex
/// your responses.
///
struct ircd::server::request
:ctx::future<http::code>
{
	struct tag;

	struct tag *tag {nullptr};
	http::response::head head;

  public:
	server::out out;
	server::in in;

	request(const net::hostport &, server::out out, server::in in);
	request() = default;
	request(request &&) noexcept;
	request(const request &) = delete;
	request &operator=(request &&) noexcept;
	request &operator=(const request &) = delete;
	~request() noexcept;
};

/// Internal portion of the request
//
struct ircd::server::request::tag
{
	server::request *request;
	ctx::promise<http::code> p;
	size_t head_read {0};
	size_t content_read {0};

	mutable_buffer make_content_buffer() const;
	mutable_buffer make_head_buffer() const;

	bool read_content(const const_buffer &, const_buffer &overrun);
	bool read_head(const const_buffer &, const_buffer &overrun);

  public:
	mutable_buffer make_read_buffer() const;
	bool read_buffer(const const_buffer &, const_buffer &overrun);

	tag(server::request &);
	tag(tag &&) noexcept;
	tag(const tag &) = delete;
	tag &operator=(tag &&) noexcept;
	tag &operator=(const tag &) = delete;
	~tag() noexcept;
};

/// Internal representation of a remote server.
///
struct ircd::server::node
:std::enable_shared_from_this<ircd::server::node>
{
	std::exception_ptr eptr;
	net::remote remote;
	std::list<link> links;
	ctx::dock dock;

	void handle_resolve(std::weak_ptr<node>, std::exception_ptr, const ipport &);
	void resolve(const hostport &);

  public:
	link &link_add(const size_t &num = 1);
	void link_del(const size_t &num = 1);
	link &link_get();

	void cancel(request &);
	void submit(request &);

	node();
	~node() noexcept;
};

/// Internal representation of a single connection to a remote.
///
struct ircd::server::link
{
	bool init {false};                           ///< link is connecting
	bool fini {false};                           ///< link is disconnecting
	std::shared_ptr<server::node> node;          ///< backreference to node
	std::exception_ptr eptr;                     ///< error from socket
	std::shared_ptr<net::socket> socket;         ///< link's socket
	std::deque<struct request::tag> queue;       ///< link's work queue

	bool connected() const noexcept;
	bool ready() const;
	bool busy() const;

  protected:
	void handle_writable(const error_code &);
	void wait_writable();

	void handle_readable_success();
	void handle_readable(const error_code &);
	void wait_readable();

	void handle_close(std::exception_ptr);
	void handle_open(std::exception_ptr);

  public:
	bool close(const net::close_opts &);
	bool open(const net::open_opts &);

	void cancel(request &);
	void submit(request &);

	link(server::node &);
	~link() noexcept;
};

/// Subsystem initialization / destruction from ircd::main
///
struct ircd::server::init
{
	void interrupt();

	init();
	~init() noexcept;
};
