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

/**************************************
 * Final Initialization.
 */

/**
 * Main client controller attached to the #charybdis root div element in the application.
 * Only one instance of this controller is made.
 */
mc.ng.app.controller('mc', class extends mc.ng.controller
{
	constructor($scope, $timeout)
	{
		super("mc", $scope, $timeout);

		$scope['$'] = $;
		$scope.exists = (id) => $(id).length > 0;
		$scope.Object = Object;
		$scope.JSON = JSON;
		$scope.parseInt = parseInt;
		$scope.console = console;
		$scope.test = mc.test;
		$scope.debug = debug;
		$scope._typeof = (val) => typeof(val);
		$scope.maybe = maybe;
		$scope.empty = empty;
		$scope.length = length;
		$scope.togdel = togdel;
		$scope.togswap = togswap;
		$scope.ircd = ircd;
		$scope.mc = mc;

		//TODO: XXX: arbitrary
		mc.watch.$scope = $scope;
		mc.timeout.$timeout = $timeout;
	}
});

mc.ready = async function(event)
{
	let status; try
	{
		console.log("IRCd Charybdis Client: Hello.");
		status = await mc.main();
		console.warn("IRCd Charybdis Client: Main exited: " + status);
	}
	catch(error)
	{
		console.error("IRCd Charybdis Client: Main exited (error): " + error + " " + error.stack);
		//mc.unhandled(error);
		mc.abort(error);
	}
	finally
	{
		mc.count++;
	}
};

window.addEventListener("unload", async function(event)
{
	if(mc.execution) try
	{
		await mc.execution;
		console.log("IRCd Charybdis Client: Goodbye.");
	}
	finally
	{
		console.log("---------------------------------------------------------------");
	}
});

mc.run = async function(event = {})
{
	mc.execution = mc.ready(event);
}

$(document).ready((event) =>
{
	mc.run(event);
});
