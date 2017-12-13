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
 *
 * Menu controller
 *
 */
mc.ng.app.controller('menu', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("menu", $scope);
	}

	/* (a)ng-click on a menu button
	 *
	 * 1. Item tab can define a click(event) function.
	 * 2. Item tab can define a target element, which will be shown/hidden.
	 */
	async toggle(menu, name, event, ...args)
	{
		let e = event.delegateTarget;
		let item = menu[name];
		if(item === undefined)
			return;

		if(item.disabled)
			return;

		if(item.holds !== undefined && event.type == "click")
			return;

		if(typeof(item.click) == "function")
			if(await item.click(event, ...args) === false)
				return;

		if(item.target !== undefined)
		{
			let state = mc.show[item.target]? true : false;
			mc.show[item.target] = !state;
		}
	}

	/* hold down on a menu button
	 * use ng-mousedown and ng-mouseup, or ng-click
	 *
	 */
	hold(menu, name, event, ...args)
	{
		let e = event.delegateTarget;
		let item = menu[name];
		if(item === undefined)
			return;

		if(item.holds === undefined)
			return;

		let click = () =>
			this.toggle(menu, name, event, ...args);

		switch(event.type)
		{
			case "mousedown":
				$(e).addClass("holding");
				item.holding = mc.timeout(item.holds, click);
				event.stopPropagation();
				break;

			case "mouseup":
				$(e).removeClass("holding");
				mc.timeout.cancel(item.holding);
				event.stopPropagation();
				break;
		}
	}

	/* Determines whether a menu button is considered "selected"
	 * which generally applies a CSS class to change the color etc.
	 *
	 * The goal here is not simply to determine selection by the user's interaction
	 * with the menu button itself, but to also allow distant functionality to assert
	 * the menu button is no longer selected. For example: closing a div without using
	 * menu button that opened it should not leave the menu button in the selected state.
	 *
	 * Options:
	 * 1. Use mc.show boolean dictionary by element ID
	 * 2. A boolean value in the menu item tab 'selected': true/false,
	 * 3. A function in place of the boolean value in #1.
	 * 4. If the menu item uses the "target" concept, the target element is :hidden.
	 */
	selected(menu, name, item)
	{
		if(item === undefined)
			return false;

		switch(typeof(item.selected))
		{
			case "undefined":
				break;

			case "function":
				return item.selected();

			default:
				return item.selected == true;
		}

		if(item.target !== undefined)
		{
			if($(item.target).length == 0)
				return false;

			return mc.show[item.target] === true? true : false;
		}

		return false;
	}
});
