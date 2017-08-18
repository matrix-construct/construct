/* 
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
 */

using namespace ircd;

//using object = db::object<m::db::accounts>;
//template<class T = string_view> using value = db::value<T, m::db::accounts>;

resource sync_resource
{
	"_matrix/client/r0/sync",
	"Synchronise the client's state with the latest state on the server. "
	"Clients use this API when they first log in to get an initial snapshot of "
	"the state on the server, and then continue to call this API to get "
	"incremental deltas to the state, and to receive new messages. (6.2)"
};

resource::response
sync(client &client, const resource::request &request)
{
	const auto filter
	{
		// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
		// encoded as a string. The server will detect whether it is an ID or a JSON object
		// by whether the first character is a "{" open brace. Passing the JSON inline is best
		// suited to one off requests. Creating a filter using the filter API is recommended
		// for clients that reuse the same filter multiple times, for example in long poll requests.
		request["filter"]
	};

	const auto since
	{
		// 6.2.1 A point in time to continue a sync from.
		request["since"]
	};

	const auto full_state
	{
		// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
		// If this is set to true, then all state events will be returned, even if since is non-empty.
		// The timeline will still be limited by the since parameter. In this case, the timeout
		// parameter will be ignored and the query will return immediately, possibly with an
		// empty timeline. If false, and since is non-empty, only state which has changed since
		// the point indicated by since will be returned. By default, this is false.
		request.get<bool>("full_state", false)
	};

	const auto set_presence
	{
		// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
		// If this parameter is omitted then the client is automatically marked as online when it
		// uses this API. Otherwise if the parameter is set to "offline" then the client is not
		// marked as being online when it uses this API. One of: ["offline"]
		request.get("set_presence", "offline")
	};

	const auto timeout
	{
		// 6.2.1 The maximum time to poll in milliseconds before returning this request.
		request.get<time_t>("timeout", -1)
	};

	return {};
}

resource::method get_sync
{
	sync_resource, "GET", sync,
	{
		get_sync.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/sync' to handle requests."
};
