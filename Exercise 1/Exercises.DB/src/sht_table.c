#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "sht_table.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }



int SHT_CreateSecondaryIndex(char *sfileName,  int buckets, char* fileName){
    int fd1;
    BF_Block* shtBlock;
    BF_Block_Init(&shtBlock);
    CALL_OR_DIE(BF_CreateFile(sfileName))
    CALL_OR_DIE(BF_OpenFile(sfileName, &fd1));
    CALL_OR_DIE(BF_AllocateBlock(fd1, shtBlock));  // Δημιουργία καινούριου block
    void* data = BF_Block_GetData(shtBlock);

    printf("Size of SHT info is : %lu\n", sizeof(SHT_info));
    SHT_info* shtInfo = malloc(sizeof(SHT_info));
    shtInfo->fileDesc = fd1;
    shtInfo->offset = 512 - sizeof(SHT_block_info);
    shtInfo->numOfBuckets = buckets;
    shtInfo->firstBlock = NULL;
    shtInfo->bucketToLastBlock = malloc(buckets * sizeof(int));
    void* data2;
    BF_Block* shtBlock2;
    BF_Block_Init(&shtBlock2);
    for(int i = 0 ; i < buckets ; i++){
        CALL_OR_DIE(BF_AllocateBlock(fd1 , shtBlock2));
        data2 = BF_Block_GetData(shtBlock2);
        SHT_block_info* shtBlockInfo = malloc(sizeof(SHT_block_info));
        shtBlockInfo->blockNumber = i + 1;
        shtBlockInfo->previousBlockNumber = -1;
        shtBlockInfo->numOfRecords = 0;
        shtBlockInfo->maxRecords = (512 - sizeof(SHT_block_info)) / sizeof(shtTuple);
        memcpy(data2 + shtInfo->offset , shtBlockInfo , sizeof(SHT_block_info));

        shtInfo->bucketToLastBlock[i] = i + 1;
        free(shtBlockInfo);
        BF_Block_SetDirty(shtBlock2);
        CALL_OR_DIE(BF_UnpinBlock(shtBlock2));
    }
    memcpy(data , shtInfo , sizeof(SHT_info));
    free(shtInfo);

    BF_Block_SetDirty(shtBlock);
    CALL_OR_DIE(BF_UnpinBlock(shtBlock));

    BF_Block_Destroy(&shtBlock);
    BF_Block_Destroy(&shtBlock2);

    CALL_OR_DIE(BF_CloseFile(fd1));

    return 0;

}

SHT_info* SHT_OpenSecondaryIndex(char *indexName){
    SHT_info* shtInfo;
    int fd1;
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_OpenFile(indexName , &fd1));
    void* data;
    CALL_OR_DIE(BF_GetBlock(fd1 , 0 , block));
    data = BF_Block_GetData(block);
    shtInfo = data;
    shtInfo->fileDesc = fd1;
    shtInfo->firstBlock = block;
    return shtInfo;
}


int SHT_CloseSecondaryIndex( SHT_info* SHT_info ){
    printf("ID in SHT is : %d \n",SHT_info->fileDesc);
    if(SHT_info == NULL)
        return -1;
    free(SHT_info->bucketToLastBlock);
    BF_Block_SetDirty(SHT_info->firstBlock);
    CALL_OR_DIE(BF_UnpinBlock(SHT_info->firstBlock));
    BF_Block_Destroy(&SHT_info->firstBlock);
    CALL_OR_DIE(BF_CloseFile(SHT_info->fileDesc));
    return 0;
}

int SHT_SecondaryInsertEntry(SHT_info* sht_info, Record record, int block_id){
    unsigned char *name = (unsigned char*) malloc(15);
    memcpy(name , record.name , strlen(record.name) + 1);
    int hashNumber = SHT_HashFunction(name , sht_info->numOfBuckets);
    printf("HashNumber is : %d, name is : %s\n",hashNumber , name);



    return 1;
}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name){

}

int SHT_HashFunction(unsigned char* string , int numOfBuckets){
//    unsigned long hash = 5381;
//    int c;
//
//    while ((c = *string++))
//        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//    int hashNumber = (int)(hash % numOfBuckets);
//    return hashNumber;
    unsigned long hash = 0;
    int c;

    while ((c = *string++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return (int)(hash % numOfBuckets);
}



