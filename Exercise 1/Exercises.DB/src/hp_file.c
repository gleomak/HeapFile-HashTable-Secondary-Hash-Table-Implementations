#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    exit(code);              \
  }                         \
}

int HP_CreateFile(char *fileName){
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_CreateFile(fileName))
    CALL_BF(BF_OpenFile(fileName, &fd1));
    void* data;

    CALL_BF(BF_AllocateBlock(fd1, block));  // Δημιουργία καινούριου block
    data = BF_Block_GetData(block);
    HP_info* info = malloc(sizeof(HP_info));
    info->isHP = 1;
    info->fileDesc = fd1;
    info->lastBlock = 0;
    info->offset = 512 - sizeof(HP_block_info)+ 1;
    info->firstBlock = NULL;
    memcpy(data , info , sizeof(HP_info));
    printf("Offset is : %d\n",info->offset);
    HP_block_info* info1 = malloc(sizeof(HP_block_info));
    info1->numOfRecords = 0;
    info1->maxRecords = (512 - sizeof(HP_block_info)) / sizeof(Record);
    info1->nextBlock = NULL;
    memcpy(data+info->offset , info1 , sizeof(HP_block_info));

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    CALL_BF(BF_CloseFile(fd1));               //Κλείσιμο αρχείου και αποδέσμευση μνήμης
//    CALL_BF(BF_Close());
    BF_Block_Destroy(&block);
    free(info);
    free(info1);
    return 0;
}

HP_info* HP_OpenFile(char *fileName){
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_OpenFile(fileName, &fd1));
    void* data;
    CALL_BF(BF_GetBlock(fd1, 0, block));
    data = BF_Block_GetData(block);
    HP_info* hpInfo = data;
    hpInfo->firstBlock = block;
    printf("is hp %d , fd is %d , offset is %d , number of last block is %d \n",hpInfo->isHP,hpInfo->fileDesc,hpInfo->offset,hpInfo->lastBlock);
    HP_block_info* hpBlockInfo = data + hpInfo->offset;
    printf("Max Records is %d\n" , hpBlockInfo->maxRecords);
    if(hpInfo->isHP != 1){
        printf("This is not a HP file \n");
        return NULL;
    }
    return hpInfo ;
}


int HP_CloseFile( HP_info* hp_info ){
    int fd1 = hp_info->fileDesc;
//    printf("FD IS %d , last block is %d\n",fd1 , hp_info->lastBlock);
//    int numOfBuckets;
//    CALL_BF(BF_GetBlockCounter(fd1, &numOfBuckets));
//    void* data;
//    BF_Block* block;
//    BF_Block_Init(&block);
//    for(int i = 0 ; i < numOfBuckets ; i++){
//        CALL_BF(BF_GetBlock(fd1, i, block));
//        data = BF_Block_GetData(block);
//        Record* rec = data;
//
//    }
    printf("size of block info : %lu\n",sizeof(HP_block_info));
    BF_Block_SetDirty(hp_info->firstBlock);
    CALL_BF(BF_UnpinBlock(hp_info->firstBlock));
    BF_Block_Destroy(&hp_info->firstBlock);

    CALL_BF(BF_CloseFile(fd1));
    return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record){
    BF_Block *lastBlock;
    void* data;
    HP_block_info* lastBlockInfo;
    if(hp_info->lastBlock == 0 ) {
        data = hp_info->firstBlock;
        lastBlockInfo = data + hp_info->offset;
    }else {
        BF_Block_Init(&lastBlock);
        CALL_BF(BF_GetBlock(hp_info->fileDesc, hp_info->lastBlock, lastBlock));
        data = BF_Block_GetData(lastBlock);
        lastBlockInfo = data + hp_info->offset;
    }
    if(hp_info->lastBlock == 0 || lastBlockInfo->numOfRecords == lastBlockInfo->maxRecords){
        BF_Block *newBlock;
        BF_Block_Init(&newBlock);
        CALL_BF(BF_AllocateBlock(hp_info->fileDesc, newBlock));  // Δημιουργία καινούριου block
        void* data2 = BF_Block_GetData(newBlock);
        HP_block_info* hpBlockInfo = malloc(sizeof(HP_block_info));
        hpBlockInfo->maxRecords = (512 - sizeof(HP_block_info)) / sizeof(Record);
        hpBlockInfo->numOfRecords = 1;
        hpBlockInfo->nextBlock = NULL;
        memcpy(data2+hp_info->offset , hpBlockInfo , sizeof(HP_block_info));
        free(hpBlockInfo);

        lastBlockInfo->nextBlock = newBlock;
        hp_info->lastBlock += 1;
        Record* rec = data2;
        rec[0] = record;
        BF_Block_SetDirty(newBlock);
        CALL_BF(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);
    }else{
        Record* rec = data;
        rec[lastBlockInfo->numOfRecords] = record;
        lastBlockInfo->numOfRecords += 1;
    }
    if(lastBlockInfo->numOfRecords != 0) {
        BF_Block_SetDirty(lastBlock);
        CALL_BF(BF_UnpinBlock(lastBlock));
        BF_Block_Destroy(&lastBlock);
    }
//    printf("Record , last block is %d\n",hp_info->lastBlock);
    return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

