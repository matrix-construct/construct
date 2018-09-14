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
 *
 * Errors and Debugging
 *
 */

/** Root exception for client
 *
 * The members of an error object are correlated with the bindings
 * in the DOM template 'ircd_error'
 *
 * @param arguments[1] (or arguments[0].handled) - This callback is invoked when the exception
 * has been "handled." It's a way to place a continuation in the same callback scopechain
 * after the user or other code has acknowledged the error.
 *
 * Also: arguments[0].element - A DOM element from which is considered the "user origin"
 * for the exception (ex. a <button>). If this exception is unhandled in JS code, it is then
 * propagated into the DOM at the element specified and percolates up the DOM until a parent
 * element is capable of displaying the contents indicated by the 'ircd-catch' attribute. If no
 * elements catch the exception it is overlayed on the root element.
 */
mc.error = class extends Error
{
	constructor()
	{
		let object = () =>
		{
			let error = arguments[0];
			super(error.message);

			// Recursive assignment from the other object.
			Object.update(this, error, Object.oeach);

			if(error.stack)
				this.stack = error.stack;

			if(error.name)
				this.name = error.name;
		};

		let string = () =>
		{
			super(arguments[0]);
		};

		typeswitch(arguments[0],
		{
			object: object,
			string: string,
		});
	}
};

/** Fatal exception handler ala std::terminate()
 *
 * For some unhandled exceptions, we shut down the whole show as best as possible.
 * In this case we assume the system is in a compromised state and should crash.
 */
mc.abort = (error) =>
{
	error.fatal = true;
	if(!(error instanceof mc.error))
		error = new mc.error(error);

	let abortion = debug.stringify(error, 2, Object.oeach);
	$("body").css("background-color", "black");
	$("body").css("color", "white");
	$("body").css("font-family", "monospace");
	$("body").css("white-space", "pre");
	$("body").text(abortion);
	window.stop();
	//window.abort();
};

/** For now, tap the host exception handler to use our abort().
 */
window.addEventListener("error", (msg, url, line, column, error) => mc.abort(
{
	name: "Fatal Error",
	message: msg,
	stack: error && error["stack"]? error.stack : null,
	url: url,
	line: line,
	column: column,
	error: error
}));

/** Unhandled exception handler
 *
 * Where exceptions are sent after being handled (or not) in some JS callstack. This
 * propagates the exception to the DOM for display to the user, and eventually if no
 * viable DOM catch directive displays it, propagation continues to the root element
 * of the application which will smother the whole app with the black screen of doom.
 */
mc.unhandled = function(error)
{
	try
	{
		if(error.name == "timeout")
			return;

		if(!error.element)
		{
			console.warn("No element found to place unhandled exception...");
			error.element = angular.element("#charybdis");
		}

		if(!(error instanceof mc.error))
			error = new mc.error(error);

		let scope = maybe(() => error.element.scope());
		if(!scope)
			scope = mc.ng.mc();

		if(scope)
			scope.error = error;
	}
	catch(e)
	{
		error.double_fault = e;
		mc.abort(error);
	}
};

/** Any unhandled exception out of a promise.
 */
window.addEventListener("unhandledrejection", (event) =>
{
	let error = event.reason;
	event.preventDefault();
	mc.unhandled(error);
	mc.ng.apply();
});

/** Tap Angular's callback stack exception handler to integrate with
 * our system.
 */
/*
mc.ng.app.config(function($provide)
{
	$provide.factory('$exceptionHandler', function($injector)
	{
		return function(exception, cause)
		{
			console.error("Angular error: " + exception);
			let error = new mc.error(exception);
			//mc.unhandled(error);
			mc.abort(error);
		};
	});
});
*/

/** Tap jquery's ajax exception handler to integrate with our system
 */
/*
$(document).ajaxError(function(event, jqxhr, settings, thrownError)
{
	debug.error({ajaxError: {settings: settings, thrownError: thrownError}}, 6);
});
*/

/** This controller backs an instance of an 'ircd_error' div when it's created and inserted
 * into the DOM at a catch point. We provide relative logic for the error div here, including
 * a close() function which removes the div itself from the parent where it was inserted,
 * therefor it should reap the controller itself with GC later.
 */
mc.ng.app.controller('errors', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("errors", $scope);
		$scope.op = this.op.bind($scope);
		$scope.close = this.close.bind($scope);
	}

	// Finds error div from within error div based on event
	op($event)
	{
		let e = $($event.delegateTarget);
		if($(e).is("div") && $(e).hasClass("error"))
			return e;

		return $(e).parents("div.error");
	}

	// Closes the error dialogue and considers it handled.
	close($event)
	{
		$(this.op($event)).detach();
		typeif(this.error.handled, "function", () => this.error.handled($event));
		$event.stopPropagation();
	}
});

/** This directive can be affixed to a <div> which will stop exception
 * propagation out of any child of that <div> and either absolutely position
 * an ircd-error template over the catching <div> or delegate the overlaying to
 * another child where the error is better displayed.
 *
 * Exceptions propagate up to the DOM root unless they hit an ircd-catch
 * directive first.
 */
mc.ng.app.directive('ircdCatch', function($compile)
{
	// Called once the final determinations for the throwing and catching
	// elements are made.
	let on_catch = ($scope, thrower, catcher, error) =>
	{
		let opts = {};
		if($(catcher).attr("ircd-catch"))
			Object.update(opts, JSON.parse($(catcher).attr("ircd-catch")));

		if(opts.delegate)
		{
			let delegate = $(catcher).find(opts.delegate);
			if($(delegate).length)
				catcher = delegate;
		}

		$(catcher).prepend($scope.error_element);
		let prepended = $(catcher).children("div.error");
		let scope = angular.element(prepended).scope();
		scope.error = error;
		$(prepended).focus();
	};

	// When an exception is seen in the scope this is called for each
	// catch block to determine if it is the most appropriate for handling
	// the error.
	let on_error = ($scope, element, attrs, error) =>
	{
		if(typeof(error) != "object")
			return;

		if(empty(error))
			return;

		if(typeof(error.element) != "object")
			error.element = $("#charybdis");

		let catcher = $(element);
		let thrower = $(error.element);
		if(!$(catcher).is($(thrower)))
		{
			if($(thrower).attr("ircd-catch") !== undefined)
				return;

			let closest = $(thrower).parents("*[ircd-catch]");
			if(!$(closest).length)
				return;

			if(!$($(closest)[0]).is($(catcher)))
				return;
		}

		on_catch($scope, thrower, catcher, error);
		delete $scope.error;
	};

	// ircd-error template is compiled here.
	let pre_compile = function($scope, element, attrs)
	{
		let template = $("#ircd_error");
		$scope.error_element = $compile(template.html())($scope);
	};

	// Each catching element listens for exception objects placed in its
	// purview so it can readily display them.
	let post_compile = function($scope, element, attrs)
	{
		$scope.$watch('error', function(new_error)
		{
			on_error($scope, element, attrs, new_error);
		});
	};

	// The compiler is invoked for each ircd-catch in the DOM
	let compile =
	{
		pre: pre_compile,
		post: post_compile,
	};

	let ret =
	{
		restrict: 'AE',
		compile: (element, attrs) => compile,
	};

	return ret;
});
