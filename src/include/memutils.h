#pragma once

#include <span>
#include "errors.h"

namespace ggp
{
	/// <summary>
	/// Check if a given span of things entirely contains another span of the same thing.
	/// It is inclusive, so if the spans are the same it will return true.
	/// </summary>
	template <typename T>
	inline constexpr bool memcontains(std::span<T> outer, std::span<T> inner)
	{
		return inner.data() >= outer.data() && outer.data() + outer.size() >= inner.data() + inner.size();
	}

	/// <summary>
	/// Check if an object is contained within a contiguous area of memory.
	/// </summary>
	template <typename T>
	inline constexpr bool is_inbounds_bytes(std::span<uint8_t> mem, T* ptr)
	{
		return (reinterpret_cast<uint8_t*>(ptr + 1) <= mem.data() + mem.size()) && reinterpret_cast<uint8_t*>(ptr) >= mem.data();
	}

	/// <summary>
	/// Check if a memory address is divisible by a number.
	/// </summary>
	inline constexpr bool is_aligned(void* ptr, size_t align)
	{
		return (uintptr_t)ptr % align == 0;
	}

	/// <summary>
	/// Check if a pointer to an object is correctly aligned for the required alignment of the type given by the pointer.
	/// </summary>
	template <typename T>
	inline constexpr bool is_aligned_to_type(T* ptr)
	{
		return (uintptr_t)ptr % (alignof(T)) == 0;
	}

	// version of round_up_to_multiple_of which is done at runtime
	inline constexpr size_t rround_up_to_multiple_of(size_t size, size_t multiple)
	{
		gassert(size != 0);
		gassert(multiple != 0);
		return (((size - 1) / multiple) + 1) * multiple;
	}

	template <size_t align>
	inline constexpr size_t round_up_to_multiple_of(size_t size)
	{
		static_assert(align != 0, "Cannot align to multiple of zero.");
		if (size == 0) [[unlikely]] return 0;
		return (((size - 1) / align) + 1) * align;
	}

	template <typename T, size_t align>
	struct alignsize
	{
		static constexpr size_t value = round_up_to_multiple_of<align>(sizeof T);
	};

	inline size_t operator""_GB(size_t const x)
	{
		return 1024L * 1024L * 1024L * x;
	}

	inline size_t operator""_MB(size_t const x)
	{
		return 1024L * 1024L * x;
	}

	static_assert(round_up_to_multiple_of<16>(0) == 0);
	static_assert(round_up_to_multiple_of<16>(15) == 16);
	static_assert(round_up_to_multiple_of<16>(16) == 16);
	static_assert(round_up_to_multiple_of<16>(32) == 32);
	static_assert(round_up_to_multiple_of<16>(33) == 48);

	static_assert(round_up_to_multiple_of<1>(32) == 32);
	static_assert(round_up_to_multiple_of<1>(33) == 33);
	static_assert(round_up_to_multiple_of<1>(15) == 15);
	static_assert(round_up_to_multiple_of<1>(16) == 16);

	static_assert(alignsize<float, 64>::value == 64UL);
	static_assert(alignsize<double, 64>::value == 64UL);
	static_assert(alignsize<char, 64>::value == 64UL);
	static_assert(alignsize<float, 1>::value == sizeof(float));
	static_assert(alignsize<float, 4>::value == sizeof(float));
	static_assert(alignsize<float, 5>::value == 5); // weird
}