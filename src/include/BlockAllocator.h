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
		std::span<u8> m_memory;
		std::span<u8> m_reservedMemory;
		size_t m_pagesize;
	};
}