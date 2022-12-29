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
    printSHTEntries(SHT_info);
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
    free(name);
    int blockNumber = sht_info->bucketToLastBlock[hashNumber];
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(sht_info->fileDesc , blockNumber , block));
    void* data = BF_Block_GetData(block);
    HT_block_info* blockInfo = data + sht_info->offset;

    if(blockInfo->numOfRecords == blockInfo->maxRecords){
        BF_Block* newBlock;
        BF_Block_Init(&newBlock);
        CALL_OR_DIE(BF_AllocateBlock(sht_info->fileDesc , newBlock));
        void* newData = BF_Block_GetData(newBlock);
        int blocks_num;
        CALL_OR_DIE(BF_GetBlockCounter(sht_info->fileDesc , &blocks_num));

        SHT_block_info* newBlockInfo = malloc(sizeof(SHT_block_info));
        newBlockInfo->blockNumber = blocks_num - 1;
        newBlockInfo->numOfRecords = 1;
        newBlockInfo->maxRecords = (512 - sizeof(SHT_block_info)) / sizeof(shtTuple);
        newBlockInfo->previousBlockNumber = blockInfo->blockNumber ;
        memcpy(newData + sht_info->offset , newBlockInfo , sizeof(SHT_block_info));
        free(newBlockInfo);

        shtTuple tuple;
        memcpy(tuple.strName , record.name , strlen(record.name) + 1);
        tuple.blockIndex = block_id;

        shtTuple* shtTuple1 = newData;
        shtTuple1[0] = tuple;

        sht_info->bucketToLastBlock[hashNumber] = blocks_num - 1;

        BF_Block_SetDirty(newBlock);
        CALL_OR_DIE(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);

    }else{
        shtTuple tuple;
        memcpy(tuple.strName , record.name , strlen(record.name) + 1);
        tuple.blockIndex = block_id;

        shtTuple* shtTuple1 = data;
        shtTuple1[blockInfo->numOfRecords] = tuple;
        blockInfo->numOfRecords += 1;
    }
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return 0;
}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name){

}

void printSHTEntries(SHT_info* shtInfo){
    BF_Block* block;
    BF_Block_Init(&block);
    void* data;
    for(int i = 0 ; i < shtInfo->numOfBuckets ; i++){
        printf("Bucket number %d with last bucket number %d has these blocks and records\n",i,shtInfo->bucketToLastBlock[i]);
        int lastBlock = shtInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc , lastBlock , block));
        data = BF_Block_GetData(block);
        shtTuple* shtTuple1 = data;
        SHT_block_info* shtBlockInfo = data + shtInfo->offset ;
        while(1){
            for(int j = 0 ; j < shtBlockInfo->numOfRecords ; j++){
                printf("\tRecord with name %s and blockID %d\n",shtTuple1[j].strName , shtTuple1[j].blockIndex);
            }
            CALL_OR_DIE(BF_UnpinBlock(block));
            if(shtBlockInfo->previousBlockNumber == -1)
                break;
            CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc , shtBlockInfo->previousBlockNumber , block));
            data = BF_Block_GetData(block);
            shtTuple1 = data;
            shtBlockInfo = data + shtInfo->offset;
        }
    }
    BF_Block_Destroy(&block);
}

int SHT_HashFunction(unsigned char* string , int numOfBuckets){
//    unsigned long hash = 5381;
//    int c;
//
//    while ((c = *string++))
//        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//    int hashNumber = (int)(hash % numOfBuckets);
//    return hashNumber;
//    unsigned long hash = 0;
//    int c;
//
//    while ((c = *string++))
//        hash = c + (hash << 6) + (hash << 16) - hash;
//    return (int)(hash % numOfBuckets);
    long hash = 0 ;
    int c;
    while ((c = *string++))
        hash += c;
    return (int)(hash % numOfBuckets);
}


