/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * hook.c - code for dealing with the hook system
 *
 * ~~~~~~~~
 * This code is basically a slow leaking array.  Events are simply just a
 * position in this array.  When hooks are added, events will be created if
 * they dont exist - this means modules with hooks can be loaded in any
 * order, and events are preserved through module reloads.
 * ~~~~~~~~~ left the comment but the code is gone. --jzk
 *
 * Copyright (C) 2004-2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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

namespace ircd {
namespace hook {


} // namespace hook
} // namespace ircd

using namespace ircd;

bool
hook::happens_before(const std::string &a_name,
                     const relationship &a_happens,
                     const std::string &b_name,
                     const relationship &b_happens)
{
	// Explicit request by b that a happens before
	if(b_happens.first == a_name)
		return true;

	// Explicit request by a that b happens before
	if(a_happens.first == b_name)
		return false;

	// Explicit request by b that a happens after
	if(b_happens.second == a_name)
		return false;

	// Explicit request by a that b happens after
	if(a_happens.second == b_name)
		return true;

	// a happens before everything and b has at least something before it
	if(a_happens.first.empty() && !b_happens.first.empty())
		return true;

	// b happens before everything and a has at least something before it
	if(b_happens.first.empty() && !a_happens.first.empty())
		return false;

	// a happens after everything and b has at least something after it
	if(a_happens.second.empty() && !b_happens.second.empty())
		return false;

	// b happens after everything and a has at least something after it
	if(b_happens.second.empty() && !a_happens.second.empty())
		return true;

	//TODO: ???
	// both have the same before requirement
	if(a_happens.first == b_happens.first)
		return a_happens.second < b_happens.second;

	//TODO: ???
	// both have the same after requirement
	if(a_happens.second == b_happens.second)
		return a_happens.first < b_happens.first;

	//TODO: ???
	return false;
}


/*

hook *hooks;

#define HOOK_INCREMENT 1000

int num_hooks = 0;
int last_hook = 0;
int max_hooks = HOOK_INCREMENT;

int h_burst_client;
int h_burst_channel;
int h_burst_finished;
int h_server_introduced;
int h_server_eob;
int h_client_exit;
int h_umode_changed;
int h_new_local_user;
int h_new_remote_user;
int h_introduce_client;
int h_can_kick;
int h_privmsg_user;
int h_privmsg_channel;
int h_conf_read_start;
int h_conf_read_end;
int h_outbound_msgbuf;
int h_rehash;

void
init_hook(void)
{
	hooks = (hook *)rb_malloc(sizeof(hook) * HOOK_INCREMENT);

	h_burst_client = register_hook("burst_client");
	h_burst_channel = register_hook("burst_channel");
	h_burst_finished = register_hook("burst_finished");
	h_server_introduced = register_hook("server_introduced");
	h_server_eob = register_hook("server_eob");
	h_client_exit = register_hook("client_exit");
	h_umode_changed = register_hook("umode_changed");
	h_new_local_user = register_hook("new_local_user");
	h_new_remote_user = register_hook("new_remote_user");
	h_introduce_client = register_hook("introduce_client");
	h_can_kick = register_hook("can_kick");
	h_privmsg_user = register_hook("privmsg_user");
	h_privmsg_channel = register_hook("privmsg_channel");
	h_conf_read_start = register_hook("conf_read_start");
	h_conf_read_end = register_hook("conf_read_end");
	h_outbound_msgbuf = register_hook("outbound_msgbuf");
	h_rehash = register_hook("rehash");
}

*/
