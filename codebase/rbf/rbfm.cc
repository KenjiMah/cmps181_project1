#include "rbfm.h"
#include <iostream>
#include <cstring>
RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
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

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    const char * c = fileName.c_str();
    FILE* fd = fopen (c, "r");
    if(fd == NULL){
        return 0;
    }
    fclose(fd);
    remove(c);
    return 0;
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    const char * c = fileName.c_str();
    fileHandle.file = fopen(c, "rw+");
    return 0;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    fclose(fileHandle.file);
    return 0;
}

struct TableSlot{
    RID ridNum;
    int offsetInBytes;
};

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    unsigned pc = fileHandle.getNumberOfPages();
    unsigned x = 0;
    // Check if there are any pages
    if(pc == 0){
        // do the writing
        fileHandle.appendPage(data);
        return 0;
    }
    // malloc size for reading pages
    void * buffer = malloc(PAGE_SIZE);
    unsigned* freeSpace;
    // read each page and determine if there is enough freespace
    for(unsigned i = 0; i < pc; i++){
        fileHandle.readPage(i, buffer);
        // gets the number of free space
        freeSpace =(unsigned*) (buffer + 4092);
        // checks if the freespace is enough
        if(*freeSpace > x){
            //do the writing
            free(buffer);
            return 0;
        }
    }
    // if none of the pages have enough size append new file with correct data
    free(buffer);
    // do the writing
    fileHandle.appendPage(data);
    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    unsigned pc = fileHandle.getNumberOfPages();
    // check if the page number exists
    if(rid.pageNum >= pc){
        fprintf(stderr, "Invalid rid: Page Number does not exist\n");
        return -1;
    }
    // if page exists read it into the buffer
    void * buffer = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, buffer);
    // get number of records in the page
    int* slots;
    slots = (int*)(buffer + 4088);
    if(*slots == 0){
        fprintf(stderr, "No records are stored on this page!\n");
        return -1;
    }
    struct TableSlot *foo =(struct TableSlot*) malloc(sizeof(struct TableSlot)*(*slots));
    memcpy(foo, buffer + 4088 -(*slots * 12), *slots * 12);
    for(int i = 0; i < *slots; i++){
        if(foo[i].ridNum.slotNum == rid.slotNum){
            cout << "This is the offset: " << foo[i].offsetInBytes << endl;
            return 0;
        }
    }
    fprintf(stderr, "Invalid rid: Slot number not found on page!\n");
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    // (e.g., age: 24  height: 6.1  salary: 9000
    //        age: NULL  height: 7.5  salary: 7500)
//    char* buffer = NULL;
//    memcpy(buffer, data, sizeof(char));
//    cout << "test success" << endl;
    char * nullFieldsIndicator = (char *)malloc(1);
    memcpy(nullFieldsIndicator, (const char *)data, sizeof(char));
    int counter = 0;
    bool nullBit = false;
    for (unsigned int i = 0; i < recordDescriptor.size(); i++){
        nullBit = nullFieldsIndicator[0] & (1 << (8-i-1));
        if (!nullBit){
            cout << recordDescriptor[i].name << " Attr Type: " << (AttrType)recordDescriptor[i].type << " Attr Len: " << recordDescriptor[i].length << endl;

        }
        else cout << recordDescriptor[i].name << ": " << "NULL  ";
        counter ++;
    }
    printf("\n%d\n", nullFieldsIndicator[5]);
    cout << endl;
    return 0;
}
