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

using namespace ircd;

mapi::header IRCD_MODULE
{
	"L-Line - configuration instruction for loadmodule"
};

struct L
:conf::top
{
	void set(client::client &, std::string label, std::string key, std::string val) override;
	void del(client::client &, const std::string &label, const std::string &key) override;

	using conf::top::top;
}

static L
{
	'L', "loadmodule"
};

void
L::set(client::client &client,
       std::string label,
       std::string key,
       std::string val)
try
{
	conf::log.debug("Loading \"%s\" via L-Line instruction", label.c_str());
	mods::load(label);
}
catch(const std::exception &e)
{
	conf::log.error("L-Line \"%s\": %s",
	                label.c_str(),
	                e.what());
	throw;
}

void
L::del(client::client &client,
       const std::string &label,
       const std::string &key)
try
{
	conf::log.debug("Unloading \"%s\" via L-Line instruction", label.c_str());
	mods::unload(label);
}
catch(const std::exception &e)
{
	conf::log.error("L-Line \"%s\": %s",
	                label.c_str(),
	                e.what());
	throw;
}
