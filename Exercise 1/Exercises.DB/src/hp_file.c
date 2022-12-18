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
    memcpy(data , info , sizeof(HP_info));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    CALL_BF(BF_CloseFile(fd1));               //Κλείσιμο αρχείου και αποδέσμευση μνήμης
    CALL_BF(BF_Close());
    BF_Block_Destroy(&block);
//    free(info);
    return 0;
}

HP_info* HP_OpenFile(char *fileName){
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_Init(LRU));
    CALL_BF(BF_OpenFile(fileName, &fd1));
    void* data;
    CALL_BF(BF_GetBlock(fd1, 0, block));
    data = BF_Block_GetData(block);
    HP_info* hpInfo = data;
    printf("is hp %d , fd is %d , number of last block is %d \n",hpInfo->isHP,hpInfo->fileDesc,hpInfo->lastBlock);
    if(hpInfo->isHP != 1){
        printf("This is not a HP file \n");
        return NULL;
    }
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return hpInfo ;
}


int HP_CloseFile( HP_info* hp_info ){
    int fd1 = hp_info->fileDesc;
    printf("FD IS %d \n",fd1);
    CALL_BF(BF_CloseFile(fd1));
//    CALL_BF(BF_Close());
    return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record){
    BF_Block *firstBlock;
    BF_Block_Init(&firstBlock);
    CALL_BF(BF_GetBlock(hp_info->fileDesc, 0, firstBlock));

    int offset = 512 - sizeof(HP_block_info) + 1;

    BF_Block *lastBlock;
    BF_Block_Init(&lastBlock);
    void* data;
    CALL_BF(BF_GetBlock(hp_info->fileDesc , hp_info->lastBlock , lastBlock));
    data = BF_Block_GetData(lastBlock);
    HP_block_info* blockInfo = data + offset;

    if(hp_info->lastBlock == 0 || blockInfo->numOfRecords == blockInfo->maxRecords){
        int newLastBlock = hp_info->lastBlock + 1;
        hp_info->lastBlock = newLastBlock;
        BF_Block *secondBlock;
        BF_Block_Init(&secondBlock);
        CALL_BF(BF_AllocateBlock(hp_info->fileDesc, secondBlock));  // Δημιουργία καινούριου block
        void* data2 = BF_Block_GetData(secondBlock);

    }
    return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

