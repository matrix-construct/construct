// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

/// Argument parser for the client's /sync request
struct syncargs
{
	string_view filter_id;
	string_view since;
	steady_point timesout;
	bool full_state;
	bool set_presence;

	syncargs(const resource::request &);
};

/// State for a client conducting a longpoll /sync. This is used after a
/// session has caught up with initial-sync and shortpoll and is waiting
/// for the next event.
struct longpoll
{
	std::weak_ptr<ircd::client> client;
	steady_point timesout;
	std::string user_id;
	std::string since;
	std::string access_token;

	longpoll(ircd::client &, const resource::request &, const syncargs &);
};

struct shortpoll
{

};

static bool update_sync(const longpoll &data, const m::event &event, const m::room &);
static void synchronize(const m::event &, const m::room::id &);
static void synchronize(const m::event &);
static void synchronize();

std::list<longpoll> polling;
ctx::dock synchronizer_dock;
static void del(longpoll &);
static void add(longpoll &);
static bool timedout(const std::weak_ptr<client> &);
static void timeout_check();
static void errored_check();
static void worker();
extern ircd::context synchronizer;

static resource::response longpoll_sync(client &, const resource::request &, const syncargs &);
static resource::response shortpoll_sync(client &, const resource::request &, const syncargs &);
static int64_t get_sequence(const resource::request &, const syncargs &, const m::room &user_room);
static resource::response since_sync(client &, const resource::request &, const syncargs &);
static resource::response initial_sync(client &, const resource::request &, const syncargs &);
static resource::response sync(client &, const resource::request &);
extern resource::method get_sync;

extern const string_view sync_description;
extern resource sync_resource;

void on_unload();
extern mapi::header IRCD_MODULE;
