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

#pragma once
#define HAVE_IRCD_MODS_MODS_H

namespace ircd {
namespace mods {

std::vector<std::string> symbols(const std::string &fullpath, const std::string &section);
std::vector<std::string> symbols(const std::string &fullpath);
std::vector<std::string> sections(const std::string &fullpath);

// Find module names where symbol resides
bool has_symbol(const std::string &name, const std::string &symbol);
std::vector<std::string> find_symbol(const std::string &symbol);

// returns dir/name of first dir containing 'name' (and this will be a loadable module)
// Unlike libltdl, the reason each individual candidate failed is presented in a vector.
std::string search(const std::string &name, std::vector<std::string> &why);
std::string search(const std::string &name);

// Potential modules available to load
std::forward_list<std::string> available();
bool available(const std::string &name);
bool loaded(const std::string &name);

} // namespace mods
} // namespace ircd
