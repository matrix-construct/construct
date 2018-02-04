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

/**
 * UI tools
 */

/**
 * Refresh a stylesheet over the DOM
 */
function reload_stylesheet(name = "construct.css")
{
	let do_reload = (elem, name) =>
	{
		let date = new Date().getMilliseconds();
		let repl = name + "?id=" + date;
		$(elem).attr("href", repl);
	};

	$("link[rel=stylesheet]").each((i, elem) =>
	{
		let nombre = $(elem).attr("href").split('?')[0];
		if(nombre != name)
			return;

		do_reload(elem, name);
		console.log("reloaded style from " + name);
	});
}

/**
 * Ctrl menu
 *
 * To add our own control key codes to the client (prudentially) we use
 * this object, which could appear as a menu if a DOM element wants to
 * $watch it.
 */
client.ctrl =
{
	menu:
	{
		"r":
		{
			name: "REFRESH",
			icon: "fa-reload",
			action: (event) =>
			{
				reload_stylesheet();
				event.preventDefault();
				event.stopPropagation();
			}
		},
	},

	apply: function(visible, event = undefined, prevent_default = true)
	{
		this.visible = visible;
		//client.apply();
		if(event !== undefined && prevent_default === true)
			event.preventDefault();
	},

	keydown: function(event)
	{
		if(event.which == 17)
			return this.apply(true, event);

		if(!this.visible)
			return;

		let letter = String.fromCharCode(event.which).toLowerCase();
		let item = this.menu[letter];
		if(item !== undefined)
			item.action(event);
	},

	keyup: function(event)
	{
		if(event.which == 17)
			return this.apply(false, event);
	},

	others: function(event)
	{
		return this.visible? this.apply(false) : undefined;
	},
};

// Enable the root control-key menu on init
$(window).on("keyup", client.ctrl.keyup.bind(client.ctrl));
$(window).on("keydown", client.ctrl.keydown.bind(client.ctrl));
$(window).on("blur", client.ctrl.others.bind(client.ctrl));

/**************************************
 * Misc util
 *
 */

mc.uri = function(string)
{
	return encodeURIComponent(string);
};
