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

RC RecordBasedFileManager::destroyFile(const string &fileName) {
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

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    if(fileHandle.file != nullptr){
        fprintf(stderr, "This file handle already has an open file!\n");
        return -1;
    }
    const char * c = fileName.c_str();
    FILE* check = fopen(c, "r");
    if(check == nullptr){
        fprintf(stderr, "Attempting to open file that does not exist!\n");
        return -1;
    }
    fclose(check);
    if((fileHandle.file = fopen(c, "rw+")) == nullptr){
        fprintf(stderr, "Error: %s", strerror(errno));
        return -1;
    }
    // void * buffer = malloc(sizeof(unsigned int));
    // Reads in the counters that are stored in the first 16 bytes of the file
    fread(&fileHandle.readPageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.writePageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.appendPageCounter, sizeof(unsigned), 1, fileHandle.file);
    fread(&fileHandle.pageCounter, sizeof(unsigned), 1, fileHandle.file);
    return 0;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    if(fileHandle.file == nullptr){
        fprintf(stderr, "This file handle has no open file\n");
        return -1;
    }
    // fseek(fileHandle.file, 0, SEEK_SET);
    rewind(fileHandle.file);
    // Writes the updated counters to the page
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

struct TableSlot{
    RID ridNum;
    int offsetInBytes;
};

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    
    void *writeBuffer = calloc(4096,sizeof(char));
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
    writeBufferOffset += numNotNull * sizeof(int) + actualByteForNullsIndicator + sizeof(int);
    //set up write buffer
    *intNum = (int)recordDescriptor.size();
    memcpy(writeBuffer, intNum, sizeof(int));
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
            *intNum = (int)writeBufferOffset;
            memcpy((char *)writeBuffer + sizeof(int) + actualByteForNullsIndicator + (fieldNum * sizeof(int)), intNum, sizeof(int));
            fieldNum ++;
        }
    }
    free(intNum);
    free(nullFieldsIndicator);
    unsigned pc = fileHandle.getNumberOfPages();
    int x = 0;
    // malloc size for reading pages
    void * buffer = calloc(PAGE_SIZE, sizeof(char));
    // Checks the return code for any filehandle functions
    int rc;
    // Check if there are any pages
    if(pc == 0){
        memcpy(buffer, writeBuffer, writeBufferOffset);
        struct TableSlot *newSlot = (struct TableSlot *)malloc(sizeof(struct TableSlot));
        newSlot->ridNum.pageNum = pc;
        newSlot->ridNum.slotNum = 1;
        newSlot->offsetInBytes = writeBufferOffset;
        memcpy((char *)buffer + 4076, newSlot, 12);
        memcpy((char *)buffer + 4092, &newSlot->offsetInBytes, 4);
        memcpy((char *)buffer + 4088, &newSlot->ridNum.slotNum, 4);
        rid = newSlot->ridNum;
        rc = fileHandle.appendPage(buffer);
        free(buffer);
        free(newSlot);
        free(writeBuffer);
        if(rc != 0){
            return -1;
        }
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
        freeSpace = (4096 - ((*slots + 1) * 12)) - *freeSpaceOffset - 8;
        // checks if the freespace is enough
        if(freeSpace >= writeBufferOffset){
            memcpy((char*)buffer + *freeSpaceOffset, writeBuffer, writeBufferOffset);
            struct TableSlot *newSlot =(struct TableSlot*) malloc(sizeof(struct TableSlot));
            newSlot->ridNum.pageNum = i;
            newSlot->ridNum.slotNum = *slots + 1;
            newSlot->offsetInBytes = *freeSpaceOffset + writeBufferOffset;
            memcpy((char*)buffer+(4088 - ((*slots + 1) * 12)), newSlot, sizeof(struct TableSlot));
            memcpy((char *)buffer + 4092, &newSlot->offsetInBytes, 4);
            memcpy((char *)buffer + 4088, &newSlot->ridNum.slotNum, 4);
            rid = newSlot->ridNum;
            rc =fileHandle.writePage(i, buffer);
            free(buffer);
            free(newSlot);
            free(writeBuffer);
            if(rc != 0){
                return -1;
            }
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
    memcpy((char *)buffer + 4092, &newSlot->offsetInBytes, 4);
    memcpy((char *)buffer + 4088, &newSlot->ridNum.slotNum, 4);
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
    // copies all the table slots into the table slot
    memcpy(foo, (char*)buffer + 4088 -(*slots * 12), *slots * 12);
    for(int i = 0; i < (int)*slots; i++){
        if(foo[i].ridNum.slotNum == rid.slotNum){
            void *recordBuffer;
            if(foo[i].ridNum.slotNum == 1){
                recordBuffer = (char *)calloc(foo[i].offsetInBytes ,sizeof(char));
                memcpy(recordBuffer, (char*)buffer, foo[i].offsetInBytes);
            }else{
                recordBuffer = (char *)calloc(foo[i].offsetInBytes - foo[i+1].offsetInBytes ,sizeof(char));
                memcpy(recordBuffer, (char*)buffer + foo[i+1].offsetInBytes, foo[i].offsetInBytes - foo[i+1].offsetInBytes);
            }
            //
            //start to decode the record data
            int *intNum = (int *)calloc(1 , sizeof(int));
            int *floatNum = (int *)calloc(1 , sizeof(float));
            int actualByteForNullsIndicator = ceil((double) recordDescriptor.size() / CHAR_BIT);
            char *nullFieldsIndicator = (char *)calloc(actualByteForNullsIndicator, sizeof(char));
            memcpy(nullFieldsIndicator, (const char *)data, actualByteForNullsIndicator * sizeof(char));
            bool nullBit = false;
            AttrType type;
            int dataOffset = 0;
            int recordOffset = 0;
            int fieldNum = 0;
            memcpy(intNum, (char *)recordBuffer, sizeof(int));
            int numOfFields = intNum[0];
            recordOffset += sizeof(int);
            memcpy((char *)data + dataOffset, (char *)recordBuffer + recordOffset, actualByteForNullsIndicator * sizeof(char));
            dataOffset += actualByteForNullsIndicator;
            recordOffset += actualByteForNullsIndicator;
            //find out how many not null attributes there are
            int numNotNull = 0;
            for (unsigned int j = 0; j < recordDescriptor.size(); j++){
                nullBit = nullFieldsIndicator[(int)(j/8)] & (1 << (8-(j%8)-1));
                if (!nullBit){
                    numNotNull++;
                }
            }
            recordOffset += numNotNull * sizeof(int);
            //start to write fields into data buffer
            //record offset basically becomes the offset of the previous field
            for(int j = 0; j < numOfFields; j++){
                nullBit = nullFieldsIndicator[(int)(j/8)] & (1 << (8-(j%8)-1));
                if (!nullBit){
                    type = (AttrType)recordDescriptor[j].type;
                    switch (type) {
                        case (TypeInt):
                            //get the offset from the field
                            memcpy(intNum, (const char *)recordBuffer + sizeof(int) + actualByteForNullsIndicator + fieldNum * sizeof(int), sizeof(int));
                            memcpy((char *)data + dataOffset, (const char *)recordBuffer + recordOffset, sizeof(int));
                            dataOffset += sizeof(int);
                            recordOffset = intNum[0];
                            break;
                        case (TypeReal):
                            //get the offset from the field
                            memcpy(intNum, (const char *)recordBuffer + sizeof(int) + actualByteForNullsIndicator + fieldNum * sizeof(int), sizeof(int));
                            memcpy((char *)data + dataOffset, (const char *)recordBuffer + recordOffset, sizeof(float));
                            dataOffset += sizeof(float);
                            recordOffset = intNum[0];
                            break;
                        case (TypeVarChar):
                            memcpy(intNum, (const char *)recordBuffer + sizeof(int) + actualByteForNullsIndicator + fieldNum * sizeof(int), sizeof(int));
                            int *numOfChars = (int *)calloc(1, sizeof(int));
                            *numOfChars = (int)((int)intNum[0] - (int)recordOffset);
                            memcpy((char *)data + dataOffset, numOfChars, sizeof(int));
                            dataOffset += sizeof(int);
                            memcpy((char *)data + dataOffset, (const char *)recordBuffer + recordOffset, numOfChars[0] * sizeof(char));
                            dataOffset += numOfChars[0] * sizeof(char);
                            recordOffset = intNum[0];
                            free(numOfChars);
                            break;
                    }
                    fieldNum ++;
                }
            }
            //
            free(foo);
            free(buffer);
            free(recordBuffer);
            free(intNum);
            free(floatNum);
            free(nullFieldsIndicator);
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
    free(intNum);
    free(floatNum);
    free(nullFieldsIndicator);
    return 0;
}
