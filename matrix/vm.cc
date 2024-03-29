// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"m.vm", 'v'
};

decltype(ircd::m::vm::dock)
ircd::m::vm::dock;

decltype(ircd::m::vm::ready)
ircd::m::vm::ready;

decltype(ircd::m::vm::default_copts)
ircd::m::vm::default_copts;

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts;

//
// init
//

ircd::m::vm::init::init()
{
	id::event::buf event_id;
	sequence::retired = sequence::get(event_id);
	sequence::committed = sequence::retired;
	sequence::uncommitted = sequence::committed;

	vm::ready = true;
	vm::dock.notify_all();

	log::info
	{
		log, "BOOT %s @%lu [%s] db:%lu",
		server_name(my()),
		sequence::retired,
		sequence::retired?
			string_view{event_id} : "NO EVENTS"_sv,
		m::dbs::events?
			db::sequence(*m::dbs::events) : 0UL,
	};
}

ircd::m::vm::init::~init()
noexcept
{
	vm::ready = false;

	if(eval::executing || eval::injecting)
		log::warning
		{
			log, "Waiting for exec:%zu inject:%zu pending:%zu evaluations",
			eval::executing,
			eval::injecting,
			sequence::pending,
		};

	vm::dock.wait([]() noexcept
	{
		return !eval::executing && !eval::injecting;
	});

	if(sequence::pending)
		log::warning
		{
			log, "Waiting for pending:%zu sequencing (retired:%zu committed:%zu uncommitted:%zu)",
			sequence::pending,
			sequence::retired,
			sequence::committed,
			sequence::uncommitted,
		};

	sequence::dock.wait([]() noexcept
	{
		return !sequence::pending;
	});

	event::id::buf event_id;
	const auto retired
	{
		sequence::get(event_id)
	};

	log::info
	{
		log, "HALT '%s' @%lu [%s] vm:%lu:%lu:%lu db:%lu",
		server_name(my()),
		retired,
		retired?
			string_view{event_id} : "NO EVENTS"_sv,
		sequence::retired,
		sequence::committed,
		sequence::uncommitted,
		m::dbs::events?
			db::sequence(*m::dbs::events) : 0UL,
	};

	assert(retired == sequence::retired || ircd::read_only);
}

ircd::m::vm::phase
ircd::m::vm::phase_reflect(const string_view &str)
noexcept
{
	phase ret{phase::NONE};
	util::for_each<phase>([&ret, &str]
	(const auto &phase)
	{
		if(reflect(phase) == str)
		{
			ret = phase;
			return false;
		}
		else return true;
	});

	return ret;
}

ircd::string_view
ircd::m::vm::reflect(const enum phase &code)
{
	switch(code)
	{
		case phase::NONE:         return "NONE";
		case phase::EXECUTE:      return "EXECUTE";
		case phase::CONFORM:      return "CONFORM";
		case phase::DUPWAIT:      return "DUPWAIT";
		case phase::DUPCHK:       return "DUPCHK";
		case phase::ISSUE:        return "ISSUE";
		case phase::ACCESS:       return "ACCESS";
		case phase::EMPTION:      return "EMPTION";
		case phase::VERIFY:       return "VERIFY";
		case phase::FETCH_AUTH:   return "FETCH_AUTH";
		case phase::AUTH_STATIC:  return "AUTH_STATIC";
		case phase::FETCH_PREV:   return "FETCH_PREV";
		case phase::FETCH_STATE:  return "FETCH_STATE";
		case phase::PRECOMMIT:    return "PRECOMMIT";
		case phase::PREINDEX:     return "PREINDEX";
		case phase::AUTH_RELA:    return "AUTH_RELA";
		case phase::COMMIT:       return "COMMIT";
		case phase::AUTH_PRES:    return "AUTH_PRES";
		case phase::EVALUATE:     return "EVALUATE";
		case phase::INDEX:        return "INDEX";
		case phase::POST:         return "POST";
		case phase::WRITE:        return "WRITE";
		case phase::RETIRE:       return "RETIRE";
		case phase::NOTIFY:       return "NOTIFY";
		case phase::EFFECTS:      return "EFFECTS";
		case phase::_NUM_:        break;
	}

	return "??????";
}

ircd::http::code
ircd::m::vm::http_code(const fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return http::OK;
		case fault::EXISTS:       return http::CONFLICT;
		case fault::INVALID:      return http::BAD_REQUEST;
		case fault::GENERAL:      return http::UNAUTHORIZED;
		case fault::AUTH:         return http::FORBIDDEN;
		case fault::STATE:        return http::NOT_FOUND;
		case fault::EVENT:        return http::NOT_FOUND;
		case fault::BOUNCE:       break;
		case fault::DONOTWANT:    break;
		case fault::DENIED:       return http::FORBIDDEN;
		case fault::IDENT:        return http::UNAUTHORIZED;
	}

	return http::INTERNAL_SERVER_ERROR;
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "#ACCEPT";
		case fault::EXISTS:       return "#EXISTS";
		case fault::GENERAL:      return "#GENERAL";
		case fault::INVALID:      return "#INVALID";
		case fault::AUTH:         return "#AUTH";
		case fault::EVENT:        return "#EVENT";
		case fault::STATE:        return "#STATE";
		case fault::BOUNCE:       return "#BOUNCE";
		case fault::DONOTWANT:    return "#DONOTWANT";
		case fault::DENIED:       return "#DENIED";
		case fault::IDENT:        return "#IDENT";
	}

	return "??????";
}
