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

struct TableSlot{
    RID ridNum;
    int offsetInBytes;
};

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    void *writeBuffer = calloc(2000,sizeof(char));
    int writeBufferOffset= 0;
    int fieldNum = 0;
    // ok
    int actualByteForNullsIndicator = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char *nullFieldsIndicator = (char *)calloc(actualByteForNullsIndicator, sizeof(char));
    memcpy(nullFieldsIndicator, (const char *)data, actualByteForNullsIndicator * sizeof(char));
    bool nullBit = false;
    AttrType type;
    int offset = actualByteForNullsIndicator;
    int *intNum = (int *)calloc(1, sizeof(int));
    //find out how many not null attributes there are
    int numNotNull = 0;
    for (unsigned int i = 0; i < recordDescriptor.size(); i++){
        nullBit = nullFieldsIndicator[(int)(i/8)] & (1 << (8-(i%8)-1));
        if (!nullBit){
            numNotNull++;
        }
    }
    writeBufferOffset += numNotNull * sizeof(int);
    //set up write buffer
    memset(writeBuffer, recordDescriptor.size(), sizeof(int));
    memcpy((char *)writeBuffer + 1,(const char *)data, actualByteForNullsIndicator *sizeof(char));
    for (unsigned int i = 0; i < recordDescriptor.size(); i++){
        nullBit = nullFieldsIndicator[(int)(i/8)] & (1 << (8-(i%8)-1));
        if (!nullBit){
            type = (AttrType)recordDescriptor[i].type;
            switch (type) {
                case (TypeInt):
                    memcpy((char *)writeBuffer + writeBufferOffset, (const char *)data + offset, sizeof(int));
                    writeBufferOffset+= sizeof(int);
                    offset += sizeof(int);
                    break;
                case (TypeReal):
                    memcpy((char *)writeBuffer + writeBufferOffset, (const char *)data + offset, sizeof(float));
                    writeBufferOffset+= sizeof(float);
                    offset += sizeof(float);
                    break;
                case (TypeVarChar):
                    memcpy(intNum, (const char *)data + offset, sizeof(int));
                    offset += sizeof(int);
                    memcpy((char *)writeBuffer + writeBufferOffset, (const char *)data + offset, intNum[0] * sizeof(char));
                    writeBufferOffset+= (intNum[0] * sizeof(char));
                    offset += (intNum[0] * sizeof(char));
                    break;
                default:
                    break;
            }
            memset((char *)writeBuffer + sizeof(int) + actualByteForNullsIndicator + (fieldNum * sizeof(int)), writeBufferOffset, sizeof(int));
            fieldNum ++;
        }
    }
    printRecord(recordDescriptor, (const void*)data);
    cout << endl;
    free(intNum);
    free(nullFieldsIndicator);
    unsigned pc = fileHandle.getNumberOfPages();
    int x = 0;
    // malloc size for reading pages
    void * buffer = malloc(PAGE_SIZE);
    // Check if there are any pages
    if(pc == 0){
        memcpy(buffer, writeBuffer, writeBufferOffset);
        struct TableSlot *newSlot = (struct TableSlot *)malloc(sizeof(struct TableSlot));
        newSlot->ridNum.pageNum = pc;
        newSlot->ridNum.slotNum = 1;
        newSlot->offsetInBytes = writeBufferOffset;
        memcpy((char *)buffer + 4076, newSlot, 12);
        memset((char *)buffer + 4092, newSlot->offsetInBytes, 1);
        memset((char *)buffer + 4088, 1, 1);
        rid = newSlot->ridNum;
        fileHandle.appendPage(buffer);
        free(buffer);
        free(newSlot);
        free(writeBuffer);
        return 0;
    }
    int* freeSpaceOffset;
    unsigned * slots;
    int freeSpace;
    // read each page and determine if there is enough freespace
    for(unsigned i = 0; i < pc; i++){
        fileHandle.readPage(i, buffer);
        // gets the number of free space
        freeSpaceOffset =(int*) ((char*)buffer + 4092);
        slots = (unsigned *) ((char*)buffer + 4088);
        freeSpace = (4096 - ((*slots + 1) * 12)) - *freeSpaceOffset;
        // checks if the freespace is enough
        if(freeSpace > x){
            //do the writing
            memcpy((char*)buffer + *freeSpaceOffset, writeBuffer, writeBufferOffset);
            struct TableSlot *newSlot =(struct TableSlot*) malloc(sizeof(struct TableSlot));
            newSlot->ridNum.pageNum = i;
            newSlot->ridNum.slotNum = *slots + 1;
            newSlot->offsetInBytes = *freeSpaceOffset + writeBufferOffset;
            memcpy((char*)buffer+(4088 - ((*slots + 1) * 12)), newSlot, 12);
            memset((char*)buffer+4092,newSlot->offsetInBytes, 1);
            memset((char*)buffer+4088,(int) *slots + 1, 1);
            rid = newSlot->ridNum;
            fileHandle.writePage(pc, buffer);
            free(buffer);
            free(newSlot);
            free(writeBuffer);
            return 0;
        }
        memset(buffer, 0, 4096);
    }
    // if none of the pages have enough size append new file with correct data
    memcpy(buffer, writeBuffer, writeBufferOffset);
    struct TableSlot *newSlot = (struct TableSlot *)malloc(sizeof(struct TableSlot));
    newSlot->ridNum.pageNum = pc;
    newSlot->ridNum.slotNum = 1;
    newSlot->offsetInBytes = writeBufferOffset;
    memcpy((char *)buffer + 4076, newSlot, 12);
    memset((char *)buffer + 4092, newSlot->offsetInBytes, 1);
    memset((char *)buffer + 4088, 1, 1);
    rid = newSlot->ridNum;
    fileHandle.appendPage(buffer);
    free(buffer);
    free(newSlot);
    free(writeBuffer);
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
    unsigned* slots;
    slots = (unsigned*)((char*)buffer + 4088);
    if(*slots == 0){
        fprintf(stderr, "No records are stored on this page!\n");
        free(buffer);
        return -1;
    }
    struct TableSlot *foo =(struct TableSlot*) malloc(12*(*slots));
    cout << *slots << endl;
    // copies all the table slots into the table slot
    memcpy(foo, (char*)buffer + 4088 -(*slots * 12), *slots * 12);
    for(int i = 0; i < (int)*slots; i++){
        cout << foo[i].ridNum.slotNum << endl;
        if(foo[i].ridNum.slotNum == rid.slotNum){
<<<<<<< HEAD
            cout << "This is the offset: " << foo[i].offsetInBytes << endl;
            
=======
            if(i == 1){
                memcpy(data, (char*)buffer, foo[i].offsetInBytes);
            }else{
                memcpy(data, (char*)buffer + foo[i-1].offsetInBytes, foo[i].offsetInBytes - foo[i-1].offsetInBytes);
            }
            free(foo);
>>>>>>> kiran
            free(buffer);
            return 0;
        }
    }
    fprintf(stderr, "Invalid rid: Slot number not found on page!\n");
    free(foo);
    free(buffer);
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    int actualByteForNullsIndicator = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char * nullFieldsIndicator = (char *)calloc(1, actualByteForNullsIndicator);
    memcpy(nullFieldsIndicator, (const char *)data, sizeof(char));
    bool nullBit = false;
    AttrType type;
    int offset = actualByteForNullsIndicator;
    int *intNum = (int *)calloc(1, sizeof(int));
    float *floatNum = (float *)calloc(1, sizeof(float));
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
                    varChar = (char *)calloc(intNum[0], sizeof(char));
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
    free(intNum);
    free(floatNum);
    free(nullFieldsIndicator);
    return 0;
}
