
#include "Block.h"



Block::Block(const char* path, int index, size_t blksize):
		_refCount(REF_COUNT_INIT),
		_index(index){
	_path = new std::string(path);
	_content = (char*)aligned_alloc(blksize, blksize);
	memset(_content, 0, blksize);
}


Block::~Block(){
	delete(_path);
	free(_content);
	_path = NULL;
	_content = NULL;
}


bool Block::comparator(const Block* d1, const Block* d2) {
	return d1->_refCount > d2->_refCount;
}