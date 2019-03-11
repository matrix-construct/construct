// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

struct ircd::net::dns::resolver
{
	struct tag;
	using header = rfc1035::header;

	static conf::item<std::string> servers;
	static conf::item<milliseconds> timeout;
	static conf::item<milliseconds> send_rate;
	static conf::item<size_t> send_burst;
	static conf::item<size_t> retry_max;

	std::vector<ip::udp::endpoint> server;       // The list of active servers
	size_t server_next{0};                       // Round-robin state to hit servers
	ctx::dock dock;
	std::map<uint16_t, tag> tags;                // The active requests
	steady_point send_last;                      // Time of last send
	std::deque<uint16_t> sendq;                  // Queue of frames for rate-limiting
	ip::udp::socket ns;                          // A pollable activity object
	ip::udp::endpoint reply_from;                // Remote addr of recv
	unique_buffer<mutable_buffer> reply;         // Buffer for recv

	void add_server(const ipport &);
	void add_server(const string_view &);
	void set_servers(const string_view &list);
	void set_servers();

	bool handle_error(const error_code &ec) const;
	bool handle_error(const header &, const rfc1035::question &, tag &);
	void handle_reply(const header &, const const_buffer &body, tag &);
	void handle_reply(const header &, const const_buffer &body);
	void handle(const error_code &ec, const size_t &) noexcept;
	void set_handle();

	void send_query(const ip::udp::endpoint &, tag &);
	void queue_query(tag &);
	void send_query(tag &);
	void submit(tag &);

	template<class... A> tag &set_tag(A&&...);
	static const_buffer make_query(const mutable_buffer &buf, const tag &);
	void operator()(const hostport &, const opts &, callback &&);

	void handle_post_error(const uint16_t id, tag &, const std::system_error &);
	void post_error(tag &, const std::system_error &ec);
	bool check_timeout(const uint16_t &id, tag &, const steady_point &expired);
	void check_timeouts(const milliseconds &timeout);
	void cancel_all_tags();
	void timeout_worker();
	ctx::context timeout_context;

	void flush(const uint16_t &);
	void sendq_work();
	void sendq_clear();
	void sendq_worker();
	ctx::context sendq_context;

	resolver();
	~resolver() noexcept;
};

struct ircd::net::dns::resolver::tag
{
	uint16_t id {0};
	hostport hp;
	dns::opts opts;       // note: invalid after query sent
	const_buffer question;
	callback cb;
	steady_point last {steady_point::min()};
	uint8_t tries {0};
	ipport server;
	char hostbuf[rfc1035::NAME_BUF_SIZE];
	char qbuf[384];

	tag(const hostport &, const dns::opts &, callback &&);
	tag(tag &&) = delete;
	tag(const tag &) = delete;
};

inline
ircd::net::dns::resolver::tag::tag(const hostport &hp,
                                   const dns::opts &opts,
                                   callback &&cb)
:hp{hp}
,opts{opts}
,cb{std::move(cb)}
{
	this->hp.host = { hostbuf, copy(hostbuf, hp.host) };
}
