#pragma once

#include <cstdio>
#include <cstdlib>
#include <cassert>

namespace ggp
{
	inline void gabort()
	{
#if defined(_MSC_VER) && (defined(DEBUG) || defined(_DEBUG))
		__debugbreak();
#endif
		std::abort();
	}

	// assert but it breaks in the debugger in vs
	constexpr inline void gassert(bool cond, const char* msg = "")
	{
#if defined(_MSC_VER)
#if defined(DEBUG) || defined(_DEBUG)
		if (!cond) {
			fprintf(stderr, "Debug Error: %s\n", msg);
			__debugbreak();
			std::abort();
		}
#endif
#else
		assert(cond);
#endif
	}

	inline void abort_if(bool condition, const char* message = "")
	{
		if (condition) [[unlikely]]
		{
			fprintf(stderr, "Unrecoverable Error: %s\n", message);
			gabort();
		}
	}
}