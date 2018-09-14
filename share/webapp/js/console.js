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


/** Console room
 *
 */

mc.console = {};

mc.console.init = function()
{
	let room = mc.rooms.get(
	{
		name: "Client Console",
		room_id: "!console:localhost",
		canonical_alias: "#console:localhost",
		topic: "Client-only (local) room representing the browser console.",
    });
/*
	let original_log = console.log;
	let original_error = console.error;
	let original_trace = console.trace;
	let hook = function(original_function, sender, message)
	{
		original_function.bind(console)(message);
		//room.console_push(message);
		if(mc.current_room !== undefined && mc.current_room == room)
		//if(mc.current_room !== undefined)
		{
			let sab = mc.current_room.scroll.at.bottom();
			mc.current_room.console_push(message);
			//mc.model.$digest();
			//if(sab)
			//	mc.current_room.scroll_to_bottom();
		}
	};

	window.console.log = function(message)
	{
		hook(original_log, "log:browser", message);
	};

	window.console.error = function(message)
	{
		hook(original_error, "error:browser", message);
	};

	window.console.trace = function(message = undefined)
	{
		hook(original_trace, "trace:browser", message);
	};
*/
};
