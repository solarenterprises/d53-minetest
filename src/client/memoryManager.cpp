#include "memoryManager.h"
#include <assert.h>

MemoryManager::MemoryInfo MemoryManager::allocate(u32 chunkSize) {
	assert(chunkSize % align_size == 0);

	u32 last_end = 0;
	s64 id = next_id++;

	std::vector<MemoryManager::MemoryInfo>::iterator it = memory.begin();
	for (; it != memory.end(); it++) {
		auto& info = *it;
		if (info.chunkStart < last_end + chunkSize) {
			last_end = info.chunkEnd;
			continue;
		}

		auto itNext = it+1;
		if (itNext == memory.end())
			break;

		if (last_end+chunkSize > itNext->chunkStart)
			continue;

		break;
	}

	if (last_end + chunkSize >= size)
		return invalid_mem;

	MemoryManager::MemoryInfo newInfo;
	newInfo.id = id;
	newInfo.chunkStart = last_end;
	newInfo.chunkEnd = newInfo.chunkStart + chunkSize;
	memory.insert(it, newInfo);

	assert(newInfo.chunkStart % align_size == 0);

	if (used_mem < newInfo.chunkEnd)
		used_mem = newInfo.chunkEnd;

	return newInfo;
}

void MemoryManager::setSize(u32 size) {
	this->size = size;
}

void MemoryManager::free(const MemoryManager::MemoryInfo& info) {
	for (auto it = memory.begin(); it != memory.end(); it++) {
		if (it->id != info.id)
			continue;

		if (it+1 == memory.end() && memory.size() >= 2) {
			used_mem = (it-1)->chunkEnd;
		}

		memory.erase(it);

		return;
	}

	assert(true); // "Memory not found"
}
