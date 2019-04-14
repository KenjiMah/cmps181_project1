#include "pfm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

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
    if(fd != nullptr){
        fprintf(stderr, "Error: Attempting to create a file with the same name\n");
        fclose(fd);
        return -1;
    }
    fd = fopen(c, "w");
    if(fd == nullptr){
        fprintf(stderr, "Error: %s", strerror(errno));
        return -1;
    }
    fclose(fd);
    return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
    const char * c = fileName.c_str();
    FILE* fd = fopen (c, "r");
    if(fd == nullptr){
        fprintf(stderr, "Attempting to destroy file that does not exist!\n");
        return -1;
    }
    fclose(fd);
    remove(c);
    return 0;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    if(fileHandle.file != nullptr){
        fprintf(stderr, "This file handle already has an open file!\n");
        return -1;
    }
    const char * c = fileName.c_str();
    FILE* check = fopen(c, "r");
    if(check == nullptr){
        // fprintf(stderr, "Error: %s", strerror(errno));
        fprintf(stderr, "Attempting to open file that does not exist!\n");
        return -1;
    }
    fclose(check);
    if((fileHandle.file = fopen(c, "rw+")) == nullptr){
        fprintf(stderr, "Error: %s", strerror(errno));
        return -1;
    }
    return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    if(fileHandle.file == nullptr){
        fprintf(stderr, "This file handle has no open file\n");
        return -1;
    }
    if(fclose(fileHandle.file) != 0){
        fprintf(stderr, "Error: %s", strerror(errno));
        return -1;
    }
    return 0;
}


FileHandle::FileHandle()
{
	readPageCounter = 0;
	writePageCounter = 0;
	appendPageCounter = 0;
    pageCounter = 0;
    file = nullptr;
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
