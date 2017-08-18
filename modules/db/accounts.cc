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

const database::descriptor accounts_token_descriptor
{
	"token",
	"An index of access_token to user_id",
	{
		// readable key      // readable value
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor accounts_registered_descriptor
{
	"registered",
	"A UNIX epoch timestamp sampled when the account was created.",
	{
		// readable key      // binary value
		typeid(string_view), typeid(time_t)
	}
};

const database::description accounts_description
{
	{ "default"                                  },
	accounts_token_descriptor,
	accounts_registered_descriptor,
	{ "access_token"                             },
	{ "access_token.text"                        },
	{ "password"                                 },
	{ "password.text"                            },
	{ "password.hash"                            },
	{ "password.hash.sha256"                     },
};

std::shared_ptr<database> accounts_database
{
	std::make_shared<database>("accounts"s, ""s, accounts_description)
};

mapi::header IRCD_MODULE
{
	"Hosts the 'accounts' database"
};

