#include "pfm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}


RC PagedFileManager::createFile(const string &fileName)
{
    const char * c = fileName.c_str();
    FILE* fd = fopen (c, "r");
    if(fd == NULL){
        FILE* fd = fopen (c, "w");
        fclose(fd);
        return 0;
    }
    fclose(fd);
    return -1;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
    const char * c = fileName.c_str();
    FILE* fd = fopen (c, "r");
    if(fd == NULL){
        return 0;
    }
    fclose(fd);
    remove(c);
    return 0;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    const char * c = fileName.c_str();
    fileHandle.file = fopen(c, "rw+");
    return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    fclose(fileHandle.file);
    return 0;
}


FileHandle::FileHandle()
{
	readPageCounter = 0;
	writePageCounter = 0;
	appendPageCounter = 0;
    pageCounter = 0;
}


FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
    return 0;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    return 0;
}


RC FileHandle::appendPage(const void *data)
{
    
    pageCounter ++;
    return 0;
}


unsigned FileHandle::getNumberOfPages()
{
    return pageCounter;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	return -1;
}
