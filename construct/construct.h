// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// construct.cc
//

extern std::unique_ptr<boost::asio::io_service> ios;

//
// console.cc
//

void console_spawn();
void console_execute(const std::vector<std::string> &lines);
void console_cancel();
void console_hangup();
void console_termstop();
