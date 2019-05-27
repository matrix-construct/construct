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

	enum type type;
	std::string s;

	unit(std::string s, const enum type &type);
	unit(const m::event &event);
};

unit::unit(std::string s, const enum type &type)
:type{type}
,s{std::move(s)}
{
}

unit::unit(const m::event &event)
:type{json::get<"event_id"_>(event)? PDU : EDU}
,s{[this, &event]() -> std::string
{
	switch(this->type)
	{
		case PDU:
			return json::strung{event};

		case EDU:
			return json::strung{json::members
			{
				{ "content",   json::get<"content"_>(event)  },
				{ "edu_type",  json::get<"type"_>(event)     },
			}};

		default:
			return {};
	}
}()}
{
}

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
	std::array<char, rfc3986::DOMAIN_BUFSIZE> rembuf;
	string_view remote;
	m::node::room room;
	server::request::opts sopts;
	txn *curtxn {nullptr};
	bool err {false};

	bool flush();
	void push(std::shared_ptr<unit>);

	node(const string_view &remote)
	:remote{strlcpy{rembuf, remote}}
	,room{this->remote}
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
	,send{this->txnid, string_view{this->content}, this->headers, std::move(opts)}
	,node{&node}
	,timeout{now<steady_point>()} //TODO: conf
	{}

	txn() = default;
};
