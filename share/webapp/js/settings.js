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

/******************************************************************************
 * Settings
 *
 */
mc.settings = {};

mc.settings.init = function()
{
	let room = mc.rooms.get(
	{
		name: "Client Settings",
		room_id: "!settings:mc",
		canonical_alias: "#settings:mc",
		topic: "Construct client configuration"
	});

	room.timeline.insert(new mc.event(
	{
		type: "mc.settings.user_id",
		state_key: "",
		content:
		{
			user_id: mc.session.user_id,
		},
	}));

	room.timeline.insert(new mc.event(
	{
		type: "mc.settings.displayname",
		state_key: "",
		content:
		{
			displayname: mc.my.displayname,
		},
	}));

	room.timeline.insert(new mc.event(
	{
		type: "mc.settings.email",
		state_key: "",
		content:
		{
			email: undefined,
		},
	}));
};
