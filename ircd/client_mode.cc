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
using client::mode::mode;

mode_table<client::mode::mask> client::mode::table;
decltype(client::mode::available) client::mode::available;

mode client::mode::SERVNOTICE  { 's' };
mode client::mode::WALLOP      { 'w' };
mode client::mode::OPERWALL    { 'z' };
mode client::mode::INVISIBLE   { 'i' };
mode client::mode::CALLERID    { 'g' };
mode client::mode::LOCOPS      { 'l' };
mode client::mode::SERVICE     { 'S' };
mode client::mode::DEAF        { 'D' };
mode client::mode::NOFORWARD   { 'Q' };
mode client::mode::REGONLYMSG  { 'R' };
mode client::mode::OPER        { 'o' };
mode client::mode::ADMIN       { 'a' };
mode client::mode::SSLCLIENT   { 'Z' };

const client::mode::mask client::mode::DEFAULT_OPER_UMODES
{
	SERVNOTICE   |
	OPERWALL     |
	WALLOP       |
	LOCOPS
};

mode::mode(const char &c)
:mode_lease{c}
{
	mask_table(table, available);
}

void
mode::release()
noexcept
{
	table[char(*this)] = 0;
	mask_table(table, available);
}
