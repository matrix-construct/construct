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
function reload_stylesheet(name = "charybdis.css")
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

/**
 * Explorer
 * (to be replaced with Angular)
 * Recursive DOM object tree to represent JSON structure for recursive exploration.
 */
let explorer = function(state, elem, depth = 0)
{
	let key = (member) => $(member).children(".key");
	let name_elem = (member) => key(member).children(".name");
	let name = (member) => name_elem(member).text().trim();
	let icon = (member) => key(member).children(".icon");
	let value = (member) => $(member).children(".value");
	let value_visible = (member) => value(member).is(":visible");
	let value_empty = (member) => value(member).html().trim().length == 0;
	let value_object = (member) => value(member).children(".object").length != 0;

	let icon_toggle = (member, active) =>
	{
		icon(member).toggleClass("fa-minus-square", active);
		icon(member).toggleClass("fa-plus-square", !active);
	};

	let value_toggle = (member) =>
	{
		let activate = !value_visible(member);
		icon_toggle(member, activate);
		value(member).toggle();
	};

	let value_clicked = (member) =>
	{
		if($(value(member)).text().length != 0)
			$(value(member)).html("<input class='input' type='text' value='" + $(value(member)).text() + "' />");
	};

	let member_clicked = (member) =>
	{
		value_toggle(member);
		if(value_empty(member))
		{
			let key = name(member);
			member.child = new explorer(state[key], value(member), depth + 1); // recurse
		}
		else delete member.child;
	};

	let member_click = (member) =>
	{
		key(member).click((event) => member_clicked(member));
	};

	let value_click = (member) =>
	{
		//value(member).click((event) => value_clicked(member));
	};

	let member_add = (root, key) =>
	{
		$(root).prepend($("#json_member").html());        // DOM template
		let member = $(root).children(".member").first();
		name_elem(member).append(key);
		icon(member).addClass("fa-plus-square");
		value(member).hide();
		member_click(member);
		value_click(member);
		return member;
	};

	let member_add_object = (root, key) =>
	{
		let member = member_add(root, key);
		if(!state[key] || Object.keys(state[key]).length == 0)
		{
			icon_toggle(member, true);
			icon(member).css("color", "#C8C8C8");
		}
	};

	let member_add_value = (root, key) =>
	{
		let member = member_add(root, key);
		value(member).text(state[key]);
		value_toggle(member);
	};

	let add = (root, key) =>
	{
		switch(typeof(state[key]))
		{
			case "object":
				member_add_object(root, key);
				break;

			case "boolean":
			case "number":
			case "string":
				member_add_value(root, key);
				break;
		}
	};

	// Add the outer object div to the content element as the root object/div
	$(elem).prepend($("#json_object").html());          // DOM template
	let root = $(elem).children(".object").first();

	// Add member divs for each member of the object
	for(let key in state)
		add(root, key);
};

/** Explorer replacement here
 */
mc.ng.app.controller('explore', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("explore", $scope);
	}
});


/**************************************
 * Misc util
 *
 */

mc.uri = function(string)
{
	return encodeURIComponent(string);
};
