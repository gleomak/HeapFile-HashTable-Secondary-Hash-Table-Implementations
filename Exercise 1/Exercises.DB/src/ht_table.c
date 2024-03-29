#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return -1;              \
    }                         \
  }

#define CALL_OR_DIE2(call)    \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return NULL;            \
    }                         \
  }


int HT_CreateFile(char *fileName, int buckets) {
    /*Init of the HT file */
    int fd1;
    BF_Block *htBlock;
    BF_Block_Init(&htBlock);
    CALL_OR_DIE(BF_CreateFile(fileName))
    CALL_OR_DIE(BF_OpenFile(fileName, &fd1));
    CALL_OR_DIE(BF_AllocateBlock(fd1, htBlock));  // Δημιουργία καινούριου block
    void *data = BF_Block_GetData(htBlock);

    /*We initialize the htInfo that will be saved in the first block of the ht file*/
    HT_info *htInfo = malloc(sizeof(HT_info));
    htInfo->fileDesc = fd1;
    htInfo->offset = 512 - sizeof(HT_block_info);
    htInfo->fileType = "HT";
    htInfo->numOfBuckets = buckets;
    htInfo->firstBlock = NULL;
    void *data2;
    BF_Block *htBlock2;
    BF_Block_Init(&htBlock2);

    /*We allocate and initialize a block for each bucket*/
    for (int i = 0; i < buckets; i++) {
        CALL_OR_DIE(BF_AllocateBlock(fd1, htBlock2));  // Δημιουργία καινούριου block
        data2 = BF_Block_GetData(htBlock2);
        HT_block_info *htBlockInfo = malloc(sizeof(HT_block_info));
        htBlockInfo->blockNumber = i + 1;
        htBlockInfo->numOfRecords = 0;
        htBlockInfo->maxRecords = (512 - sizeof(HT_block_info)) / sizeof(Record);
        htBlockInfo->previousBlockNumber = -1;
        memcpy(data2 + htInfo->offset, htBlockInfo, sizeof(HT_block_info));
        htInfo->bucketToLastBlock[i] = i + 1;
        free(htBlockInfo);

        BF_Block_SetDirty(htBlock2);
        CALL_OR_DIE(BF_UnpinBlock(htBlock2));
    }
    memcpy(data, htInfo, sizeof(HT_info));
    free(htInfo);

    BF_Block_SetDirty(htBlock);
    CALL_OR_DIE(BF_UnpinBlock(htBlock));

    /*Destroying the block that we used*/
    BF_Block_Destroy(&htBlock);
    BF_Block_Destroy(&htBlock2);

    /*Closing the file after the creation*/
    CALL_OR_DIE(BF_CloseFile(fd1));

    return 0;
}

HT_info *HT_OpenFile(char *fileName) {
    /*Re-opening the file*/
    HT_info *htInfo;
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE2(BF_OpenFile(fileName, &fd1));

    /*Get and pin the first block and update the fileDesc/first-block pointer values*/
    void *data;
    CALL_OR_DIE2(BF_GetBlock(fd1, 0, block));
    data = BF_Block_GetData(block);
    htInfo = data;
    htInfo->fileDesc = fd1;
    htInfo->firstBlock = block;
    if (strcmp(htInfo->fileType, "HT") != 0) {
        printf("This is not a HT file\n");
        return NULL;
    }
    return htInfo;
}


int HT_CloseFile(HT_info *HT_info) {
//    printHTEntries(HT_info);
    if (HT_info == NULL)
        return -1;
    /*Freeing the hash-table , Setting dirty/unpinning the first block ,closing the file*/
    BF_Block_SetDirty(HT_info->firstBlock);
    CALL_OR_DIE(BF_UnpinBlock(HT_info->firstBlock));
    BF_Block_Destroy(&HT_info->firstBlock);
    CALL_OR_DIE(BF_CloseFile(HT_info->fileDesc));
    return 0;
}

int HT_InsertEntry(HT_info *ht_info, Record record) {
    /*Hashing the record.id , finding the last block of the bucket*/
    int hashNumber = record.id % ht_info->numOfBuckets;
    int blockNumber = ht_info->bucketToLastBlock[hashNumber];

    int insertedInBlock; // Block that the record is going to be inserted

    /*Pinning the last block and its contents*/
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, blockNumber, block));
    void *data = BF_Block_GetData(block);
    HT_block_info *blockInfo = data + ht_info->offset;

    /*If the last block is full create a new block*/
    if (blockInfo->numOfRecords == blockInfo->maxRecords) {
        BF_Block *newBlock;
        BF_Block_Init(&newBlock);
        CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, newBlock));  // Δημιουργία καινούριου block
        void *newData = BF_Block_GetData(newBlock);
        int blocks_num;
        CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &blocks_num));

        HT_block_info *newBlockInfo = malloc(sizeof(HT_block_info));
        newBlockInfo->blockNumber = blocks_num - 1;
        newBlockInfo->numOfRecords = 1;
        newBlockInfo->maxRecords = (512 - sizeof(HT_block_info)) / sizeof(Record);
        newBlockInfo->previousBlockNumber = blockInfo->blockNumber;
        memcpy(newData + ht_info->offset, newBlockInfo, sizeof(HT_block_info));
        free(newBlockInfo);

        Record *rec = newData;
        rec[0] = record; // insertion of the record in the new Block
        insertedInBlock = blocks_num - 1;

        /*Updating the last block of the desired bucket*/
        ht_info->bucketToLastBlock[hashNumber] = blocks_num - 1;

        BF_Block_SetDirty(newBlock);
        CALL_OR_DIE(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);
    } else { //if the block has space insert the record and update its contents
        Record *rec = data;
        rec[blockInfo->numOfRecords] = record;
        blockInfo->numOfRecords += 1;
        insertedInBlock = blockInfo->blockNumber;
    }
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return insertedInBlock;
}

/*Auxiliary function that prints all the entries for every bucket and all their blocks*/
int printHTEntries(HT_info *htInfo) {
    BF_Block *block;
    BF_Block_Init(&block);
    void *data;
    for (int i = 0; i < htInfo->numOfBuckets; i++) {
        printf("Bucket number %d with last block number %d has these blocks and records \n", i,
               htInfo->bucketToLastBlock[i]);
        int lastBlock = htInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc, lastBlock, block));
        data = BF_Block_GetData(block);
        Record *rec = data;
        HT_block_info *htBlockInfo = data + htInfo->offset;
        while (1) {
            printf(" Block number : %d from HT file with predecessor block %d has :\n", htBlockInfo->blockNumber,
                   htBlockInfo->previousBlockNumber);

            for (int j = 0; j < htBlockInfo->numOfRecords; j++) {
                printf("\tRecord number %d of Block %d is ", j, htBlockInfo->blockNumber);
                printRecord(rec[j]);
            }
            CALL_OR_DIE(BF_UnpinBlock(block));
            if (htBlockInfo->previousBlockNumber == -1)
                break;
            CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc, htBlockInfo->previousBlockNumber, block));
            data = BF_Block_GetData(block);
            htBlockInfo = data + htInfo->offset;
            rec = data;
        }
    }
    BF_Block_Destroy(&block);
    return 0;
}

int HT_GetAllEntries(HT_info *ht_info, int value) {
    BF_Block *block;
    BF_Block_Init(&block);
    void *data;

    /*Hashing the value getting its data*/
    int hashNumber = value % ht_info->numOfBuckets;
    int lastBlock = ht_info->bucketToLastBlock[hashNumber];
    CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, lastBlock, block));
    data = BF_Block_GetData(block);
    Record *rec = data;
    HT_block_info *htBlockInfo = data + ht_info->offset;
    int blocksRead = 0;

    /*While that breaks if we reached the first created block of the desired bucket*/
    while (1) {
        blocksRead++;
        for (int i = 0; i < htBlockInfo->numOfRecords; i++) {
            if (rec[i].id == value) { // we found the desired record
                printf("Found value: %d,in block: %d, of hash %d\n", value, htBlockInfo->blockNumber, hashNumber);
                printRecord(rec[i]);
                CALL_OR_DIE(BF_UnpinBlock(block));
                BF_Block_Destroy(&block);
                return blocksRead;
            }
        }
        CALL_OR_DIE(BF_UnpinBlock(block));
        if (htBlockInfo->previousBlockNumber == -1) { // We reached the first block of the bucket
            break;
        }

        /*Getting and pinning the predecessor of this block*/
        CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, htBlockInfo->previousBlockNumber, block));
        data = BF_Block_GetData(block);
        htBlockInfo = data + ht_info->offset;
        rec = data;
    }
    BF_Block_Destroy(&block);

    /*If the code reaches here that means we did not find the ID we looked for*/
    return -1;
}

int HashStatisticsHT(char *filename) {
    HT_info *htInfo = HT_OpenFile(filename);

    BF_Block *block;
    BF_Block_Init(&block);
    void *data;

    int allBlocks;
    BF_GetBlockCounter(htInfo->fileDesc, &allBlocks);
    printf("HT file with a total of %d buckets and %d blocks has:\n", htInfo->numOfBuckets, allBlocks);

    int minRecords, maxRecords, averageRecords;
    int overflowBlocks;
    int totalBlocks = 0;
    int bucketBlocks;
    int bucketsWithOverFlowBlocks = 0;
    for (int i = 0; i < htInfo->numOfBuckets; i++) {
        int lastBlock = htInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc, lastBlock, block));
        data = BF_Block_GetData(block);
        HT_block_info *htBlockInfo = data + htInfo->offset;

        minRecords = maxRecords = htBlockInfo->numOfRecords;
        averageRecords = overflowBlocks = bucketBlocks = 0;
        while (1) {
            if (htBlockInfo->numOfRecords < minRecords)
                minRecords = htBlockInfo->numOfRecords;
            if (htBlockInfo->numOfRecords > maxRecords)
                maxRecords = htBlockInfo->numOfRecords;
            totalBlocks += 1;
            bucketBlocks += 1;
            averageRecords += htBlockInfo->numOfRecords;
            CALL_OR_DIE(BF_UnpinBlock(block));
            if (htBlockInfo->previousBlockNumber == -1)
                break;
            bucketsWithOverFlowBlocks += 1;
            overflowBlocks += 1;
            CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc, htBlockInfo->previousBlockNumber, block));
            data = BF_Block_GetData(block);
            htBlockInfo = data + htInfo->offset;
        }
        printf("\tBucket number %d with a total of %d blocks( %d of them are overflow blocks) has min records: %d, max records: %d, average Records: %d\n",
               i, bucketBlocks, overflowBlocks, minRecords,
               maxRecords, averageRecords / bucketBlocks);
    }
    printf("Average number of blocks a bucket has is: %d\n\n", totalBlocks / htInfo->numOfBuckets);
    printHTEntries(htInfo);
    BF_Block_Destroy(&block);
    HT_CloseFile(htInfo);
    return 0;
}




