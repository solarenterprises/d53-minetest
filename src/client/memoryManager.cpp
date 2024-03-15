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

		memory.erase(it);
		return;
	}

	assert(true, "Memory not found");
}

//
//MemoryManager::~MemoryManager() {
//	auto current = freeMemory;
//	while (current) {
//		auto del = current;
//		current = current->next;
//		delete del;
//	}
//}
//
//s64 MemoryManager::allocate(u32 chunkSize) {
//	assert(chunkSize % align_size == 0);
//
//	MemoryInfo* current = freeMemory;
//	while (current) {
//		auto mem = current;
//		current = current->next;
//
//		if (mem->chunkSize < chunkSize)
//			continue;
//
//		if (mem->chunkSize != chunkSize) {
//			u32 chunkOffset = mem->chunkOffset;
//			mem->chunkOffset = mem->chunkOffset + chunkSize;
//			mem->chunkSize = mem->chunkSize - chunkSize;
//
//			assert(mem->chunkSize % align_size == 0);
//			return chunkOffset;
//		}
//
//		//
//		// exactly matches size
//
//		u32 chunkOffset = mem->chunkOffset;
//		if (mem->next) {
//			if (!mem->prev) {
//				freeMemory = mem->next;
//				freeMemory->prev = nullptr;
//			}
//			else {
//				mem->prev->next = mem->next;
//				mem->next->prev = mem->prev;
//			}
//		}
//		else {
//			if (!mem->prev)
//				freeMemory = nullptr;
//			else
//				mem->prev->next = nullptr;
//		}
//
//		validate_list();
//
//		delete mem;
//
//		return chunkOffset;
//	}
//
//	if (used_mem + chunkSize >= size)
//		return -1;
//
//	s64 offset = used_mem;
//	assert(offset % align_size == 0);
//
//	used_mem += chunkSize;
//	return offset;
//}
//
//void MemoryManager::validate_list() {
//	if (freeMemory)
//		assert(!freeMemory->prev);
//
//	MemoryInfo* current = freeMemory;
//	u32 lastOffset = 0;
//	while (current) {
//		auto mem = current;
//		assert(mem->chunkOffset >= lastOffset);
//		lastOffset = mem->chunkOffset;
//		assert(mem->chunkOffset % align_size == 0);
//		assert(mem->chunkSize % align_size == 0);
//
//		if (mem->prev) {
//			assert(mem->prev->next == mem);
//			assert(mem->prev->chunkOffset + mem->prev->chunkSize <= mem->chunkOffset);
//		}
//		if (mem->next) {
//			assert(mem->next->prev == mem);
//			assert(mem->chunkOffset + mem->chunkSize <= mem->next->chunkOffset);
//		}
//		current = current->next;
//	}
//}
//
//void MemoryManager::free(u32 offset, u32 chunkSize) {
//	MemoryInfo* current = freeMemory;
//	MemoryInfo* last = nullptr;
//	while (current) {
//		auto mem = current;
//		last = current;
//		current = current->next;
//
//		if (mem->chunkOffset < offset)
//			continue;
//		if (mem->chunkOffset + mem->chunkSize < offset)
//			continue;
//
//		if (!mem->prev && !mem->next) {
//			delete mem;
//			freeMemory = nullptr;
//			return;
//		}
//
//		auto startMem = offset;
//		auto endMem = offset + chunkSize;
//		assert(!(startMem >= mem->chunkOffset && endMem <= mem->chunkOffset + mem->chunkSize), "This memory was already free.");
//
//		MemoryInfo* newMem = new MemoryInfo();
//		newMem->chunkOffset = startMem;
//		if (!mem->prev) {
//			freeMemory = newMem;
//		}
//		else {
//			newMem->prev = mem->prev;
//			newMem->prev->next = newMem;
//			mem->prev = nullptr;
//		}
//
//		while (mem && mem->chunkOffset == endMem) {
//			endMem = mem->chunkOffset + mem->chunkSize;
//			auto delMem = mem;
//			mem = mem->next;
//			delete delMem;
//		}
//
//		newMem->next = mem;
//		newMem->chunkSize = endMem - startMem;
//
//		if (mem)
//			mem->prev = newMem;
//
//		validate_list();
//
//		if (!newMem->next)
//			shrink_used_mem(newMem);
//		return;
//	}
//
//	if (offset + chunkSize >= used_mem) {
//		used_mem = offset;
//		return;
//	}
//
//	MemoryInfo* newMem = new MemoryInfo();
//	newMem->chunkOffset = offset;
//	newMem->chunkSize = chunkSize;
//
//	if (last) {
//		last->next = newMem;
//		newMem->prev = last;
//
//		validate_list();
//		return;
//	}
//
//	freeMemory = newMem;
//	validate_list();
//}
//
//void MemoryManager::shrink_used_mem(MemoryInfo* last) {
//	assert(last);
//	assert(!last->next);
//
//	while (last && last->chunkOffset + last->chunkSize >= used_mem) {
//		used_mem = last->chunkOffset;
//		auto del = last;
//		last = last->prev;
//		delete del;
//	}
//
//	if (last)
//		last->next = nullptr;
//	else
//		freeMemory = nullptr;
//
//	validate_list();
//}
