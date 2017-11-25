/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_SERVER_H

namespace ircd
{
	struct server;

	size_t write(server &, const ilist<const_buffer> &iov);
	char *read(server &, char *&start, char *const &stop);

	http::request::write_closure write_closure(server &);
	parse::read_closure read_closure(server &);
}

/// The interface for when IRCd plays the role of client to other servers
///
/// This is a handle for being a client to another server. This class is
/// named ircd::server and not ircd::client because this project is IRCd
/// and not IRCc. This handle will attempt to find an existing connection
/// pool for the remote server and then multiplex your requests and demultiplex
/// your responses to achieve concurrency and sharing and limitations and
/// shape the pipeline as best as the pool allows along with other instances
/// of ircd::server for the same remote.
///
/// Note that this means ircd::server is only appropriate for stateless
/// protocols like HTTP and chunked-encoding should be avoided if we are
/// to get the best out of nagle'ing the pipe. Individual net::socket should
/// be used otherwise.
///
/// This handle is a "tag" which services a single request and response per
/// instance to an ircd::server::node over one ircd::server::link available
/// to that node. Those interfaces are internal and don't have to be messed
/// with.
///
/// Instances of this class can be used in arrays to make bulk requests.
///
struct ircd::server
{
	struct init;
	struct node;
	struct link;

	static std::map<net::ipport, std::shared_ptr<node>> nodes;

	std::shared_ptr<node> n;
	unique_iterator<std::list<server *>> it;

  public:
	operator const net::remote &() const;

	server() = default;
	server(server &&) noexcept = default;
	server &operator=(server &&) noexcept = default;
	server(net::remote);
	~server() noexcept;
};

struct ircd::server::link
{
	enum state :int;

	std::shared_ptr<net::socket> s;
	std::deque<server *> q;
	enum state state;

	bool connected() const noexcept;
	bool connect(const net::remote &);
	bool disconnect();

	link(const net::remote &remote);
};

enum ircd::server::link::state
:int
{
	DEAD,
	IDLE,
	BUSY,
};

struct ircd::server::node
:std::enable_shared_from_this<ircd::server::node>
{
	enum state :int;

	net::remote remote;
	std::list<server *> tags;
	std::list<link> links;

	void add(const size_t &num = 1);
	void del(const size_t &num = 1);

	node(net::remote remote);
	~node() noexcept;
};

struct ircd::server::init
{
	init();
	~init() noexcept;
};
