// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MAGICK_API_H

namespace ircd::magick
{
	static void handle_exception(const ::ExceptionType, const char *, const char *);
	static void handle_fatal(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_error(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_warning(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_log(const ::ExceptionType, const char *) noexcept;
	static void *handle_realloc(void *, size_t) noexcept;
	static void *handle_malloc(size_t) noexcept;
	static void handle_free(void *) noexcept;
	static uint handle_progress(const char *, const int64_t, const uint64_t, ExceptionInfo *) noexcept;

	template<class R, class F, class... A> static R call(F&&, A&&...);
	template<class R, class F, class... A> static R callex(F&&, A&&...);
	template<class F, class... A> static void callpf(F&&, A&&...);

	static void init();
	static void fini();

	extern conf::item<uint64_t> yield_threshold;
	extern conf::item<uint64_t> yield_interval;
	extern log::log log;
}

ircd::mapi::header
IRCD_MODULE
{
	"GraphicsMagick Library support for media manipulation",
	ircd::magick::init,
	ircd::magick::fini
};

decltype(ircd::magick::log)
ircd::magick::log
{
	"magick"
};

decltype(ircd::magick::yield_threshold)
ircd::magick::yield_threshold
{
	{ "name",    "ircd.magick.yield.threshold" },
	{ "default", 1000L                         },
};

decltype(ircd::magick::yield_interval)
ircd::magick::yield_interval
{
	{ "name",    "ircd.magick.yield.interval" },
	{ "default", 768L                         },
};

//
// init
//

void
ircd::magick::init()
{
	const auto version
	{
		magick::version()
	};

	log::debug
	{
		log, "Initializing Magick Library version inc:%lu [%s] lib:%lu [%s]",
		ulong(MagickLibVersion),
		MagickLibVersionText,
		std::get<0>(version),
		std::get<1>(version),
	};

	if(std::get<0>(version) != ulong(MagickLibVersion))
		log::warning
		{
			log, "Magick Library version mismatch headers:%lu library:%lu",
			ulong(MagickLibVersion),
			std::get<0>(version),
		};

	::MagickAllocFunctions(handle_free, handle_malloc, handle_realloc);
	::SetFatalErrorHandler(handle_fatal);
	::SetErrorHandler(handle_error);
	::SetWarningHandler(handle_warning);
	::InitializeMagick(nullptr);
	::SetLogMethod(handle_log);
	//::SetLogEventMask("all"); // Pollutes stderr :/ can't fix
	::SetMonitorHandler(handle_progress);
	::SetMagickResourceLimit(ThreadsResource, 1UL);

	log::debug
	{
		log, "resource settings: pixel max:%lu:%lu height:%lu:%lu width:%lu:%lu; threads:%lu:%lu",
		::GetMagickResource(PixelsResource),
		::GetMagickResourceLimit(PixelsResource),
		::GetMagickResource(HeightResource),
		::GetMagickResourceLimit(HeightResource),
		::GetMagickResource(WidthResource),
		::GetMagickResourceLimit(WidthResource),
		::GetMagickResource(ThreadsResource),
		::GetMagickResourceLimit(ThreadsResource),
	};
}

void
ircd::magick::fini()
{
	log::debug
	{
		"Shutting down Magick Library..."
	};

	::DestroyMagick();
}

std::tuple<ulong, ircd::string_view>
ircd::magick::version()
{
	ulong number(0);
	const char *const string
	{
		::GetMagickVersion(&number)
	};

	return { number, string };
}

//
// thumbnail
//

ircd::magick::thumbnail::thumbnail(const const_buffer &in,
                                   const dimensions &xy,
                                   const result_closure &closure)
{
	const custom_ptr<::ImageInfo> input_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::ImageInfo> output_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::Image> input
	{
		callex<::Image *>(::BlobToImage, input_info.get(), data(in), size(in)), ::DestroyImage // pollock
	};

	const custom_ptr<::Image> output
	{
		callex<::Image *>(::ThumbnailImage, input.get(), xy.first, xy.second), ::DestroyImage
	};

	size_t output_size(0);
	const auto output_data
	{
		callex<void *>(::ImageToBlob, output_info.get(), output.get(), &output_size)
	};

	const const_buffer result
	{
		reinterpret_cast<char *>(output_data), output_size
	};

	closure(result);
}

//
// crop
//

ircd::magick::crop::crop(const const_buffer &in,
                         const dimensions &dim,
                         const offset &off,
                         const result_closure &closure)
{
	const custom_ptr<::ImageInfo> input_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::ImageInfo> output_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::Image> input
	{
		callex<::Image *>(::BlobToImage, input_info.get(), data(in), size(in)), ::DestroyImage
	};

	// Note: negative offsets appear to just reduce the dimension in my limited test
	const ::RectangleInfo geometry
	{
		dim.first,   // width
		dim.second,  // height
		off.first,   // x
		off.second,  // y
	};

	const custom_ptr<::Image> output
	{
		callex<::Image *>(::CropImage, input.get(), &geometry), ::DestroyImage
	};

	size_t output_size(0);
	const auto output_data
	{
		callex<void *>(::ImageToBlob, output_info.get(), output.get(), &output_size)
	};

	const const_buffer result
	{
		reinterpret_cast<char *>(output_data), output_size
	};

	closure(result);
}

//
// util
//

namespace ircd::magick
{
	// It is likely that we can't have two contexts enter libmagick
	// simultaneously. This race is possible if the progress callback yields
	// and another context starts an operation. It is highly unlikely the lib
	// can handle reentrancy on the same thread. Hitting thread mutexes within
	// magick will also be catestrophic to ircd::ctx.
	ctx::mutex call_mutex;
}

template<class return_t,
         class function,
         class... args>
return_t
ircd::magick::callex(function&& f,
                     args&&... a)
{
	const std::lock_guard lock
	{
		call_mutex
	};

	::ExceptionInfo ei;
	GetExceptionInfo(&ei); // initializer
	const unwind destroy{[&ei]
	{
		::DestroyExceptionInfo(&ei);
	}};

	const auto ret
	{
		f(std::forward<args>(a)..., &ei)
	};

	const auto their_handler
	{
		::SetErrorHandler(handle_exception)
	};

	const unwind reset{[&their_handler]
	{
		::SetErrorHandler(their_handler);
	}};

	// exception comes out of here; if this is not safe we'll have to
	// convey with a global or inspect ExceptionInfo manually.
	::CatchException(&ei);
	return ret;
}

template<class function,
         class... args>
void
ircd::magick::callpf(function&& f,
                     args&&... a)
{
	if(!call(f, std::forward<args>(a)...))
		throw error{};
}

template<class return_t,
         class function,
         class... args>
return_t
ircd::magick::call(function&& f,
                   args&&... a)
{
	const std::lock_guard lock
	{
		call_mutex
	};

	return f(std::forward<args>(a)...);
}

namespace ircd::magick
{
	static thread_local uint64_t job_ctr;
	static thread_local int64_t last_quantum;
	static thread_local uint64_t last_yield;
}

uint
ircd::magick::handle_progress(const char *text,
                              const int64_t quantum,
                              const uint64_t span,
                              ExceptionInfo *ei)
noexcept try
{
	// This is a new job; reset any global state here
	if(quantum == 0)
	{
		++job_ctr;
		last_quantum = 0;
		last_yield = 0;
	}

	#ifdef IRCD_MAGICK_DEBUG_PROGRESS
	log::debug
	{
		log, "job:%lu progress %2.2lf pct (%ld/%ld) :%s",
		job_ctr,
		(quantum / double(span) * 100.0),
		quantum,
		span,
		text,
	};
	#endif

	// This is a larger job; we yield this ircd::ctx at interval
	if(span > uint64_t(yield_threshold))
	{
		if(quantum - last_yield > uint64_t(yield_interval))
		{
			last_yield = quantum;
			ctx::yield();
		}
	}

	last_quantum = quantum;

	// return false to interrupt the job; set the exception saying why.
	//
	// If MonitorEvent (or any *Event) is the code the interruption is
	// not an error and the operation will silently complete, possibly with
	// incomplete or corrupt results (i guess? this might be ok for raster
	// or optimization operations maybe which can go on indefinitely)
	//
	// If MonitorError (or any *Error) is the code we propagate the exception
	// all the way back through our user.
	//
	return true;
}
catch(const ctx::interrupted &e)
{
	::ThrowException(ei, MonitorError, "interrupted", e.what());
	ei->signature = MagickSignature; // ???
	return false;
}
catch(const ctx::terminated &)
{
	::ThrowException(ei, MonitorError, "terminated", nullptr);
	ei->signature = MagickSignature; // ???
	return false;
}
catch(const std::exception &e)
{
	::ThrowLoggedException(ei, MonitorError, "error", e.what(), __FILE__, __FUNCTION__, __LINE__);
	ei->signature = MagickSignature; // ???
	return false;
}
catch(...)
{
	::ThrowLoggedException(ei, MonitorFatalError, "unknown", nullptr, __FILE__, __FUNCTION__, __LINE__);
	ei->signature = MagickSignature; // ???
	return false;
}

void
ircd::magick::handle_free(void *const ptr)
noexcept
{
	std::free(ptr);
}

void *
ircd::magick::handle_malloc(size_t size)
noexcept
{
	return std::malloc(size);
}

void *
ircd::magick::handle_realloc(void *const ptr,
                             size_t size)
noexcept
{
	return std::realloc(ptr, size);
}

void
ircd::magick::handle_log(const ::ExceptionType type,
                         const char *const message)
noexcept
{
	log::debug
	{
		log, "(%d) %s :%s",
		int(type),
		::GetLocaleExceptionMessage(type, ""),
		message,
	};
}

void
ircd::magick::handle_warning(const ::ExceptionType type,
                             const char *const reason,
                             const char *const description)
noexcept
{
	log::warning
	{
		log, "(#%d) %s :%s :%s",
		int(type),
		::GetLocaleExceptionMessage(type, ""),
		reason,
		description,
	};
}

void
ircd::magick::handle_error(const ::ExceptionType type,
                           const char *const reason,
                           const char *const description)
noexcept
{
	log::error
	{
		log, "(#%d) %s :%s :%s",
		int(type),
		::GetLocaleExceptionMessage(type, ""),
		reason,
		description,
	};
}

[[noreturn]] void
ircd::magick::handle_fatal(const ::ExceptionType type,
                           const char *const reason,
                           const char *const description)
noexcept
{
	log::critical
	{
		log, "(#%d) %s :%s :%s",
		int(type),
		::GetLocaleExceptionMessage(type, ""),
		reason,
		description,
	};

	ircd::terminate();
}

[[noreturn]] void
ircd::magick::handle_exception(const ::ExceptionType type,
                               const char *const reason,
                               const char *const description)
{
	const auto &message
	{
		::GetLocaleExceptionMessage(type, "")?: "???"
	};

	thread_local char buf[exception::BUFSIZE];
	const string_view what{fmt::sprintf
	{
		buf, "(#%d) %s :%s :%s",
		int(type),
		message,
		reason,
		description,
	}};

	log::derror
	{
		log, "%s", what
	};

	if(reason == "terminated"_sv)
		throw ctx::terminated{};

	if(reason == "interrupted"_sv)
		throw ctx::interrupted
		{
			"%s", what
		};

	throw error
	{
		"%s", what
	};
}
