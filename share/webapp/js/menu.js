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
