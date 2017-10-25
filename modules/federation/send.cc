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

mapi::header IRCD_MODULE
{
	"federation send"
};

struct send
:resource
{
	using resource::resource;
}
send_resource
{
	"/_matrix/federation/v1/send/", resource::opts
	{
		resource::DIRECTORY,
		"federation send"
	}
};

void
handle_edu(client &client,
           const resource::request::object<m::txn> &request,
           const string_view &txn_id,
           const json::object &edu)
{
	//std::cout << edu << std::endl;
	log::debug("%s | %s | %s | %s",
	           at<"origin"_>(request),
	           txn_id,
	           edu.at("edu_type"),
	           edu.get("sender", string_view{"*"}));
}

void
handle_pdu(client &client,
           const resource::request::object<m::txn> &request,
           const string_view &txn_id,
           const m::event &event)
try
{
	//std::cout << event << std::endl;
	log::debug("%s | %s | %s | %s %s %s",
	           at<"origin"_>(request),
	           txn_id,
	           at<"type"_>(event),
	           at<"sender"_>(event),
	           at<"room_id"_>(event),
	           at<"event_id"_>(event));

	m::vm::eval(event);
}
catch(const ed25519::bad_sig &e)
{
	throw m::BAD_SIGNATURE
	{
		":%s %s %s %s",
		at<"origin"_>(request),
		at<"room_id"_>(event),
		at<"event_id"_>(event),
		e.what()
	};
}

void
handle_pdu_failure(client &client,
                   const resource::request::object<m::txn> &request,
                   const string_view &txn_id,
                   const json::object &pdu_failure)
{
	log::debug("%s | %s | (pdu_failure) %s",
	           at<"origin"_>(request),
	           txn_id,
	           pdu_failure.get("sender", string_view{"*"}),
	           string_view{pdu_failure});
}

resource::response
handle_put(client &client,
           const resource::request::object<m::txn> &request)
{
	const auto txn_id
	{
		request.parv[0]
	};

	const string_view &origin
	{
		json::at<"origin"_>(request)
	};

	const json::array &edus
	{
		json::get<"edus"_>(request)
	};

	const json::array &pdus
	{
		json::get<"pdus"_>(request)
	};

	const json::array &pdu_failures
	{
		json::get<"pdu_failures"_>(request)
	};

	for(const auto &pdu_failure : pdu_failures)
		handle_pdu_failure(client, request, txn_id, pdu_failure);

	for(const auto &edu : edus)
		handle_edu(client, request, txn_id, edu);

	for(const auto &pdu : pdus)
		handle_pdu(client, request, txn_id, m::event{pdu});

	log::debug("%s | %s | %s --> edus:%zu pdus:%zu errors:%zu",
	           origin,
	           txn_id,
	           string(remote(client)),
	           edus.count(),
	           pdus.count(),
	           pdu_failures.count());

	return resource::response
	{
		client, json::members{}
	};
}

resource::method method_put
{
	send_resource, "PUT", handle_put,
	{
		method_put.VERIFY_ORIGIN
	}
};
