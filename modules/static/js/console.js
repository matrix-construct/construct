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
