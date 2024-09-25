#pragma once

#include <span>
#include "short_numbers.h"
#include "errors.h"

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
		BlockAllocator& operator=(BlockAllocator&&) noexcept;

		BlockAllocator(const Options&) noexcept;

		~BlockAllocator() noexcept;

		std::span<u8> alloc() noexcept;
		void free(std::span<u8> mem) noexcept;

		/// <summary>
		/// Construct an object of type T within a block in this allocator.
		/// Similar to "new" et all. Calls T's constructor.
		/// </summary>
		/// <param name="...args">The arguments to pass to the constructor.</param>
		/// <returns>A pointer to the initialized memory, or nullptr if the requested type is too big to be allocated with this allocator.</returns>
		template <typename T, typename ...Args>
		inline T* create(Args&&... args) noexcept
		{
			static_assert(std::is_trivially_destructible_v<T>, "BlockAllocator::create will call constructor of a type which it cannot destruct");
			if (sizeof(T) > m_blockSize || alignof(T) > m_blockSize) [[unlikely]]
			{
				gassert(false, "Attempt to create type with block allocator, but its too big or too aligned");
				return nullptr;
			}
			
			T* const out = reinterpret_cast<T*>(alloc().data());
			// if no arguments were provided and T is trivially constructible, no need to do anything
			// and this function is a glorified assert
			if constexpr (sizeof...(Args) > 0 || !std::is_trivially_constructible_v<T>)
			{
				if (!out) [[unlikely]]
					return nullptr;
				new (out) T(std::forward<Args>(args)...);
			}
			return out;
		}

		template <typename T>
		inline void destroy(T* object) noexcept
		{
			free({ (u8*)object, m_blockSize } );
		}

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