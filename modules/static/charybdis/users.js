/*
 * IRCd Charybdis 5/Matrix
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk (jason@zemos.net)
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
 *
 */

'use strict';

/**
 ******************************************************************************
 * Users
 *
 */
mc.users = class extends Object.bindtree
{
	constructor()
	{
		super(mc.users,
		{
			enumerable: false,
		});
	}
};

mc.users.sync = function(events)
{
	events.forEach(this.sync.event);
};

mc.users.sync.event = function(event)
{
	this.learn(event);

	if(!(event.type in this.sync.event))
	{
		console.error("Unhandled mc.users.sync event[" + event.type + "]" + e);
		return;
	}

	this.sync.event[event.type](event);
};

mc.users.sync.event["m.push_rules"] = function(event)
{
	let user = this.get(event.sender);
	//debug.object({push_rules: event}, 2);
};

mc.users.sync.event["m.presence"] = function(event)
{
	let user = this.get(event.sender);
	Object.update(user, event.content);
};

/** Flash any matrix event to this function and the mc.users subsystem
 * can learn about users and user data from this event.
 */
mc.users.learn = function(event)
{
	if(typeof(event) != "object")
		return;

	if(event.sender)
		if(!this.has(event.sender))
			this.add(event.sender);

	if(event.target)
		if(!this.has(event.target))
			this.add(event.target);

	if(event.user_id)
		if(!this.has(event.user_id))
			this.add(event.user_id);

	if(event.sender && event.origin_server_ts)
	{
		let user = this.get(event.sender);
		let elapsed = mc.now() - event.origin_server_ts;
		if(!user.last_active_ago || user.last_active_ago > elapsed)
			user.last_active_ago = elapsed;
	}
};

mc.users.add = function(mxid)
{
	if(this.has(mxid))
		return this.get(mxid);

	let user = new mc.user(mxid);
	mc.users[mxid] = user;
	return user;
};

mc.users.get = function(mxid)
{
	return mc.users[mxid];
};

mc.users.has = function(mxid)
{
	return ((mxid in mc.users));
};

mc.users.displayname = function(mxid)
{
	let user = mc.users.get(mxid);
	if(!user)
		return mc.m.sid(mxid);

	if(!user.displayname)
		return mc.m.sid(mxid);

	return user.displayname;
};


/* Binds the mc.users tree of functions to a singleton instance.
 * see: mc.users.constructor()
 */
mc.users = new mc.users();
