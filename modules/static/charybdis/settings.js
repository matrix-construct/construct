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
