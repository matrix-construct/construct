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

/// The interface for when IRCd plays the role of client to other nodes
///
namespace ircd::server
{
	struct init;
	struct link;
	struct node;
	struct request;
	struct tag;

	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, buffer_overrun);
	IRCD_EXCEPTION(error, unavailable);
	IRCD_EXCEPTION(error, canceled);

	extern ircd::log::log log;
	extern std::map<string_view, std::shared_ptr<node>> nodes;

	size_t tag_count();
	size_t link_count();
	size_t node_count();

	bool exists(const net::hostport &);
	node &find(const net::hostport &);
	node &get(const net::hostport &);
}

#include "tag.h"
#include "request.h"
#include "link.h"
#include "node.h"

/// Subsystem initialization / destruction from ircd::main
///
struct ircd::server::init
{
	void interrupt();

	init();
	~init() noexcept;
};
