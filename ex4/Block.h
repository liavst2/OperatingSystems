

#ifndef EX4_BLOCK_H
#define EX4_BLOCK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#define REF_COUNT_INIT 1


class Block {
public:
	int _refCount;
	std::string* _path;
	int _index;
	char* _content;

	Block(const char* path, int index, size_t blksize);

	~Block();

	static bool comparator(const Block* , const Block*);
};



#endif //EX4_BLOCK_H
