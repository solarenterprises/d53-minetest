#include "mesh_buffer_handler.h"
#include "IMaterialRenderer.h"

//void OpenGLSubDataArray::optimize(v3s16 view_center) {
//	if (data.size() <= 1)
//		return;
//
//	//
//	// Sort these so openGL data will load by blocks
//	for (int i = 0; i < data.size() - 1; i++) {
//		OpenGLSubData* a = data[i];
//		OpenGLSubData* b = data[i + 1];
//
//		if (a->offset + a->size >= b->offset && a->offset <= b->offset + b->size)
//			continue;
//
//		const s16 distA = (a->block_pos - view_center).getLengthSQ();
//		const s16 distB = (b->block_pos - view_center).getLengthSQ();
//
//		if (distA <= distB)
//			continue;
//
//		data[i] = b;
//		data[i + 1] = a;
//		i -= 2;
//	}
//
//	std::vector<OpenGLSubData*> toDelete;
//	for (std::vector<OpenGLSubData*>::reverse_iterator it = data.rbegin(); it != data.rend(); it++) {
//		for (std::vector<OpenGLSubData*>::reverse_iterator cmpIt = std::next(it); cmpIt != data.rend(); cmpIt++) {
//			if ((*cmpIt)->offset != (*it)->offset)
//				continue;
//			if ((*cmpIt)->size != (*it)->size)
//				continue;
//
//			toDelete.push_back(*cmpIt);
//		}
//	}
//
//	for (auto* elem : toDelete) {
//		auto it = std::find(data.begin(), data.end(), elem);
//		if (it == data.end())
//			continue;
//
//		delete* it;
//		data.erase(it);
//	}
//
//	// Define the structure for groups.
//	//struct Group {
//	//	u32 begin = 0;
//	//	u32 end = 0;
//	//	irr::core::array<u8> data;
//	//};
//
//	//std::vector<Group> groups;
//
//	//// Initial grouping based on offsets and sizes.
//	//for (const auto d : data) {
//	//	groups.push_back({ d->offset, d->offset + d->size, irr::core::array<u8>() });
//	//}
//
//	//// Sort groups based on 'begin' to simplify merging process.
//	//std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
//	//	return a.begin < b.begin;
//	//});
//
//	//// Attempt to collapse overlapping or contiguous groups.
//	//bool mergedOnce = false;
//	//bool merged;
//	//do {
//	//	merged = false;
//	//	for (size_t i = 0; i < groups.size(); i++) {
//	//		for (size_t j = i + 1; j < groups.size(); j++) {
//	//			auto& a = groups[i];
//	//			auto& b = groups[j];
//
//	//			// Check if 'a' and 'b' overlap or are contiguous.
//	//			if (a.end >= b.begin) {
//	//				// Merge 'b' into 'a' and erase 'b'.
//	//				a.end = std::max(a.end, b.end);
//	//				groups.erase(groups.begin() + j);
//	//				merged = true;
//	//				mergedOnce = true;
//	//				break; // Start over since we modified the vector.
//	//			}
//	//		}
//	//		if (merged) {
//	//			break; // Restart merging process if any merge occurred.
//	//		}
//	//	}
//	//} while (merged);
//
//	//// nothing is going to change
//	//if (!mergedOnce)
//	//	return;
//
//	//// Allocate memory for the groups based on their new sizes.
//	//for (auto& group : groups) {
//	//	group.data.set_used(group.end - group.begin); // Initially empty, fill in next step.
//	//}
//
//	//// Copy data into the newly allocated groups.
//	//int dataInserted = 0;
//	//for (auto d : data) {
//	//	if (d->isEmpty()) {
//	//		dataInserted++;
//	//		continue;
//	//	}
//
//	//	for (auto& group : groups) {
//	//		if (d->offset >= group.begin && d->offset + d->size <= group.end) {
//	//			// Calculate the correct offset within the group.
//	//			auto dest = group.data.pointer() + (d->offset - group.begin);
//	//			std::copy(d->data.pointer(), d->data.pointer() + d->size, dest);
//	//			dataInserted++;
//	//			break; // Data can only belong to one group, so stop once found.
//	//		}
//	//	}
//	//}
//	//assert(dataInserted == data.size());
//
//	//// Prepare the new data vector with the grouped data.
//	//std::vector<OpenGLSubData*> newData;
//	//for (auto& group : groups) {
//	//	auto d = new OpenGLSubData();
//	//	d->data = std::move(group.data);
//	//	d->size = group.end - group.begin;
//	//	d->offset = group.begin;
//	//	newData.push_back(d);
//	//}
//
//	//// Cleanup
//	//for (auto d : data)
//	//	delete d;
//
//	//// Replace the old data with the new, optimized data.
//	//data = std::move(newData);
//}

MeshBufferWorkerThread::MeshBufferWorkerThread(MeshUpdateManager* meshUpdateManager, video::CNullDriver* driver) : Thread("mesh buffer worker thread") {
	this->driver = driver;
	this->meshUpdateManager = meshUpdateManager;
}

MeshBufferWorkerThread::~MeshBufferWorkerThread() {
	cache_buffers.clear();
}

void MeshBufferWorkerThread::setView(v3s16 min, v3s16 max) {
	std::unique_lock<std::mutex> lock(m_mutex_view_range, std::try_to_lock);
	if (!lock.owns_lock())
		return;

	view_center = (max + min) / 2;
	view_min = min;
	view_max = max;
}

bool MeshBufferWorkerThread::getCacheBuffers(TextureBufListMaps& maps) {
	std::unique_lock<std::mutex> lock(m_mutex_cache_buffers, std::try_to_lock);
	if (!lock.owns_lock())
		return false;

	cache_buffers.grabEachTexture();
	maps = cache_buffers;
	return true;
}

void MeshBufferWorkerThread::removeBlocks(v3s16* positions, size_t num) {
	MutexAutoLock lock(m_mutex_remove_load_blocks);
	remove_load_mapblocks.insert(remove_load_mapblocks.end(), positions, positions + num);
}

std::vector<MeshUpdateResult> MeshBufferWorkerThread::getMeshUpdateResults() {
	MutexAutoLock lock(m_mutex_mesh_update_results);
	auto vector = std::move(mesh_update_results);
	mesh_update_results = std::vector<MeshUpdateResult>();
	return vector;
}

bool MeshBufferWorkerThread::unload_block(v3s16 pos, TextureBufListMaps::LoadBlockData& loadBlockData) {
	if (buffer_data.find(pos) == buffer_data.end())
		return false;

	auto& bufferList = buffer_data[pos];
	for (auto& meshBufferData : bufferList) {
		auto data = cache_buffers.getNoCreate(meshBufferData.texture, meshBufferData.layer);
		if (!data) {
			continue;
		}

		if (meshBufferData.indexMemory.is_valid()) {
			OpenGLSubData* indexData = new OpenGLSubData();
			indexData->size = meshBufferData.indexMemory.size();
			indexData->offset = meshBufferData.indexMemory.chunkStart;

			data->index_memory.free(meshBufferData.indexMemory);

			TextureBufListMaps::LoadData loadData;
			loadData.layer = meshBufferData.layer;
			loadData.texture = meshBufferData.texture;
			loadData.glIndexSubData = indexData;
			loadData.drawPrimitiveCount = (data->index_memory.used_mem / sizeof(u32)) / 3;

			loadBlockData.data.push_back(loadData);
		}

		if (meshBufferData.vertexMemory.is_valid())
			data->vertex_memory.free(meshBufferData.vertexMemory);
	}

	buffer_data.erase(pos);
	return true;
}

void MeshBufferWorkerThread::doUpdate() {

	static const size_t max_load_data_queue = 70;

	//
	// Don't stack on a huge queue.
	//
	size_t num_load_data_queue = 0;
	{
		MutexAutoLock lock(m_mutex_cache_buffers);
		num_load_data_queue = cache_buffers.loadData.size();
		if (num_load_data_queue >= max_load_data_queue)
			return;
	}

	//
	// Gather results
	//
	std::unordered_set<v3s16> unload_mapblocks;
	std::unordered_set<v3s16> mapblocks_needs_to_reload;
	MeshUpdateResult r;
	meshUpdateManager->undelayAll();
	while (meshUpdateManager->getNextResult(r)) {
		auto& p = r.p;

		mesh_update_result_queue.push_back(r);
	}

	//
	// sort and cleanup mesh update result queue
	//
	for (int i = mesh_update_result_queue.size()-1; i > 0; i--) {
		auto r = mesh_update_result_queue[i];
		for (int j = i - 1; j >= 0; j--) {
			auto& r2 = mesh_update_result_queue[j];
			if (r.p != r2.p)
				continue;

			mesh_update_result_queue.erase(mesh_update_result_queue.begin() + j);
			i--;
		}
	}

	//
	// Sort by urgent
	for (int i = 0; i < (int)mesh_update_result_queue.size() - 1; i++) {
		auto r = mesh_update_result_queue[i];
		auto r2 = mesh_update_result_queue[i+1];

		if (!r2.urgent)
			continue;
		if (r.urgent)
			continue;

		mesh_update_result_queue[i] = r2;
		mesh_update_result_queue[i+1] = r;

		i -= 2;
		if (i < 0)
			i = 0;
	}

	static const v3s16 view_padding(0, 0, 0);
	v3s16 cache_view_min;
	v3s16 cache_view_max;
	v3s16 cache_view_center;
	{
		MutexAutoLock lock(m_mutex_view_range);
		cache_view_min = view_min;
		cache_view_max = view_max;
		cache_view_center = view_center;
	}

	cache_view_min -= view_padding;
	cache_view_max += view_padding;

	//
	// Sort by distance
	std::vector<s16> distances;
	distances.reserve(mesh_update_results.size());
	for (auto& r : mesh_update_result_queue) {
		distances.push_back((r.p - cache_view_center).getLengthSQ());
	}

	for (int i = 0; i < (int)mesh_update_result_queue.size() - 1; i++) {
		auto r = mesh_update_result_queue[i];
		if (r.urgent)
			continue;

		auto distR = distances[i];
		auto distR2 = distances[i + 1];

		if (distR2 >= distR)
			continue;

		mesh_update_result_queue[i] = mesh_update_result_queue[i + 1];
		mesh_update_result_queue[i+1] = r;
		distances[i] = distR2;
		distances[i + 1] = distR;

		i--;
		if (i < 0)
			i = 0;
	}

	std::vector<MeshUpdateResult> results;

	while(!mesh_update_result_queue.empty()) {
		auto r = mesh_update_result_queue.front();
		results.push_back(r);
		mesh_update_result_queue.erase(mesh_update_result_queue.begin());

		if (r.mesh.get()) {
			bool is_empty = true;
			for (int l = 0; l < MAX_TILE_LAYERS; l++)
				if (r.mesh->getMesh(l)->getMeshBufferCount() != 0)
					is_empty = false;

			if (is_empty) {
				if (buffer_data.find(r.p) != buffer_data.end())
					unload_mapblocks.insert(r.p);
			}
			else
				mapblocks_needs_to_reload.insert(r.p);

			load_mapblocks[r.p] = r;
	
			num_load_data_queue++;
		}

		if (num_load_data_queue >= max_load_data_queue)
			break;
	}

	{
		MutexAutoLock lock(m_mutex_mesh_update_results);
		mesh_update_results.insert(mesh_update_results.end(), results.begin(), results.end());
	}

	//
	// Unload buffers
	//
	for (auto& it : buffer_data) {
		auto& p = it.first;
		if (p.X > cache_view_min.X && p.X < cache_view_max.X &&
			p.Z > cache_view_min.Z && p.Z < cache_view_max.Z)
			continue;

		if (buffer_data.find(r.p) == buffer_data.end())
			continue;

		unload_mapblocks.insert(p);
	}

	std::vector<TextureBufListMaps::LoadOrder> loadOrderVec;

	for (auto pos : unload_mapblocks) {
		TextureBufListMaps::LoadOrder loadOrder;
		loadOrder.pos = pos;

		if (!unload_block(pos, loadOrder.block_data))
			continue;

		loadOrderVec.push_back(loadOrder);
	}

	//
	// Cleanup load_mapblocks if they are too far away
	std::vector<v3s16> remove_load_mapblocks;
	{
		MutexAutoLock lock(m_mutex_remove_load_blocks);
		remove_load_mapblocks = this->remove_load_mapblocks;
		this->remove_load_mapblocks.clear();
	}
	for (auto& p : remove_load_mapblocks)
		load_mapblocks.erase(p);

	//
	// Block meshes has not been built yet.
	//
	std::unordered_map<v3s16, std::vector<SMeshBufferData>> build_blocks;
	std::set<video::ITexture*> keepTextures;

	for (auto it : load_mapblocks) {
		auto& r = it.second;

		auto block_mesh = r.mesh;
		if (!block_mesh)
			continue;

		auto& p = r.p;
		if (p.X <= cache_view_min.X || p.X >= cache_view_max.X ||
			p.Z <= cache_view_min.Z || p.Z >= cache_view_max.Z)
			continue;

		if (mapblocks_needs_to_reload.find(p) == mapblocks_needs_to_reload.end()) {
			if (buffer_data.find(p) != buffer_data.end())
				continue;
		}

		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
			scene::IMesh* mesh = block_mesh->getMesh(layer);
			assert(mesh);

			u32 c = mesh->getMeshBufferCount();
			for (u32 i = 0; i < c; i++) {
				if (!block_mesh->canMeshBufferBeCached(layer, i)) {
					continue;
				}

				scene::IMeshBuffer* buf = mesh->getMeshBuffer(i);

				video::IMaterialRenderer* rnd = driver->getMaterialRenderer(buf->getMaterial().MaterialType);
				bool transparent = (rnd && rnd->isTransparent());
				if (transparent)
					continue;

				auto sMeshBufferData = SMeshBufferData();
				sMeshBufferData.meshBuffer = buf;
				sMeshBufferData.texture = block_mesh->getBufferMainTexture(layer, i);
				sMeshBufferData.layer = layer;
				sMeshBufferData.pos = r.p;
				build_blocks[p].push_back(sMeshBufferData);
			}
		}
	}

	//
	// Start building dynamic meshes
	//
	for (auto& it : build_blocks) {
		auto& pos = it.first;
		auto& loadBuffers = it.second;

		TextureBufListMaps::LoadOrder* loadOrder = nullptr;
		loadOrderVec.push_back(TextureBufListMaps::LoadOrder());
		loadOrder = &loadOrderVec.back();
		loadOrder->pos = pos;
		TextureBufListMaps::LoadBlockData& loadBlockData = loadOrder->block_data;

		//
		// Unload here
		unload_block(pos, loadBlockData);

		//
		// Vertices & Indices
		//
		for (auto& sMeshBufferData : loadBuffers) {
			video::ITexture* texture = sMeshBufferData.texture;
			keepTextures.insert(texture);

			TextureBufListMaps::Data* data;
			{
				MutexAutoLock lock(m_mutex_cache_buffers);
				data = cache_buffers.get(texture, sMeshBufferData.layer);
			}

			if (!data->vertexCount && !data->indexCount) {
				data->vertexCount = 100'000;
				data->indexCount = 50'000;

				data->vertex_memory.setSize(data->vertexCount * sizeof(video::S3DVertex));
				data->index_memory.setSize(data->indexCount * sizeof(u32));

				data->material = sMeshBufferData.meshBuffer->getMaterial();
			}

			//
			// Allocate memory spots for buffers
			//
			auto meshBuffer = sMeshBufferData.meshBuffer;
			auto vertexCount = meshBuffer->getVertexCount();
			auto indexCount = meshBuffer->getIndexCount();

			if (indexCount == 0 || vertexCount == 0) {
				continue;
			}

			sMeshBufferData.vertexMemory = data->vertex_memory.allocate(vertexCount * sizeof(video::S3DVertex));
			sMeshBufferData.indexMemory = data->index_memory.allocate(indexCount * sizeof(u32));

			if (!sMeshBufferData.vertexMemory.is_valid()) {
				//
				// Grow Vertex memory
				//
				size_t grow_to_size = data->vertex_memory.size / sizeof(video::S3DVertex) + 50'000;
				for (auto& sMeshBufferData : loadBuffers)
					grow_to_size += sMeshBufferData.meshBuffer->getVertexCount();

				data->vertexCount = grow_to_size;
				data->vertex_memory.setSize(data->vertexCount * sizeof(video::S3DVertex));

				//
				// Try to get offset again
				if (!sMeshBufferData.vertexMemory.is_valid())
					sMeshBufferData.vertexMemory = data->vertex_memory.allocate(vertexCount * sizeof(video::S3DVertex));

				if (!sMeshBufferData.vertexMemory.is_valid())
					continue;
			}

			if (!sMeshBufferData.indexMemory.is_valid()) {
				//
				// Grow Index memory
				//
				size_t grow_to_size = data->index_memory.size / sizeof(u32) + 80'000;
				for (auto& sMeshBufferData : loadBuffers)
					grow_to_size += sMeshBufferData.meshBuffer->getIndexCount();

				data->indexCount = grow_to_size;
				data->index_memory.setSize(data->indexCount * sizeof(u32));

				//
				// Try to get offset again
				if (!sMeshBufferData.indexMemory.is_valid())
					sMeshBufferData.indexMemory = data->index_memory.allocate(indexCount * sizeof(u32));

				//
				// Move vertex & index memory to GPU
				//
				if (!sMeshBufferData.indexMemory.is_valid())
					continue;
			}

			//
			// Move vertex & index memory to GPU
			//

			//
			// Vertices
			auto vertices = (video::S3DVertex*)meshBuffer->getVertices();
			OpenGLSubData* vertexData = new OpenGLSubData();
			vertexData->size = vertexCount * sizeof(video::S3DVertex);
			vertexData->data.set_data((u8*)vertices, vertexData->size);
			vertexData->offset = sMeshBufferData.vertexMemory.chunkStart;

			auto indices = meshBuffer->getIndices();

			//
			// Indices
			OpenGLSubData* indexData = new OpenGLSubData();

			core::array<u32> indicesWithOffset;
			indicesWithOffset.set_used(indexCount);
			u32 indexValueOffset = sMeshBufferData.vertexMemory.chunkStart / sizeof(video::S3DVertex);
			u32* indices_ptr = indicesWithOffset.pointer();
			for (u32 i = 0; i < indexCount; ++i) {
				*indices_ptr = indices[i] + indexValueOffset;
				indices_ptr++;
			}

			indexData->size = indexCount * sizeof(u32);
			indexData->data.set_data((u8*)indicesWithOffset.pointer(), indexData->size);
			indexData->offset = sMeshBufferData.indexMemory.chunkStart;

			//
			// Load Data
			TextureBufListMaps::LoadData loadData;
			loadData.glIndexSubData = indexData;
			loadData.glVertexSubData = vertexData;
			loadData.drawPrimitiveCount = (data->index_memory.used_mem / sizeof(u32)) / 3;
			loadData.texture = texture;
			loadData.layer = sMeshBufferData.layer;

			loadBlockData.data.push_back(loadData);

			buffer_data[sMeshBufferData.pos].push_back(sMeshBufferData);
		}
	}

	if (!loadOrderVec.empty()) {
		MutexAutoLock lock(m_mutex_cache_buffers);
		cache_buffers.loadData.insert(cache_buffers.loadData.end(), loadOrderVec.begin(), loadOrderVec.end());
	}

	//
	// Keep textures
	for (auto it : buffer_data) {
		for (auto& buffer : it.second) {
			keepTextures.insert(buffer.texture);
		}
	}

	//
	// Check if should drop a texture
	struct Drop {
		u8 layer;
		video::ITexture* texture;
	};
	std::vector<Drop> dropTextures;
	for (u8 layer = 0; layer < MAX_TILE_LAYERS; layer++) {
		auto& map = cache_buffers.maps[layer];
		for (auto& list : map) {
			auto cache = list.second;
			auto texture = list.first;
			if (cache && keepTextures.find(texture) != keepTextures.end())
				continue;

			dropTextures.push_back({ layer, texture });
		}
	}

	{
		MutexAutoLock lock(m_mutex_cache_buffers);
		for (auto& it : dropTextures) {
			cache_buffers.drop(it.texture, it.layer);
		}
	}
}


MeshBufferHandler::MeshBufferHandler(MeshUpdateManager* meshUpdateManager, video::CNullDriver* driver) {
	this->meshUpdateManager = meshUpdateManager;
	m_worker = std::make_unique<MeshBufferWorkerThread>(meshUpdateManager, driver);
}

void MeshBufferHandler::setView(v3s16 min, v3s16 max) {
	m_worker->setView(min, max);
}

std::vector<MeshUpdateResult> MeshBufferHandler::getMeshUpdateResults() {
	return std::move(m_worker->getMeshUpdateResults());
}

void MeshBufferHandler::removeBlocks(v3s16* positions, size_t num) {
	m_worker->removeBlocks(positions, num);
}

bool MeshBufferHandler::getCacheBuffers(TextureBufListMaps& maps) {
	// This function grabs each texture but will be dropped again after maps get's cleared
	return m_worker->getCacheBuffers(maps);
}

void MeshBufferHandler::start()
{
	m_worker->start();
}

void MeshBufferHandler::stop()
{
	m_worker->stop();
}

void MeshBufferHandler::wait()
{
	m_worker->wait();
}

bool MeshBufferHandler::isRunning()
{
	return meshUpdateManager->isRunning() && m_worker->isRunning();
}
