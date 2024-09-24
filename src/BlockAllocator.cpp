#include "BlockAllocator.h"
#include "memory_map.h"
#include "memutils.h"

ggp::BlockAllocator::BlockAllocator(const Options& options) noexcept
{
	m_pageSize = mm::get_page_size();
	m_blockSize = max(options.blockSize, sizeof EmptyBlock); // NOTE: macro preventing me from using std::max? :(
	m_blockSize = round_up_to_multiple_of<alignof(EmptyBlock)>(m_blockSize);
	const size_t pagesReserved = rround_up_to_multiple_of(options.maxBytes, m_pageSize) / m_pageSize;
	const size_t bytesCommitted = rround_up_to_multiple_of(options.initialBytes, m_pageSize);
	const size_t pagesCommitted = bytesCommitted / m_pageSize;

	const size_t maxPossibleBlocks = (pagesReserved * m_pageSize) / m_blockSize;
	assert(maxPossibleBlocks > 0);

	if (auto result = mm::reserve_pages(nullptr, pagesReserved); result.code != 0)
	{
		printf("ERROR: Failed to reserve memory for block allocator, errcode %ld\n", result.code);
		std::abort();
	}
	else
	{
		m_reservedMemory = std::span<u8>{ (u8*)result.data, result.bytes };
	
		if (bytesCommitted > 0)
		{
			if (auto commitResult = mm::commit_pages(result.data, pagesCommitted); commitResult == 0)
			{
				m_memory = std::span<u8>{ (u8*)result.data, bytesCommitted };
			}
			else
			{
				printf("ERROR: Failed to commit memory for block allocator, errcode %ld\n", commitResult);
				std::abort();
			}
		}
		else
		{
			m_memory = std::span<u8>{ m_reservedMemory.data(), 0};
		}
	}

	// memory allocated, now initialize if needed
	const size_t initialBlocks = bytesCommitted / m_blockSize;
	assert(m_blockSize >= sizeof EmptyBlock);
	for (size_t i = 0; i < initialBlocks; ++i) {
		EmptyBlock* addr = reinterpret_cast<EmptyBlock*>(m_memory.data() + (i * m_blockSize));
		assert(is_aligned_to_type(addr));
		assert(is_inbounds(m_memory, addr));
		// write the address of the next available thing
		*addr = EmptyBlock{ .nextEmpty = i + 1 };
	}
	m_blocksFree = initialBlocks;
	m_lastFree = 0;
}

ggp::BlockAllocator::BlockAllocator(BlockAllocator&& other) noexcept
	:m_memory(std::exchange(other.m_memory, {})),
	m_reservedMemory(std::exchange(other.m_reservedMemory, {})),
	m_pageSize(other.m_pageSize),
	m_blocksFree(other.m_blocksFree),
	m_blockSize(other.m_blockSize),
	m_lastFree(other.m_lastFree)
{
}

auto ggp::BlockAllocator::operator=(BlockAllocator&& other) noexcept -> BlockAllocator&&
{
	m_memory = std::exchange(other.m_memory, {});
	m_reservedMemory = std::exchange(other.m_reservedMemory, {});
	m_pageSize = other.m_pageSize;
	m_blocksFree = other.m_blocksFree;
	m_blockSize = other.m_blockSize;
	m_lastFree = other.m_lastFree;
}

ggp::BlockAllocator::~BlockAllocator() noexcept
{
	mm::memory_unmap(m_reservedMemory.data(), m_reservedMemory.size_bytes());
}

std::span<u8> ggp::BlockAllocator::alloc() noexcept
{
	if (m_blocksFree == 0)
		if (!growCapacity())
			return {};

	EmptyBlock* const lastFree = getBlockAt(m_lastFree);
	EmptyBlock* const nextFree = m_blocksFree > 1 ? getBlockAt(lastFree->nextEmpty) : nullptr;

	--m_blocksFree;
	m_lastFree = lastFree->nextEmpty;

	return { (u8*)lastFree, m_blockSize };
}

bool ggp::BlockAllocator::growCapacity() noexcept
{
	if (m_memory.size() == m_reservedMemory.size())
		return false;

	// grow by 2x, not necessarily the best? but it works
	// if started at 0, start with only one block
	const size_t newSizePages = max(1, (m_memory.size_bytes() / m_pageSize) * 2);

	const size_t reservedPages = m_reservedMemory.size_bytes() / m_pageSize;
	const size_t cappedSizePages = min(newSizePages, reservedPages);

	auto result = mm::commit_pages(m_memory.data(), cappedSizePages);

	if (result != 0)
	{
		// could return false here?
		printf("ERROR: memory page commit failure, errcode %ld\n", result);
		std::abort();
	}

	const size_t oldNumBlocks = (cappedSizePages * m_pageSize) / m_blockSize;
	const size_t newNumBlocks = (cappedSizePages * m_pageSize) / m_blockSize;
	m_memory = { m_reservedMemory.data(), cappedSizePages * m_pageSize };

	for (size_t i = oldNumBlocks; i < newNumBlocks; ++i)
	{
		getBlockAt(i)->nextEmpty = i + i;
	}

	// we will start allocating into the newly allocated row of blocks, not
	// great for fragmentation but easy
	assert(newNumBlocks > 0);
	getBlockAt(newNumBlocks - 1)->nextEmpty = m_lastFree;
	m_lastFree = oldNumBlocks;

	m_blocksFree += newNumBlocks - oldNumBlocks;

	return true;
}

auto ggp::BlockAllocator::getBlockAt(size_t i) const noexcept -> EmptyBlock* 
{
	EmptyBlock* const out = (EmptyBlock*)(m_memory.data() + (m_blockSize * i));
	assert(is_aligned_to_type(out));
	assert(is_inbounds(m_memory, out));
	return out;
}

void ggp::BlockAllocator::free(std::span<u8> mem) noexcept
{}
