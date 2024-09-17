#pragma once

namespace ggp
{
	template <size_t align>
	inline constexpr size_t round_up_to_multiple_of(size_t size)
	{
		static_assert(align != 0, "Cannot align to multiple of zero.");
		if (size == 0) [[unlikely]]
			return 0;
		return (((size - 1) / align) + 1) * align;
	}

	template <typename T, size_t align>
	struct alignsize
	{
		static constexpr size_t value = round_up_to_multiple_of<align>(sizeof T);
	};

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