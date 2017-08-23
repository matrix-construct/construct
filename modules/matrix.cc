/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

#include <ircd/listen.h>

using namespace ircd;

mapi::header IRCD_MODULE
{
	"Chat Matrix Protocol"
};

std::map<std::string, module> modules;

 //TODO: XXX very temporary stuff around here
// The path root (serves static assets for web etc); this pointer
// exists for now to easily find and reload that specifically.
module *root_module;
const auto _init_([]
{
	// These modules host databases and have to be loaded first.
	modules.emplace("client_account.so"s, "client_account.so"s);
	modules.emplace("client_room.so"s, "client_room.so"s);

	for(const auto &name : mods::available())
		if(name != "matrix.so")
			modules.emplace(name, name);

	root_module = &modules.at("root.so"s);
	return true;
}());

listener matrices
{
	std::string { json::obj
	{
		{ "name",    "Chat Matrix" },
		{ "host",    "0.0.0.0"     },
		{
			"ssl_certificate_chain_file", "/home/jason/.synapse/zemos.net.crt"
		},
		{
			"ssl_tmp_dh_file", "/home/jason/.synapse/cdc.z.tls.dh"
		},
		{
			"ssl_private_key_file_pem", "/home/jason/.synapse/cdc.z.tls.key"
		},
		{
			"port", 6667
		}
	}}
};
