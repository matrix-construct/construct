// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>

extern ircd::mapi::header
IRCD_MODULE;

namespace ircd::net::dns
{
	struct tag;

	using answers = vector_view<const rfc1035::answer>;
	using answers_callback = std::function<void (std::exception_ptr, const tag &, const answers &)>;

	constexpr const size_t MAX_COUNT {64};

	template<class T> static rfc1035::record *new_record(mutable_buffer &, const rfc1035::answer &);
	static void handle_resolved(std::exception_ptr, const tag &, const answers &);

	static void handle_resolve_A_ipport(const hostport &, const json::object &rr, opts, uint16_t, callback_ipport);
	static void handle_resolve_SRV_ipport(const hostport &, const json::object &rr, opts, callback_ipport);
	static void handle_resolve_one(const hostport &, const json::array &rr, callback_one);
}

namespace ircd::net::dns::cache
{
	struct waiter;

	static bool call_waiter(const string_view &, const string_view &, const json::array &, waiter &);
	static void call_waiters(const string_view &, const string_view &, const json::array &);
	static void handle(const m::event &, m::vm::eval &);

	static bool put(const string_view &type, const string_view &state_key, const records &rrs);
	static bool put(const string_view &type, const string_view &state_key, const uint &code, const string_view &msg);

	extern conf::item<seconds> min_ttl IRCD_MODULE_EXPORT_DATA;
	extern conf::item<seconds> error_ttl IRCD_MODULE_EXPORT_DATA;
	extern conf::item<seconds> nxdomain_ttl IRCD_MODULE_EXPORT_DATA;

	extern const m::room::id::buf room_id;
	extern m::hookfn<m::vm::eval &> hook;
	extern std::list<waiter> waiting;
	extern ctx::dock dock;
}

struct ircd::net::dns::cache::waiter
{
	dns::callback callback;
	dns::opts opts;
	uint16_t port {0};
	string_view key;
	char keybuf[rfc1035::NAME_BUF_SIZE*2];

	waiter(const hostport &hp, const dns::opts &opts, dns::callback &&callback)
	:callback{std::move(callback)}
	,opts{opts}
	,port{net::port(hp)}
	,key
	{
		opts.qtype == 33?
			make_SRV_key(keybuf, hp, opts):
			strlcpy(keybuf, host(hp))
	}
	{}
};

//
// s_dns_resolver.cc
//

namespace ircd::net::dns
{
	// Resolver instance
	struct resolver extern *resolver;

	uint16_t resolver_call(const hostport &, const opts &);
	void resolver_init(answers_callback);
	void resolver_fini();
}

struct ircd::net::dns::resolver
{
	using header = rfc1035::header;

	static conf::item<std::string> servers;
	static conf::item<milliseconds> timeout;
	static conf::item<milliseconds> send_rate;
	static conf::item<size_t> send_burst;
	static conf::item<size_t> retry_max;

	answers_callback callback;
	std::vector<ip::udp::endpoint> server;       // The list of active servers
	size_t server_next{0};                       // Round-robin state to hit servers
	ctx::dock dock, done;
	ctx::mutex mutex;
	std::map<uint16_t, tag> tags;                // The active requests
	steady_point send_last;                      // Time of last send
	std::deque<uint16_t> sendq;                  // Queue of frames for rate-limiting
	ip::udp::socket ns;                          // A pollable activity object

	// util
	void add_server(const ipport &);
	void add_server(const string_view &);
	void set_servers(const string_view &list);
	void set_servers();

	// removal (must have lock)
	void unqueue(tag &);
	void remove(tag &);
	decltype(tags)::iterator remove(tag &, const decltype(tags)::iterator &);
	void error_one(tag &, const std::exception_ptr &, const bool &remove = true);
	void error_one(tag &, const std::system_error &, const bool &remove = true);
	void error_all(const std::error_code &, const bool &remove = true);
	void cancel_all(const bool &remove = true);

	// reception
	bool handle_error(const header &, tag &);
	void handle_reply(const header &, const const_buffer &body, tag &);
	void handle_reply(const ipport &, const header &, const const_buffer &body);
	void handle(const ipport &, const mutable_buffer &);
	void recv_worker();
	ctx::context recv_context;

	// submission
	void send_query(const ip::udp::endpoint &, tag &);
	void queue_query(tag &);
	void send_query(tag &);
	void submit(tag &);

	// timeout
	bool check_timeout(const uint16_t &id, tag &, const steady_point &expired);
	void check_timeouts(const milliseconds &timeout);
	void timeout_worker();
	ctx::context timeout_context;

	// sendq
	void flush(const uint16_t &);
	void sendq_work();
	void sendq_clear();
	void sendq_worker();
	ctx::context sendq_context;

	template<class... A> tag &set_tag(A&&...);
	const_buffer make_query(const mutable_buffer &buf, tag &);
	uint16_t operator()(const hostport &, const opts &);

	resolver(answers_callback);
	~resolver() noexcept;
};

struct ircd::net::dns::tag
{
	uint16_t id {0};
	hostport hp;
	dns::opts opts;       // note: invalid after query sent
	const_buffer question;
	steady_point last {steady_point::min()};
	uint8_t tries {0};
	uint rcode {0};
	ipport server;
	char hostbuf[rfc1035::NAME_BUF_SIZE];
	char qbuf[512];

	tag(const hostport &hp, const dns::opts &opts)
	:hp{hp}
	,opts{opts}
	{
		this->hp.host = { hostbuf, copy(hostbuf, hp.host) };
		this->hp.service = {};
	}

	tag(tag &&) = delete;
	tag(const tag &) = delete;
};
