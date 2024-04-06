#pragma once

#include "threading/mutex_auto_lock.h"
#include "util/thread.h"
#include "mapblock_mesh.h"
#include "mesh_generator_thread.h"
#include "memoryManager.h"
#include <thread>
#include "CNullDriver.h"

struct OpenGLSubData {
	irr::core::array<u8> data;
	u32 size = 0;
	u32 offset = 0;

	inline bool isEmpty() const {
		return data.empty();
	}
};

//struct OpenGLSubDataArray {
//	std::vector<OpenGLSubData*> data;
//
//	inline void push_back(OpenGLSubData* d) {
//		data.push_back(d);
//	}
//
//	void optimize(v3s16 view_center);
//};

struct TextureBufListMaps
{
	struct TextureHash
	{
		size_t operator()(const video::ITexture* t) const noexcept
		{
			// Only hash first texture. Simple and fast.
			//return std::hash<video::ITexture*>{}(m.TextureLayers[0].Texture);
			return (size_t)t;
		}
	};

	struct LoadData {
		u8 layer = 0;
		video::ITexture* texture = nullptr;

		size_t drawPrimitiveCount = 0;

		OpenGLSubData* glVertexSubData = nullptr;
		OpenGLSubData* glIndexSubData = nullptr;
	};

	struct LoadBlockData {
		std::vector<LoadData> data;
	};

	struct LoadOrder {
		v3s16 pos;
		LoadBlockData block_data;
	};
	std::vector<LoadOrder> loadData;

	struct Data
	{
		size_t vertexCount = 0;
		size_t indexCount = 0;
		video::SMaterial material;

		MemoryManager vertex_memory;
		MemoryManager index_memory;

		Data() :
			vertex_memory(sizeof(video::S3DVertex)),
			index_memory(sizeof(u32) * 3) {}
	};

	using MaterialBufListMap = std::unordered_map<
		video::ITexture*,
		std::shared_ptr<Data>,
		TextureHash>;

	std::array<MaterialBufListMap, MAX_TILE_LAYERS> maps;

	~TextureBufListMaps() {
	}

	void clear()
	{
		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
			auto& map = maps[layer];

			for (auto it : map) {
				it.first->drop();
			}

			map.clear();
		}
	}

	void grabEachTexture() {
		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
			auto& map = maps[layer];
			for (auto it : map)
				it.first->grab();
		}
	}

	Data* getNoCreate(video::ITexture* texture, u8 layer)
	{
		assert(layer < MAX_TILE_LAYERS);

		// Append to the correct layer
		auto& map = maps[layer];
		if (map.find(texture) == map.end())
			return nullptr;

		return map[texture].get();
	}

	Data* get(video::ITexture* texture, u8 layer)
	{
		assert(layer < MAX_TILE_LAYERS);

		// Append to the correct layer
		auto& map = maps[layer];

		if (map.find(texture) != map.end())
			return map[texture].get();

		texture->grab();

		auto* data = new Data();
		map[texture] = std::shared_ptr<Data>(data);
		return data;
	}

	void drop(video::ITexture* texture, u8 layer)
	{
		assert(layer < MAX_TILE_LAYERS);

		// Append to the correct layer
		auto& map = maps[layer];
		if (map.find(texture) == map.end())
			return;

		for (auto& loadOrder : loadData) {
			for (int i = 0; i < loadOrder.block_data.data.size(); i++) {
				auto& data = loadOrder.block_data.data[i];
				if (data.texture != texture)
					continue;

				loadOrder.block_data.data.erase(loadOrder.block_data.data.begin()+i);
				i--;
			}
		}

		texture->drop();

		map.erase(texture);
	}
};

struct SMeshBufferData {
	MemoryManager::MemoryInfo vertexMemory;
	MemoryManager::MemoryInfo indexMemory;

	scene::IMeshBuffer* meshBuffer = nullptr;
	video::ITexture* texture = nullptr;
	u8 layer = 0;
	v3s16 pos;
};

class MeshBufferWorkerThread : public Thread
{
public:
	MeshBufferWorkerThread(MeshUpdateManager* meshUpdateManager, video::CNullDriver* driver);
	~MeshBufferWorkerThread();
	void setView(v3s16 min, v3s16 max);
	bool getCacheBuffers(TextureBufListMaps &map);
	std::vector<MeshUpdateResult> getMeshUpdateResults();
	void removeBlocks(v3s16* positions, size_t num);

	void eraseLoadOrder(size_t num) {
		MutexAutoLock lock(m_mutex_cache_buffers);
		cache_buffers.loadData.erase(cache_buffers.loadData.begin(), cache_buffers.loadData.begin() + num);
	}

	bool unload_block(v3s16 pos, TextureBufListMaps::LoadBlockData& loadBlockData);

	void stop()
	{
		meshUpdateManager->stop();
		meshUpdateManager->wait();
		Thread::stop();
	}

	void* run()
	{
		using namespace std::chrono_literals;

		BEGIN_DEBUG_EXCEPTION_HANDLER

		while (!stopRequested()) {
			doUpdate();
			std::this_thread::sleep_for(1ms);
		}

		END_DEBUG_EXCEPTION_HANDLER

			return NULL;
	}

private:
	void doUpdate();
	
private:
	MeshUpdateManager* meshUpdateManager;
	video::CNullDriver* driver;

	std::mutex m_mutex_mesh_update_results;
	std::vector<MeshUpdateResult> mesh_update_results;
	std::vector<MeshUpdateResult> mesh_update_result_queue;

	std::mutex m_mutex_cache_buffers;
	TextureBufListMaps cache_buffers;
	std::unordered_map<v3s16, std::vector<SMeshBufferData>> buffer_data;

	std::unordered_map<v3s16, MeshUpdateResult> load_mapblocks;

	std::mutex m_mutex_view_range;
	v3s16 view_center;
	v3s16 view_min;
	v3s16 view_max;

	std::mutex m_mutex_remove_load_blocks;
	std::vector<v3s16> remove_load_mapblocks;
};

class MeshBufferHandler {
public:
	MeshBufferHandler(MeshUpdateManager* meshUpdateManager, video::CNullDriver *driver);

	void setView(v3s16 min, v3s16 max);
	std::vector<MeshUpdateResult> getMeshUpdateResults();
	void removeBlocks(v3s16* positions, size_t num);
	void eraseLoadOrder(size_t num) {
		if (!num)
			return;

		m_worker->eraseLoadOrder(num);
	}

	void start();
	void stop();
	void wait();

	bool isRunning();

	bool getCacheBuffers(TextureBufListMaps &map);

private:
	std::unique_ptr<MeshBufferWorkerThread> m_worker;
	MeshUpdateManager* meshUpdateManager;
};
