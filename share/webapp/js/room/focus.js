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

room.focus = function()
{
	this.focus.last = mc.now();
	if(!this.focus.count++)
		this.focus.on_first_focus();
};

room.focus.defocus = function()
{
};

room.focus.focused = function()
{
	return mc.rooms.current(this.id) !== undefined;
};

// This is instance state
room.focus.last = 0;
room.focus.count = 0;

// Aliases
room.prototype.defocus = room.focus.defocus;
room.prototype.focused = room.focus.focused;

room.focus.on_first_focus = function()
{
	this.control.mode = "LIVE";
};
