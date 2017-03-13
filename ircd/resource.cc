/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

namespace ircd {

IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(resource::resources)
resource::resources
{};

} // namespace ircd


ircd::resource::resource(const char *const &name,
                         const char *const &description)
:name{name}
,description{description}
,resources_it{[this, &name]
{
	const auto iit(resources.emplace(this->name, this));
	if(!iit.second)
		throw error("resource \"%s\" already registered", name);

	return iit.first;
}()}
{
	log::info("Registered resource \"%s\" by handler @ %p", name, this);
}

ircd::resource::~resource()
noexcept
{
	resources.erase(resources_it);
	log::info("Unregistered resource \"%s\" by handler @ %p", name, this);
}

void
ircd::resource::operator()(client &client,
                           parse::context &pc,
                           const http::request::head &head)
const try
{
	const auto &method(*methods.at(head.method));
	http::request::content content{pc, head};
	resource::request request
	{
		head, content
	};

	method(client, request);
}
catch(const std::out_of_range &e)
{
	throw http::error(http::METHOD_NOT_ALLOWED);
}

ircd::resource::method::method(struct resource &resource,
                               const char *const &name,
                               const handler &handler,
                               const std::initializer_list<member *> &members)
:function{handler}
,name{name}
,resource{&resource}
,methods_it{[this, &name]
{
	const auto iit(this->resource->methods.emplace(this->name, this));
	if(!iit.second)
		throw error("resource \"%s\" already registered", name);

	return iit.first;
}()}
{
	for(const auto &member : members)
		this->members.emplace(member->name, member);
}

ircd::resource::method::~method()
noexcept
{
	resource->methods.erase(methods_it);
}

ircd::resource::response::response(client &client,
                                   const json::obj &doc,
                                   const http::code &code)
{
	char cbuf[1024];
	response(client, serialize(doc, cbuf, cbuf + sizeof(cbuf)), code);
}

ircd::resource::response::response(client &client,
                                   const json::doc &doc,
                                   const http::code &code)
{
	http::response
	{
		code, doc, write_closure(client),
		{
			{ "Content-Type", "application/json" }
		}
	};
}

ircd::resource::response::~response()
noexcept
{
}

ircd::resource::member::member(const char *const &name,
                               const enum json::type &type,
                               validator valid)
:name{name}
,type{type}
,valid{std::move(valid)}
{
}
