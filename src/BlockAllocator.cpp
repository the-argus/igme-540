#include "BlockAllocator.h"
#include "memory_map.h"

ggp::BlockAllocator::BlockAllocator(const Options& options) noexcept
{
	m_pagesize = mm::get_page_size();
	// round max bytes to multiple of pages needed, but dont multiply by pagesize again so we just get number in pages
	size_t pagesReserved = (options.maxBytes + m_pagesize - 1) / m_pagesize;
	// same for initial bytes, this is how many we commit at the start
	size_t pagesCommitted = (options.initialBytes + m_pagesize - 1) / m_pagesize;

	const size_t maxPossibleBlocks = (pagesReserved * m_pagesize) / options.blockSize;
	assert(maxPossibleBlocks > 0);

	if (auto result = mm::reserve_pages(nullptr, pagesReserved); result.code != 0) {
		printf("ERROR: Failed to reserve memory for block allocator, errcode %ld\n", result.code);
		std::abort();
	}
	else {
		m_reservedMemory = std::span<u8>{ (u8*)result.data, result.bytes };
	
		if (pagesCommitted > 0) {
			if (auto commitResult = mm::commit_pages(result.data, pagesCommitted); commitResult == 0)
			{
				m_memory = std::span<u8>{ (u8*)result.data, pagesCommitted * m_pagesize };
			}
			else {
				printf("ERROR: Failed to commit memory for block allocator, errcode %ld\n", commitResult);
				std::abort();
			}
		}
		else {
			m_memory = std::span<u8>{ m_reservedMemory.data(), 0};
		}
	
		// memory allocated, now initialize if needed
		for (int i = 0; i < )
		}
	}

}

ggp::BlockAllocator::BlockAllocator(BlockAllocator&& other) noexcept
	:m_memory(std::exchange(other.m_memory, {})),
	m_reservedMemory(std::exchange(other.m_reservedMemory, {})),
	m_pagesize(other.m_pagesize)
{
}

auto ggp::BlockAllocator::operator=(BlockAllocator&& other) noexcept -> BlockAllocator&&
{
	m_memory = std::exchange(other.m_memory, {});
	m_reservedMemory = std::exchange(other.m_reservedMemory, {});
	m_pagesize = other.m_pagesize;
}

ggp::BlockAllocator::~BlockAllocator() noexcept
{
	mm::memory_unmap(m_reservedMemory.data(), m_reservedMemory.size_bytes());
}

std::span<u8> ggp::BlockAllocator::alloc() noexcept
{

}

void ggp::BlockAllocator::free(std::span<u8> mem) noexcept
{}
