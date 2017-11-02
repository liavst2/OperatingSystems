
#include "MapReduceFramework.h"
#include <iostream>
#include <dirent.h>
#include <algorithm>
#include <memory>

#define FALIURE -1
#define SUCCESS 0
#define MULTI_THREAD_LEVEL 5
#define USAGE "Usage: <substring to search> <folders, separated by space>"

/**
 * representing the directory name
 */
class Dir : public v1Base {
private:
	std::string __dir;

public:

	explicit Dir(std::string dir_str){
		__dir = dir_str;
	}

	explicit Dir(const Dir& dir_str){
		__dir = const_cast<Dir&>(dir_str).__dir;
	}

	virtual ~Dir(){}

	std::string getDir() const{
		return __dir;
	}

};


/**
 * representing the substring
 */
class Substring : public k1Base, public k2Base {
private:
	std::string __sub;

public:

	explicit Substring(std::string sub){
		__sub = sub;
	}

	explicit Substring(const Substring& sub_str){
		__sub = const_cast<Substring&>(sub_str).__sub;
	}

	virtual ~Substring(){}

	virtual bool operator<(const k2Base &other) const{
		return ((__sub.compare(((const Substring&)other).__sub) < 0));
	}

	virtual bool operator<(const k1Base &other) const{
		return ((__sub.compare(((const Substring&)other).__sub) < 0));
	}

	std::string getSub() const{
		return __sub;
	}
};


/**
 * representing a file name
 */
class File : public v2Base, public k3Base{
private:
	std::string __filename;

public:

	explicit File(std::string str){
		__filename = str;
	}

	explicit File(const File& filename_str){
		__filename = const_cast<File&>(filename_str).__filename;
	}

	virtual ~File(){}

	virtual bool operator< (const k3Base &other) const{
		return ((__filename.compare(((const File&)other).__filename) < 0));
	}
	std::string getFile() const{
		return __filename;
	}
	void print(){
		std::cout << __filename << std::endl;
	}
};

/**
 * an abstract class holding the desired map and reduce functions
 * used inside the framework
 */
class MapReduce : public MapReduceBase{

public:

	/**
	 * implementing the map function
	 */
	virtual void Map(const k1Base *const key, const v1Base *const val) const{
		Substring* sub = dynamic_cast<Substring*>(const_cast<k1Base*>(key));
		Dir* val1 = dynamic_cast<Dir*>(const_cast<v1Base*>(val));
		DIR* dir;
		struct dirent* ent;
		if ((dir = opendir(val1->getDir().c_str())) != NULL){
			while ((ent = readdir(dir)) != NULL) {
				File* file = new File(ent->d_name);
				Emit2(sub, file);
			}
			closedir(dir);
		}
		return;
	}

	/**
	 * implementing the reduce function
	 */
	virtual void Reduce(const k2Base *const key, const V2_LIST &val) const{
		Substring* sub = dynamic_cast<Substring*>(const_cast<k2Base*>(key));
		for(V2_LIST::const_iterator it = val.cbegin(); it != val.cend();it++){
			File* file = dynamic_cast<File*>(const_cast<v2Base*>(*it));
			if (file->getFile().find(sub->getSub()) != std::string::npos){
				Emit3(file, NULL);
			} else {
				delete(file);
			}
		}
		return;
	}
};


/**
 * prints the file names the framework returned
 * @param outlist - the list of the desired file names
 */
void printFiles(OUT_ITEMS_LIST &outlist){
	while (!outlist.empty()){
		OUT_ITEM& out = outlist.front();
		((File*)out.first)->print();
		delete(out.first);
		outlist.pop_front();
	}
}

/**
 * clears the input of the framework
 */
void _clear_inlist(IN_ITEMS_LIST& inlist){
	for (auto& item: inlist){
		if (item.second){
			delete(item.second);
		}
	}
	inlist.clear();
}



/**
 * the main function for this program
 */
int main(int argc, char* argv[]){

	// there are no arguments
	if (argc == 1){
		std::cerr << USAGE << std::endl;
		return FALIURE;
	}

	// the g_FWinput is the substring only
	if (argc == 2){
		return SUCCESS;
	}

	IN_ITEMS_LIST inlist;
	//Substring* substring = new Substring(argv[1]);
	Substring* substring = new Substring(argv[1]);

	// filling the g_InputVec list
	for(int i = 2; i < argc; inlist.push_back
			(IN_ITEM(&(*substring), new Dir(argv[i++]))));

	MapReduce MR;

	// running the framework and printing the desired files
	OUT_ITEMS_LIST outlist = runMapReduceFramework
									(MR, inlist, MULTI_THREAD_LEVEL);
	printFiles(outlist);

	delete(substring);
	_clear_inlist(inlist);
	return SUCCESS;
}