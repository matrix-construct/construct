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
	struct display;
	struct transform;

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

	extern conf::item<uint64_t> limit_ticks;
	extern conf::item<uint64_t> limit_cycles;
	extern conf::item<uint64_t> yield_threshold;
	extern conf::item<uint64_t> yield_interval;
	extern log::log log;
}

struct ircd::magick::display
{
	display(const ::ImageInfo &, ::Image &);
	display(const const_buffer &);
};

struct ircd::magick::transform
{
	using input = std::tuple<const ::ImageInfo &, const ::Image *>;
	using output = std::function<void (const const_buffer &)>;
	using transformer = std::function<::Image *(const input &)>;

	transform(const const_buffer &, const output &, const transformer &);
};

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

decltype(ircd::magick::limit_ticks)
ircd::magick::limit_ticks
{
	{ "name",    "ircd.magick.limit.ticks" },
	{ "default", 10000L                    },
};

decltype(ircd::magick::limit_cycles)
ircd::magick::limit_cycles
{
	{ "name",    "ircd.magick.limit.cycles" },
	{ "default", 0L                         },
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

decltype(ircd::magick::version_api)
ircd::magick::version_api
{
	"magick", info::versions::API, MagickLibVersion, {0}, MagickLibVersionText
};

decltype(ircd::magick::version_abi)
ircd::magick::version_abi
{
	"magick", info::versions::ABI, 0, {0}, []
	(auto &version, const auto &buf)
	{
		ulong monotonic(0);
		strlcpy(buf, ::GetMagickVersion(&monotonic));
		version.monotonic = monotonic;
	}
};

//
// init
//

void
ircd::magick::init()
{
	log::info
	{
		log, "Initializing Magick Library version API:%lu [%s] ABI:%lu [%s]",
		long(version_api),
		string_view{version_api},
		long(version_abi),
		string_view{version_abi},
	};

	if(long(version_api) != long(version_abi))
		log::warning
		{
			log, "Magick Library version mismatch headers:%lu library:%lu",
			long(version_api),
			long(version_abi),
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

//
// thumbcrop
//

ircd::magick::thumbcrop::thumbcrop(const const_buffer &in,
                                   const dimensions &req,
                                   const result_closure &out)
{
	crop::offset offset;
	const auto scaler{[&req, &offset]
	(const auto &image)
	{
		const auto &img_p
		{
			std::get<const ::Image *>(image)
		};

		const auto &req_x(req.first);
		const auto &req_y(req.second);
		const auto &img_x(img_p->columns);
		const auto &img_y(img_p->rows);

		const bool aspect
		{
			req_x * img_y < req_y * img_x
		};

		const dimensions scaled
		{
			aspect? req_y * img_x / img_y : req_x,
			aspect? req_y : req_x * img_y / img_x,
		};

		offset =
		{
			aspect? (scaled.first - req_x) / 2.0 : 0,
			aspect? 0 : (scaled.second - req_y) / 2.0,
		};

		return callex<::Image *>(::ThumbnailImage, img_p, scaled.first, scaled.second);
	}};

	const auto cropper{[&req, &out, &offset]
	(const const_buffer &in)
	{
		crop
		{
			in, req, offset, out
		};
	}};

	transform
	{
		in, cropper, scaler
	};
}

//
// thumbnail
//

ircd::magick::thumbnail::thumbnail(const const_buffer &in,
                                   const dimensions &dim,
                                   const result_closure &out)
{
	transform
	{
		in, out, [&dim](const auto &image)
		{
			return callex<::Image *>(::ThumbnailImage, std::get<const ::Image *>(image), dim.first, dim.second);
		}
	};
}

//
// scale
//

ircd::magick::scale::scale(const const_buffer &in,
                           const dimensions &dim,
                           const result_closure &out)
{
	transform
	{
		in, out, [&dim](const auto &image)
		{
			return callex<::Image *>(::ScaleImage, std::get<const ::Image *>(image), dim.first, dim.second);
		}
	};
}

//
// shave
//

ircd::magick::shave::shave(const const_buffer &in,
                           const dimensions &dim,
                           const offset &off,
                           const result_closure &out)
{
	const ::RectangleInfo geometry
	{
		dim.first,   // width
		dim.second,  // height
		off.first,   // x
		off.second,  // y
	};

	transform
	{
		in, out, [&geometry](const auto &image)
		{
			return callex<::Image *>(::ShaveImage, std::get<const ::Image *>(image), &geometry);
		}
	};
}

//
// crop
//

ircd::magick::crop::crop(const const_buffer &in,
                         const dimensions &dim,
                         const offset &off,
                         const result_closure &out)
{
	const ::RectangleInfo geometry
	{
		dim.first,   // width
		dim.second,  // height
		off.first,   // x
		off.second,  // y
	};

	transform
	{
		in, out, [&geometry](const auto &image)
		{
			return callex<::Image *>(::CropImage, std::get<const ::Image *>(image), &geometry);
		}
	};
}

//
// transform (internal)
//

ircd::magick::transform::transform(const const_buffer &input,
                                   const output &output,
                                   const transformer &transformer)
{
	const custom_ptr<::ImageInfo> input_info
	{
		::CloneImageInfo(nullptr),
		::DestroyImageInfo
	};

	const custom_ptr<::ImageInfo> output_info
	{
		::CloneImageInfo(nullptr),
		::DestroyImageInfo
	};

	const custom_ptr<::Image> input_image
	{
		callex<::Image *>(::BlobToImage, input_info.get(), data(input), size(input)),
		::DestroyImage // pollock
	};

	const custom_ptr<::Image> output_image
	{
		transformer({*input_info, input_image.get()}),
		::DestroyImage
	};

	size_t output_size(0);
	const auto output_data
	{
		callex<void *>(::ImageToBlob, output_info.get(), output_image.get(), &output_size)
	};

	const const_buffer result
	{
		reinterpret_cast<char *>(output_data), output_size
	};

	output(result);
}

//
// display (internal)
//

ircd::magick::display::display(const const_buffer &input)
{
	const custom_ptr<::ImageInfo> input_info
	{
		::CloneImageInfo(nullptr),
		::DestroyImageInfo
	};

	const custom_ptr<::Image> input_image
	{
		callex<::Image *>(::BlobToImage, input_info.get(), data(input), size(input)),
		::DestroyImage // pollock
	};

	display
	{
		*input_info, *input_image
	};
}

ircd::magick::display::display(const ::ImageInfo &info,
                               ::Image &image)
{
	callpf(::DisplayImages, &info, &image);
}

//
// util (internal)
//

namespace ircd::magick
{
	// It is likely that we can't have two contexts enter libmagick
	// simultaneously. This race is possible if the progress callback yields
	// and another context starts an operation. It is highly unlikely the lib
	// can handle reentrancy on the same thread. Hitting thread mutexes within
	// magick will also be catastrophic to ircd::ctx.
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
	if(!call<MagickPassFail>(f, std::forward<args>(a)...))
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

//
// ircd::magick::job
//

namespace ircd::magick
{
	static string_view loghead(const job &);
	static void job_init(const string_view &, const int64_t &, const uint64_t &, const uint64_t &);
	static void finished(job &);
	static bool check_yield(job &);
	static void check_cycles(job &);
}

struct ircd::magick::job::state
{
	uint64_t cycles {0};
	uint64_t yield {0};
	char description[1024];
}
thread_local ircd::magick::job::state;

decltype(ircd::magick::job::cur) thread_local
ircd::magick::job::cur;

decltype(ircd::magick::job::tot) thread_local
ircd::magick::job::tot;

uint
ircd::magick::handle_progress(const char *const text,
                              const int64_t tick,
                              const uint64_t ticks,
                              ExceptionInfo *ei)
noexcept try
{
	// Sample the current reference cycle count first and once. This is an
	// accumulated cycle count for only this ircd::ctx and the current slice,
	// (all other cycles are not accumulated here) which is non-zero by now
	// and monotonically increases across jobs as well.
	const auto cycles_sample
	{
		cycles(ctx::cur()) + ctx::prof::cur_slice_cycles()
	};

	// Detect if this is a new job. Tick is usually zero for a new job, but for
	// large jobs it may start after 0. Tick always appears monotonic for a job.
	// The ticks appears constant for a job, though could be the same for different
	// jobs. We don't know of any succinct way to test for a new job, so we use all
	// of the above information.
	const bool new_job
	{
		tick == 0
		|| tick < job::cur.tick
		|| ticks != job::cur.ticks
	};

	// Assert general assumptions about invocations of this callback.
	assert(new_job || tick >= job::cur.tick);
	assert(new_job || ticks == job::cur.ticks);

	// Branch after detecting this callback is unrelated to the last job.
	if(new_job)
	{
		finished(job::cur);
		job_init(text, tick, ticks, cycles_sample);
	}

	// Unconditional bookkeeping updates for this invocation. These statements
	// behave properly regardless of whether this is the same or a new job.
	assert(cycles_sample >= job::state.cycles);
	job::cur.cycles += cycles_sample - job::state.cycles;
	job::state.cycles = cycles_sample;
	job::cur.tick = tick;

	// This debug message is very noisy, even for debug mode. Developer can
	// enable it at their discretion.
	#ifdef IRCD_MAGICK_DEBUG_PROGRESS
	log::debug
	{
		log, "job:%lu progress %2.2lf%% (%ld/%ld) cycles:%lu :%s",
		job::cur.id,
		(job::cur.tick / double(job::cur.ticks) * 100.0),
		job::cur.tick,
		job::cur.ticks,
		job::cur.cycles,
		job::cur.text,
	};
	#endif

	check_cycles(job::cur);
	check_yield(job::cur);

	return true;
}
catch(const ctx::interrupted &e)
{
	++job::cur.intrs;
	job::cur.eptr = std::current_exception();
	::ThrowException(ei, MonitorError, "interrupted", e.what());
	ei->signature = MagickSignature; // ???
	return false;
}
catch(const ctx::terminated &)
{
	++job::cur.intrs;
	job::cur.eptr = std::current_exception();
	::ThrowException(ei, MonitorError, "terminated", nullptr);
	ei->signature = MagickSignature; // ???
	return false;
}
catch(const std::exception &e)
{
	++job::cur.errors;
	job::cur.eptr = std::current_exception();
	::ThrowLoggedException(ei, MonitorError, "error", e.what(), __FILE__, __FUNCTION__, __LINE__);
	ei->signature = MagickSignature; // ???
	return false;
}
catch(...)
{
	++job::cur.errors;
	job::cur.eptr = std::current_exception();
	::ThrowLoggedException(ei, MonitorFatalError, "unknown", nullptr, __FILE__, __FUNCTION__, __LINE__);
	ei->signature = MagickSignature; // ???
	return false;
}

void
ircd::magick::check_cycles(job &job)
{
	const uint64_t &limit_cycles
	{
		magick::limit_cycles
	};

	// Check if job exceeded its reference cycle limit if enabled.
	if(unlikely(limit_cycles && job.cycles > limit_cycles))
		throw error
		{
			"job:%lu CPU cycles:%lu exceeded server limit:%lu (progress %2.2lf%% (%ld/%ld))",
			job.id,
			job.cycles,
			limit_cycles,
			(job.tick / double(job.ticks) * 100.0),
			job.tick,
			job.ticks,
		};
}

bool
ircd::magick::check_yield(job &job)
{
	const uint64_t &yield_threshold
	{
		magick::yield_threshold
	};

	// This job is too small to conduct any yields.
	if(likely(job.ticks < yield_threshold))
		return false;

	const uint64_t &yield_interval
	{
		magick::yield_interval
	};

	// Haven't reached the yield interval yet.
	if(likely(job.tick - job::state.yield <= yield_interval))
		return false;

	job::state.yield = job.tick;
	ctx::yield();
	return true;
}

void
ircd::magick::finished(job &job)
{
	// Update total state from last job
	assert(job.id == job::tot.id + 1 || (job.id == job::tot.id && !job.id));
	job::tot.id = job.id;
	job::tot.tick += job.tick;
	job::tot.ticks += job.ticks;
	job::tot.cycles += job.cycles;
	job::tot.yields += job.yields;
	job::tot.intrs += job.intrs;
	job::tot.errors += job.errors;
}

void
ircd::magick::job_init(const string_view &text,
                       const int64_t &tick,
                       const uint64_t &ticks,
                       const uint64_t &cycles_sample)
{
	// Reset the current job structure
	job::cur =
	{
		job::tot.id + 1,   // id
		tick,              // tick
		ticks,             // ticks
	};

	// Update internal state
	job::state.cycles = cycles_sample;

	// The description text may have this annoying empty "[]" on this
	// message so we'll strip that here.
	job::cur.description = strlcpy
	{
		job::state.description, lstrip(text, "[] ")
	};

	log::debug
	{
		log, "job:%lu started; ticks:%lu :%s",
		job::cur.id,
		job::cur.ticks,
		job::cur.description,
	};

	// This job is too large based on the ticks measurement. This is an ad hoc
	// measurement of the job size created internally by ImageMagick.
	if(job::cur.ticks > uint64_t(limit_ticks))
		throw error
		{
			"job:%lu computation ticks:%lu exceeds server limit:%lu :%s",
			job::cur.id,
			job::cur.ticks,
			uint64_t(limit_ticks),
			job::cur.description,
		};
}

ircd::string_view
ircd::magick::loghead(const job &job)
{
	thread_local char buf[256];
	return fmt::sprintf
	{
		buf, "job:%lu %ld/%lu [%s]",
		job.id,
		job.tick,
		job.ticks,
		job.description,
	};
}

//
// (Internal) patch panels
//

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
		log, "%s (%d) %s :%s",
		loghead(job::cur),
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
		log, "%s (#%d) %s :%s :%s",
		loghead(job::cur),
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
		log, "%s (#%d) %s :%s :%s",
		loghead(job::cur),
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
		log, "%s (#%d) %s :%s :%s",
		loghead(job::cur),
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
		log, "%s %s",
		loghead(job::cur),
		what,
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
