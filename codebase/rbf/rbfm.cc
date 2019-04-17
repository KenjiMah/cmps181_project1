#include "rbfm.h"
#include <iostream>
#include <cstring>
#include <math.h>
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
    int actualByteForNullsIndicator = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char * nullFieldsIndicator = (char *)malloc(actualByteForNullsIndicator);
    memcpy(nullFieldsIndicator, (const char *)data, sizeof(char));
    bool nullBit = false;
    AttrType type;
    int offset = actualByteForNullsIndicator;
    TableSlot newSlot;
    int *intNum = (int *)malloc(sizeof(int));
    float *floatNum = (float *)malloc(sizeof(float));
    for (unsigned int i = 0; i < recordDescriptor.size(); i++){
        nullBit = nullFieldsIndicator[(int)(i/8)] & (1 << (8-(i%8)-1));
        if (!nullBit){
            type = (AttrType)recordDescriptor[i].type;
            char * varChar;
            switch (type) {
                case (TypeInt):
                    memcpy(intNum, (const char *)data + offset, sizeof(int));
                    offset += sizeof(int);
                    break;
                case (TypeReal):
                    memcpy(floatNum, (const char *)data + offset, sizeof(float));
                    offset += sizeof(float);
                    break;
                case (TypeVarChar):
                    memcpy(intNum, (const char *)data + offset, sizeof(int));
                    offset += sizeof(int);
                    varChar = (char *)calloc(intNum[0], sizeof(char));
                    memcpy(varChar, (const char *)data + offset, intNum[0] * sizeof(char));
                    offset += (intNum[0] * sizeof(char));
                    free(varChar);
                    break;
                default:
                    break;
            }
        }
    }
    cout << "this is the size of the offset " << offset <<endl;
    if(tableIndex.size() != 0){
        newSlot.offsetInBytes = tableIndex.back().offsetInBytes + offset;
        fseek (fileHandle.file , newSlot.offsetInBytes + 1, SEEK_SET);
        newSlot.ridNum.slotNum = tableIndex.back().ridNum.slotNum + 1;
    }
    else{
        newSlot.offsetInBytes = offset - 1;
        fseek(fileHandle.file , 0 , SEEK_SET);
        newSlot.ridNum.slotNum = 1;
    }
    printRecord(recordDescriptor, (const void*)data);
    cout << endl;
    newSlot.ridNum.pageNum = 1;
    fwrite((const char *)data, sizeof(char), offset, fileHandle.file);
    tableIndex.push_back(newSlot);
    free(intNum);
    free(floatNum);
    free(nullFieldsIndicator);
    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    for(int i = 0; i < (int)tableIndex.size(); i++){
        if(tableIndex[i].ridNum.pageNum == rid.pageNum && tableIndex[i].ridNum.slotNum == rid.slotNum){
            if(i == 0){
                fseek(fileHandle.file , 0 , SEEK_SET);
                fread((char *)data, sizeof(char), tableIndex[i].offsetInBytes + 1, fileHandle.file);
                
            }
            else{
                fseek(fileHandle.file , tableIndex[i-1].offsetInBytes + 1 , SEEK_SET);
                fread((char *)data, sizeof(char), tableIndex[i].offsetInBytes - tableIndex[i-1].offsetInBytes, fileHandle.file);
            }
        }
        else{
            perror("RID could not be found");
            return -1;
        }
    }
    return 0;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    int actualByteForNullsIndicator = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char * nullFieldsIndicator = (char *)malloc(actualByteForNullsIndicator);
    memcpy(nullFieldsIndicator, (const char *)data, sizeof(char));
    bool nullBit = false;
    AttrType type;
    int offset = actualByteForNullsIndicator;
    int *intNum = (int *)malloc(sizeof(int));
    float *floatNum = (float *)malloc(sizeof(float));
    char *varChar;
    for (unsigned int i = 0; i < recordDescriptor.size(); i++){
        nullBit = nullFieldsIndicator[(int)(i/8)] & (1 << (8-(i%8)-1));
        if (!nullBit){
            type = (AttrType)recordDescriptor[i].type;
            switch (type) {
                case (TypeInt):
                    memcpy(intNum, (const char *)data + offset, sizeof(int));
                    offset += sizeof(int);
                    cout << recordDescriptor[i].name << ": " << intNum[0] << "  ";
                    break;
                case (TypeReal):
                    memcpy(floatNum, (const char *)data + offset, sizeof(float));
                    offset += sizeof(float);
                    cout << recordDescriptor[i].name << ": " << floatNum[0] << "  ";
                    break;
                case (TypeVarChar):
                    memcpy(intNum, (const char *)data + offset, sizeof(int));
                    offset += sizeof(int);
                    varChar = (char *)malloc(intNum[0] * sizeof(char));
                    memcpy(varChar, (const char *)data + offset, intNum[0] * sizeof(char));
                    offset += (intNum[0] * sizeof(char));
                    cout << recordDescriptor[i].name << ": " << varChar << "  ";
                    free(varChar);
                    break;
                default:
                    break;
            }
        }
        else cout << recordDescriptor[i].name << ": " << "NULL  ";
    }
    cout << endl;
    if(tableIndex.size()!= 0){
        cout << tableIndex.back().offsetInBytes << endl;
    }
    free(intNum);
    free(floatNum);
    free(nullFieldsIndicator);
    return 0;
}
