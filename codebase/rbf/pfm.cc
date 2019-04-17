#include "pfm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>
#include <iostream>

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
    fd = fopen(c, "w+b");
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
    // void * buffer = malloc(sizeof(unsigned int));
    fread(&fileHandle.readPageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.writePageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.appendPageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.pageCounter, sizeof(unsigned), 1, fileHandle.file);
    cout << "READING " << fileHandle.readPageCounter << endl;
    cout << fileHandle.pageCounter << endl;
    cout << fileHandle.appendPageCounter << endl;
    cout << fileHandle.writePageCounter << endl;

    return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    if(fileHandle.file == nullptr){
        fprintf(stderr, "This file handle has no open file\n");
        return -1;
    }
    // fseek(fileHandle.file, 0, SEEK_SET);
    rewind(fileHandle.file);
    fwrite(&fileHandle.readPageCounter, sizeof(unsigned int), 1, fileHandle.file);
    // fseek(fileHandle.file, sizeof(unsigned), SEEK_SET);
    fwrite(&fileHandle.writePageCounter, sizeof(unsigned int), 1, fileHandle.file);
    // fseek(fileHandle.file, sizeof(unsigned), SEEK_CUR);
    fwrite(&fileHandle.appendPageCounter, sizeof(unsigned int), 1, fileHandle.file);
    // fseek(fileHandle.file, sizeof(unsigned), SEEK_CUR);
    fwrite(&fileHandle.pageCounter, sizeof(unsigned), 1, fileHandle.file);
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

typedef struct{
    int pageNum;

}page;
struct fileStats{
    unsigned rpc = 0;
    unsigned wpc = 0;
    unsigned apc = 0;
    unsigned pc = 0;
};

RC FileHandle::readPage(PageNum pageNum, void *data)
{
    if(pageNum >= pageCounter){
        fprintf(stderr, "Invalid page number!\n");
        return -1;
    }
    if(file == nullptr){
        fprintf(stderr, "No file currently open\n");
        return -1;
    }
    long offset = (pageNum * 4096) + 16;
    if(fseek(file, offset, SEEK_SET) != 0){
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return -1;
    }
    // cout << "Bytes read: " << fread(*&data, sizeof(unsigned), 4096, file) << endl;
    if(fread(data, 4096, 1, file) != 1){
        fprintf(stderr, "File corrupt!\n");
        return -1;
    };
    readPageCounter++;
    return 0;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    if(pageNum >= pageCounter){
        fprintf(stderr, "Invalid page number!\n");
        return -1;
    }
    if(file == nullptr){
        fprintf(stderr, "No file currently open\n");
        return -1;
    }
    long offset = (pageNum * 4096) + 16;
    if(fseek(file, offset, SEEK_SET) != 0){
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return -1;
    }
    fwrite(data, 4096, 1, file);
    writePageCounter++;
    return 0;
}


RC FileHandle::appendPage(const void *data)
{
    if(file == nullptr){
        fprintf(stderr, "No file currently open\n");
        return -1;
    }
    // Offset to pointing to the end of the new page
    long offset = ((pageCounter) * 4096) + 16;
    unsigned check = 0;
    fseek(file, offset, SEEK_SET);
    fwrite(data, 4096, 1, file);
    // Write the table/array containing the freespace, etc...
    // fwrite(&check, sizeof(unsigned),1,file);
    appendPageCounter++;
    pageCounter ++;
    return 0;
}


unsigned FileHandle::getNumberOfPages()
{
    return pageCounter;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
	return 0;
}
