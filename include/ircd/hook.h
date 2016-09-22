/*
 * Copyright (C) 2004-2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_HOOK_H

namespace ircd {
namespace hook {

using ctx::future;

// Function returns an empty future for serialization; valid for asynchronous
template<class state> using closure = std::function<future<> (state)>;

// Gives name of event required to happen.first (before), and required to happen.second (after)
using relationship = std::pair<std::string, std::string>;

// Returns true if A happens before B (for sorting)
bool happens_before(const std::string &a_name, const relationship &a_happens,
                    const std::string &b_name, const relationship &b_happens);


template<class state = void>
struct phase
{
	std::string name;
	relationship happens;
	closure<state> function;

	phase(const std::string &name, const relationship &, closure<state>&&);
	phase(const std::string &name, closure<state>&&);
	phase() = default;
};

template<class state>
phase<state>::phase(const std::string &name,
                    closure<state>&& function)
:phase{name, {}, std::forward<closure<state>>(function)}
{
}

template<class state>
phase<state>::phase(const std::string &name,
                    const relationship &happens,
                    closure<state>&& function)
:name{name}
,happens{happens}
,function{std::forward<closure<state>>(function)}
{
}


template<class state>
struct execution
{
	std::map<std::string, future<>> pending;         // phase.name => future
	std::multimap<std::string, std::string> fences;  // happens.second = > phase name

	void fences_wait(const std::string &name);
	void pending_wait(const std::string &name);
	void pending_wait();
};

template<class state>
void
execution<state>::pending_wait()
{
	for(const auto &pit : pending)
	{
		const auto &future(pit.second);
		future.wait();
	}
}

template<class state>
void
execution<state>::pending_wait(const std::string &name)
{
	const auto pit(pending.find(name));
	if(pit == end(pending))
		return;

	const scope erase([this, &pit]
	{
		pending.erase(pit);
	});

	const auto &future(pit->second);
	future.wait();
}

template<class state>
void
execution<state>::fences_wait(const std::string &name)
{
	const auto ppit(fences.equal_range(name));
	const scope erase([this, &ppit]
	{
		fences.erase(ppit.first, ppit.second);
	});

	for(auto pit(ppit.first); pit != ppit.second; ++pit)
	{
		const auto &name(pit->second);
		pending_wait(name);
	}
}


template<class state = void>
struct sequence
{
	std::list<phase<state>> space;

	void sort();

  public:
	template<class... phase> void add(phase&&...);
	void del(const std::string &name);

	void operator()(state);
};

template<class state>
void
sequence<state>::operator()(state s)
{
	execution<state> e;
	for(const auto &phase : space)
	{
		e.fences_wait(phase.name);
		e.pending_wait(phase.happens.first);

		auto future(phase.function(s));
		if(!future.valid())
			continue;

		if(!phase.happens.second.empty())
			e.fences.emplace(phase.happens.second, phase.name);

		e.pending.emplace(phase.name, std::move(future));
	}

	e.pending_wait();
}

template<class state>
void
sequence<state>::del(const std::string &name)
{
	space.remove_if([&name]
	(const auto &phase)
	{
		return phase.name == name;
	});
}

template<class state>
template<class... phase>
void
sequence<state>::add(phase&&... p)
{
	space.emplace_back(std::forward<phase>(p)...);
	sort();
}

template<class state>
void
sequence<state>::sort()
{
	space.sort([]
	(const auto &a, const auto &b)
	{
		return happens_before(a.name, a.happens, b.name, b.happens);
	});
}


template<class state = void>
class lambda
{
	sequence<state> *s;
	std::string name;

  public:
	lambda(sequence<state> &, const std::string &name, const relationship &, closure<state>);
	lambda(sequence<state> &, const std::string &name, closure<state>);
	~lambda() noexcept;
};

template<class state>
lambda<state>::lambda(sequence<state> &s,
                      const std::string &name,
                      const relationship &relationship,
                      closure<state> closure)
:s{&s}
,name{name}
{
	s.add(name, relationship, std::move(closure));
}

template<class state>
lambda<state>::lambda(sequence<state> &s,
                      const std::string &name,
                      closure<state> closure)
:lambda{s, name, {}, std::move(closure)}
{
}

template<class state>
lambda<state>::~lambda()
noexcept
{
	s->del(name);
}


template<class state = void>
class function
:lambda<state>
{
	virtual future<> operator()(state) const = 0;

  public:
	function(sequence<state> &, const std::string &name, const relationship & = {});
	virtual ~function() noexcept = default;
};

template<class state>
function<state>::function(sequence<state> &s,
                          const std::string &name,
                          const relationship &relationship)
:lambda<state>{s, name, relationship, [this]
(state&& st)
{
	return this->operator()(std::forward<state>(st));
}}
{
}

} // namespace hook
} // namespace ircd


/*
#ifdef __cplusplus
namespace ircd {

typedef struct
{
	char *name;
	rb_dlink_list hooks;
} hook;

typedef void (*hookfn) (void *data);

extern int h_iosend_id;
extern int h_iorecv_id;
extern int h_iorecvctrl_id;

extern int h_burst_client;
extern int h_burst_channel;
extern int h_burst_finished;
extern int h_server_introduced;
extern int h_server_eob;
extern int h_client_exit;
extern int h_umode_changed;
extern int h_new_local_user;
extern int h_new_remote_user;
extern int h_introduce_client;
extern int h_can_kick;
extern int h_privmsg_channel;
extern int h_privmsg_user;
extern int h_conf_read_start;
extern int h_conf_read_end;
extern int h_outbound_msgbuf;
extern int h_rehash;

void init_hook(void);
int register_hook(const char *name);
void add_hook(const char *name, hookfn fn);
void remove_hook(const char *name, hookfn fn);
void call_hook(int id, void *arg);

typedef struct
{
	client::client *client;
	void *arg1;
	void *arg2;
} hook_data;

typedef struct
{
	client::client *client;
	const void *arg1;
	const void *arg2;
} hook_cdata;

typedef struct
{
	client::client *client;
	const void *arg1;
	int arg2;
	int result;
} hook_data_int;

typedef struct
{
	client::client *client;
	client::client *target;
	chan::chan *chptr;
	int approved;
} hook_data_client;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	int approved;
} hook_data_channel;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	char *key;
} hook_data_channel_activity;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	chan::membership *msptr;
	client::client *target;
	int approved;
	int dir;
	const char *modestr;
} hook_data_channel_approval;

typedef struct
{
	client::client *client;
	client::client *target;
	int approved;
} hook_data_client_approval;

typedef struct
{
	client::client *local_link; // local client originating this, or NULL
	client::client *target; // dying client
	client::client *from; // causing client (could be &me or target)
	const char *comment;
} hook_data_client_exit;

typedef struct
{
	client::client *client;
	unsigned int oldumodes;
	unsigned int oldsnomask;
} hook_data_umode_changed;

enum message_type {
	MESSAGE_TYPE_NOTICE,
	MESSAGE_TYPE_PRIVMSG,
	MESSAGE_TYPE_PART,
	MESSAGE_TYPE_COUNT
};

typedef struct
{
	enum message_type msgtype;
	client::client *source_p;
	chan::chan *chptr;
	const char *text;
	int approved;
	const char *reason;
} hook_data_privmsg_channel;

typedef struct
{
	enum message_type msgtype;
	client::client *source_p;
	client::client *target_p;
	const char *text;
	int approved;
} hook_data_privmsg_user;

typedef struct
{
	bool signal;
} hook_data_rehash;

}      // namespace ircd
#endif // __cplusplus

*/
