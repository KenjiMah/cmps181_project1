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

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    
    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    // (e.g., age: 24  height: 6.1  salary: 9000
    //        age: NULL  height: 7.5  salary: 7500)
//    char* buffer = NULL;
//    memcpy(buffer, data, sizeof(char));
//    cout << "test success" << endl;
    char * nullFieldsIndicator;
    memcpy(nullFieldsIndicator, (const char *)data, sizeof(char));
    int counter = 0;
    bool nullBit;
    for (unsigned int i = 0; i < recordDescriptor.size(); ++i){
        nullBit = nullFieldsIndicator & (1 << (5));
        if (!nullBit){
            cout << recordDescriptor[i].name << " Attr Type: " << (AttrType)recordDescriptor[i].type << " Attr Len: " << recordDescriptor[i].length << endl;

        }
        else cout << recordDescriptor[i].name << ": " << "NULL  ";
        counter ++;
    }
    cout << endl;
    return 0;
}
