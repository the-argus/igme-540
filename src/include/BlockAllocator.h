#pragma once

#include <span>
#include "short_numbers.h"

namespace ggp
{
	class BlockAllocator
	{
	public:
		struct Options {
			size_t maxBytes;
			size_t initialBytes;
			size_t blockSize;
		};

		BlockAllocator(const BlockAllocator&) = delete;
		BlockAllocator& operator=(const BlockAllocator&) = delete;

		BlockAllocator(BlockAllocator&&) noexcept;
		BlockAllocator&& operator=(BlockAllocator&&) noexcept;

		BlockAllocator(const Options&) noexcept;

		~BlockAllocator() noexcept;

		std::span<u8> alloc() noexcept;
		void free(std::span<u8> mem) noexcept;

	private:

		struct EmptyBlock
		{
			u64 nextEmpty;
		};

		// returns true if capacity grew, or false if already at max
		bool growCapacity() noexcept;
		EmptyBlock* getBlockAt(size_t i) const noexcept;

		std::span<u8> m_memory;
		std::span<u8> m_reservedMemory;
		size_t m_pageSize;
		size_t m_blockSize;
		size_t m_blocksFree;
		size_t m_lastFree;
	};
}