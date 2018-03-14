// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// Internal header for sender.cc only.
// Do not include.
//

using namespace ircd;

struct txn;
struct node;

struct unit
:std::enable_shared_from_this<unit>
{
	enum type { PDU, EDU, FAILURE };

	std::string s;
	enum type type;

	unit(std::string s, const enum type &type)
	:s{std::move(s)}
	,type{type}
	{}

	unit(const m::event &event)
	:unit{json::strung{event}, PDU}
	{}
};

struct txndata
{
	std::string content;
	string_view txnid;
	char txnidbuf[64];

	txndata(std::string content)
	:content{std::move(content)}
	,txnid{m::txn::create_id(txnidbuf, this->content)}
	{}
};

struct node
{
	std::deque<std::shared_ptr<unit>> q;
	m::node::id::buf id;
	m::node::room room;
	server::request::opts sopts;
	txn *curtxn {nullptr};

	bool flush();
	void push(std::shared_ptr<unit>);

	node(const string_view &origin)
	:id{"", origin}
	,room{id}
	{}
};

struct txn
:txndata
,m::v1::send
{
	struct node *node;
	steady_point timeout;
	char headers[8_KiB];

	txn(struct node &node,
	    std::string content,
	    m::v1::send::opts opts)
	:txndata{std::move(content)}
	,send{txnid, string_view{this->content}, headers, std::move(opts)}
	,node{&node}
	,timeout{now<steady_point>()} //TODO: conf
	{}

	txn() = default;
};
