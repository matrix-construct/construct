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
#define HAVE_IRCD_SERVER_LINK_H

/// A single connection to a remote node.
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
