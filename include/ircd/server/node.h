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
#define HAVE_IRCD_SERVER_NODE_H

/// Remote entity.
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

	void del(link &);
	void handle_error(link &, const boost::system::system_error &);
	void handle_close(link &, std::exception_ptr);
	void handle_open(link &, std::exception_ptr);

  public:
	size_t num_tags() const;
	size_t num_links() const;
	size_t num_links_busy() const;
	size_t num_links_ready() const;

	link &link_add(const size_t &num = 1);
	link &link_get();

	void cancel(request &);
	void submit(request &);

	node();
	~node() noexcept;
};
