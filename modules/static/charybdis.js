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
		mc.unhandled(error);
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
