


#include <iostream>
#include "Cache.h"



Cache::~Cache(){
	_cache_clean();
}



Cache::Cache(const double num_args[]){
	_TotalSize = (size_t) num_args[0];
	_NewSize = (size_t) std::floor((double)_TotalSize * num_args[1]);
	_OldSize = (size_t) std::floor((double)_TotalSize * num_args[2]);
	if (!_NewSize || !_OldSize){
		throw FALIURE;
	}
	_MidSize = _TotalSize - _NewSize - _OldSize;
	struct stat fi;
	stat("/tmp", &fi);
	_blksize = fi.st_blksize;
}



int Cache::collect(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi){

	int total_size = _fsize(path);
	if (total_size < 0){
		return total_size;
	}

	if (size > (size_t) (total_size - offset)){ // adjust the size
		size = (size_t) (total_size - offset);
	}

	if (total_size < offset || size <= 0){
		return SUCCESS;
	}

	// check if its in the cache
	int start_index = (int) std::floor((double)offset / (double)_blksize);
	int end_index = (int) std::floor
			((((double)size + (double)offset) - 1)  / (double)_blksize);
	int num_of_blocks = end_index - start_index + 1;

	char* ret[num_of_blocks];
	for (int index = start_index, i = 0; i < num_of_blocks; index++, i++){

		if (!_find_existing_block(path, index)) {
			try {
				ret[i] = _cache_add(path, fi, index);
			} catch (std::string& err){
				return -errno;
			} catch (std::bad_alloc& err){
				return -FALIURE;
			}
			continue;
		}
		ret[i] = _cache_replace(path, index);
	}

	if (num_of_blocks == 1){
		memcpy(buf, ret[0] + offset - start_index*_blksize, size);
		return (int)size;
	}

	size_t buf_offset = (size_t) ((start_index + 1)*_blksize - offset);
	memcpy(buf, ret[0] + offset - start_index*_blksize, buf_offset);


	for (int i = 1; i < num_of_blocks - 1; i++){
		memcpy(buf + buf_offset, ret[i], (size_t) _blksize);
		buf_offset += (size_t) (_blksize);
	}

	memcpy(buf + buf_offset, ret[num_of_blocks - 1], size - buf_offset);
	return (int)size;
}


bool Cache::_find_existing_block(const char *path, int index){
	for (Block* block: _Cache){
		if (block->_index == index && !block->_path->compare(path)){
			return true;
		}
	}
	return false;
}



int Cache::ioctl_info(LogFile** lf, std::string rootdir){
	for (size_t i = 0; i < _Cache.size(); i++){
		std::string file(*(_Cache[i]->_path));
		size_t pos = file.find(rootdir);
		std::string path = file.substr(pos + rootdir.length() + 1);
		(*lf)->cache_ioctl(path, _Cache[i]->_index, _Cache[i]->_refCount);
	}
	return SUCCESS;
}



void Cache::inner_rename(std::string* path, std::string* newpath) {

	for (Block* item: _Cache){
		if (path->length() > item->_path->length()){
			continue;
		}
		std::string prefix_path = item->_path->substr(0, path->length());
		if (!prefix_path.compare(*path)) {
			item->_path->replace(0, path->length(), *newpath);
		}
	}
	return;
}



void Cache::_cache_clean(){
	for (Block* item: _Cache){
		delete(item);
	}
	_Cache.clear();
	return;
}



char* Cache::_cache_replace(const char* path, int index){

	auto iter = _Cache.begin();
	for (size_t i = 1; i <= _Cache.size(); i++, iter++){
		if(!(*iter)->_path->compare(path) && (*iter)->_index == index) {
			Block* block = std::move((*iter));
			_Cache.erase(iter);
			_Cache.push_front(block);

			if (i > _NewSize){ // was not in new section
				block->_refCount++;
				if(i > _NewSize + _MidSize){ // was in old section
					_sort_old();
				}
			}

			return block->_content;
		}
	}

	return NULL;
}



char* Cache::_cache_add(const char* path,struct fuse_file_info*fi, int index){

	Block* newBlock = new Block(path, index, (size_t) _blksize);
	ssize_t amount = pread((int)fi->fh, newBlock->_content,
	                       (size_t) _blksize, (off_t)(index*_blksize));
	if (amount < 0){
		throw "error";
	}

	_Cache.push_front(newBlock);
	if (_Cache.size() == _TotalSize + 1){
		_cache_evict();
	}

	return newBlock->_content;
}



void Cache::_cache_evict(){
	Block* evicted = std::move(_Cache.back());
	_Cache.pop_back();
	delete(evicted);
	_sort_old();
}



void Cache::_sort_old(void){
	auto iter = _Cache.begin();
	std::advance(iter, _TotalSize - _OldSize);
	std::stable_sort(iter, _Cache.end(), Block::comparator);
}



int Cache::_fsize(const char *file){
	struct stat st;
	return stat(file, &st) ? (-errno): (int)st.st_size;
}