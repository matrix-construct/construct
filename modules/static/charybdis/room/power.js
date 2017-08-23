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

/**************************************
 * Power levels interface
 *
 * This is a category of functions that query the room's state
 * apropos room access control.
 */
room.power = {};

/**
 * Query the room state for the power levels (and the room state
 * provides the proper default content so this is always well defined).
 */
room.power.levels = function()
{
	return this.state['m.room.power_levels'].content;
};

/**
 * Get the power level required to issue an event of `type`
 * or the state_default for [anything we explicitly consider stat
 * because we can't see if a type has a state_key] or events_default
 * for any other type not specified.
 *
 * Also accepts the arbitrary transition labels for invite, kick, ban,
 * redact.
 */
room.power.event = function(type)
{
	let levels = this.power.levels();
	switch(type)
	{
		case "invite":   return levels.invite;
		case "redact":   return levels.redact;
		case "kick":     return levels.kick;
		case "ban":      return levels.ban;
		default:         break;
	}

	if(maybe(() => levels.events[type]) !== undefined)
		return levels.events[type];

	if((type in this.state))
		return levels.state_default;

	return levels.events_default;
};

/**
 * Get the power level of an mxid or the users_default if they
 * have no specified power level.
 */
room.power.user = function(mxid)
{
	let levels = this.power.levels();
	let power = maybe(() => levels.users[mxid]);
	return power !== undefined? power : levels.users_default;
};

/**
 * Decisional boolean for whether a user has power to issue an event
 * of `type` (or `type` can be a specified arbitrary transition label)
 */
room.power.has = function(mxid, type)
{
	return this.power.user(mxid) >= this.power.event(type);
};

/**
 * Determines the power level (this is vanity) for an 'operator' which
 * is someone who can issue at least one type of state event.
 */
room.power.op = function()
{
	let levels = this.power.levels();
	let ret = levels.state_default;
	Object.each(levels.events, (type, level) =>
	{
		if((type in this.state))
			if(level < ret)
				ret = level;
	});

	return ret;
};

/**
 * Determines the power level (this is vanity) for 'voice' which
 * is someone who can issue any event -- so someone without voice
 * is read-only to the timeline except for their own membership event.
 *
 * Voice is not specific to m.room.message... should it be?
 */
room.power.voice = function()
{
	let levels = this.power.levels();
	let ret = levels.events_default;
	Object.each(levels.events, (type, level) =>
	{
		if(level < ret)
			ret = level;
	});

	return ret;
};
