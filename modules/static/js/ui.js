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
