/*
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.
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
	return this.content['m.room.power_levels'][''];
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
