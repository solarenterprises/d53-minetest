#include <irrTypes.h>
#include <vector>

using namespace irr;

class MemoryManager {
public:
	struct MemoryInfo {
		s64 id = -1;
		u32 chunkStart = 0;
		u32 chunkEnd = 0;

		inline u32 size() {
			return chunkEnd - chunkStart;
		}

		inline bool is_valid() {
			return id >= 0;
		}
	};

	MemoryInfo invalid_mem;

	u32 size = 0;
	u32 used_mem = 0;
	u32 align_size = 1;

	s64 next_id = 0;

	std::vector<MemoryInfo> memory;

	MemoryManager(u32 align_size) : align_size(align_size) {}

	void setSize(u32 size);
	MemoryInfo allocate(u32 chunkSize);
	void free(const MemoryInfo &info);
};

//class MemoryManager {
//public:
//	struct MemoryInfo {
//		u32 chunkOffset = 0;
//		u32 chunkSize = 0;
//		MemoryInfo* prev = nullptr;
//		MemoryInfo* next = nullptr;
//	};
//
//	u32 size = 0;
//	u32 used_mem = 0;
//	u32 align_size = 1;
//	MemoryInfo* freeMemory = nullptr;
//
//	MemoryManager(u32 align_size) : align_size(align_size) {}
//	~MemoryManager();
//
//	s64 allocate(u32 chunkSize);
//
//	void validate_list();
//
//	void free(u32 offset, u32 chunkSize);
//
//	void shrink_used_mem(MemoryInfo* last);
//};
