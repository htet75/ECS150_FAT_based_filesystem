#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

struct superblock{
	char signature[8]; // Signature ( must be equal to "ECS150FS")
	uint16_t total_disk_blocks; // Total amount of blocks of virtual disk
	uint16_t root_dir_index; // Root directory block index
	uint16_t data_block_start_index; // Data block start index
	uint16_t total_data_blocks; // Amount of data blocks
	uint8_t total_FAT_blocks; // Number of blocks for FAT
	uint8_t padding[4079]; // Unused/Padding
}__attribute__((packed));

#define FAT_EOC 0xFFFF // End-of-Chain value
#define FAT_ENTRIES_PER_BLOCK 2048 // Number of FAT entries per block

uint16_t* FAT; // pointer to FAT array

/* Initializing FAT array */
void init_FAT(uint16_t total_blocks)
{
	int num_FAT_blocks = (total_blocks + FAT_ENTRIES_PER_BLOCK -1 ) / FAT_ENTRIES_PER_BLOCK;
	FAT = calloc(num_FAT_blocks * FAT_ENTRIES_PER_BLOCK, sizeof(uint16_t)); // allocating memory for FAT
	FAT[0] = FAT_EOC; // Setting first entry to END-of-Chain value
}

/* Getting the next block in the chain of FAT array */
uint16_t get_next_block(uint16_t block_index)
{
	uint16_t entry = FAT[block_index];
	if(entry >= FAT_EOC)
		return FAT_EOC; // End-of-cain
	return entry;	
}

/* FAT: set next block in the chain */
void set_next_block(uint16_t block_index, uint16_t next_block_index)
{
	FAT[block_index] = next_block_index;
}

/* FAT: get the block index of the first block of a file */
uint16_t get_first_block_index(uint16_t file_block_index) {
    return file_block_index + 1; // First block index is one past the file block index
}

/* FAT: get the block index of the last block of a file */
uint16_t get_last_block_index(uint16_t first_block_index) {
    uint16_t block_index = first_block_index;
    while (get_next_block(block_index) != FAT_EOC) {
        block_index = get_next_block(block_index);
    }
    return block_index;
}

/* FAT: allocating a new block for a file */
uint16_t allocate_block(uint16_t prev_block_index, uint16_t num_blocks_allocated) {
    uint16_t block_index = prev_block_index;
    for (int i = 0; i < num_blocks_allocated; i++) {
        uint16_t next_block_index = get_next_block(block_index);
        if (next_block_index == FAT_EOC) {
            uint16_t new_block_index = block_index + 1;
            set_next_block(block_index, new_block_index);
            set_next_block(new_block_index, FAT_EOC);
            block_index = new_block_index;
        } else {
            block_index = next_block_index;
        }
    }
    return block_index;
}

/* Root Directory data structure */
struct root_dir_entry {
	char filename[FS_FILENAME_LEN];	// Filename ( including NULL character )
	uint32_t size; // Size of the file (in bytes)
	uint16_t first_datablock_index; // Index of the first data block
	char padding[10];
}__attribute__((packed));

/* Global variables to used in the file system */
static int fs_mounted = 0;
static struct superblock sb; // superblock of the mounter file system
struct root_dir_entry root_dir[FS_FILE_MAX_COUNT]; //root directory of the mounter file system

/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	/* Opening virtual disk file */
	if(block_disk_open(diskname)==-1)
		return -1;

	/* Read the superblock */
	if (block_read(0, &sb) == -1)
		return -1;

	/* Verify the signature of the file system */
	if(strncmp((char*)sb.signature, "ECS150FS", 8) != 0)
		return -1; //Incorrect signature

	/* verify the size of the virtual disk and the block size */
	if(block_disk_count() != sb.total_disk_blocks)
		return -1; //Currently open disk does not match SB block count

	/* Allocate memory for the FAT table and read it from disk */
	FAT = (uint16_t*)malloc(sb.total_data_blocks * sizeof(uint16_t)); //"There are as many entries as data blocks in the disk"

	if(FAT == NULL)
		return -1; //Memory allocation for FAT failed

	for(size_t i = 1; i < sb.total_FAT_blocks + 1; i++)
	{
		if(block_read(i, &FAT + (i-1)*BLOCK_SIZE) == -1)
		{
			free(FAT);
			return -1;
		}
	}

	if(FAT[0] != FAT_EOC)
	{
		//First entry is not 0xFFFF
		return -1;
	}

	/* Read the root direcory from disk */
	if (block_read(sb.root_dir_index, root_dir) == -1)
	{
		free(FAT);
		return -1;
	}

	fs_mounted == 1;

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	/* checking if the file system is mounted*/
	if(fs_mounted == 0)
		return -1;

	/* Rewriting Superblock back to disk */
	if (block_write(0, &sb)==-1)
	{
		//failed to rewrite superblock back to disk
		return -1;
	}
	/* Write FAT table and root directory back to disk */
	for(size_t i = 1; i < sb.total_FAT_blocks + 1; i++)
	{
		if(block_write(i, &FAT + (i-1)*BLOCK_SIZE ) == -1)
			return -1;
	}
	if(block_write(sb.root_dir_index, root_dir) == -1)
		return -1;

	fs_mounted = 0;
	
	return block_disk_close();
}

int fs_info(void)
{
	printf("FS Info:\n"
			"total_blk_count=%d\n"
			"fat_blk_count=%d\n"
			"rdir_blk=%d\n"
			"data_blk=%d\n"
			"data_blk_count%d\n", 
			sb.total_disk_blocks, sb.total_FAT_blocks, sb.root_dir_index, sb.data_block_start_index, sb.total_data_blocks
	);
	int fat_free = 0;
	for(int i = 0; i < sb.total_data_blocks; i++) //subtract 1 if FAT_EOC doesn't count
	{
		if(FAT[i] == 0)
			fat_free++;
	}
	printf("fat_free_ratio=%zu\%\n", (uint16_t)fat_free/sb.total_data_blocks);
	int root_free = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(root_dir[i].filename[0] == '\0')
			root_free++;
	}
	printf("rdir_free_ratio=%d\%\n", root_free/FS_FILE_MAX_COUNT);
}

int fs_create(const char *filename)
{
	/* check if FS is mounted */
	if(fs_mounted == 0)
		return -1;

	if(filename == NULL)
	{
		//no filename provided
		return -1;
	}
	if(strlen(filename) > FS_FILENAME_LEN)
	{
		//filename too long
		return -1;
	}
	
	int freeEntry = -1; //keep track of the first freeEntry in the directory
	int pollingFlag = 1; //flag to determine whether or not we're still looking for a freeEntry
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) //Purpose: check if filename already exists but also find first free entry
	{
		if(root_dir[i].filename[0] == '\0' && pollingFlag) // if we found a freeEntry
		{
			freeEntry = i; //take the index
			pollingFlag = 0; //no longer polling for freeEntry
		}
		if(strcmp((char*)root_dir[i].filename, filename) == 0)
		{
			//file already exists within directory
			return -1;
		}
	}
	if (freeEntry < 0)
	{
		//directory is full
		return -1;
	}
	else
	{
		memcpy(root_dir[freeEntry].filename, filename, FS_FILENAME_LEN);
		root_dir[freeEntry].size = 0;
		root_dir[freeEntry].first_datablock_index = FAT_EOC;
	}

	return 0;

}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */

	//if file @filename is currently open

	/*
	array[FS_FILE_MAX_COUNT] = malloc(0)
	fs_open -> file descriptor number
	open -> array[i] = 1

	*/

	if(fs_mounted == 0)
		return -1;

	if(filename == NULL || strlen(filename) > FS_FILENAME_LEN)
	{
		//provided invalid file name
		return -1;
	}

	int i = 0;
	while(strcmp(root_dir[i].filename, filename) != 0 && i <= FS_FILE_MAX_COUNT)
		i++;
	if(i > FS_FILE_MAX_COUNT-1)
	{
		//File not found
		return -1;
	}
	uint16_t index = root_dir[i].first_datablock_index;
	root_dir[i].filename[0] = '\0';
	root_dir[i].size = 0;
	root_dir[i].first_datablock_index = FAT_EOC;

	block_write(sb.root_dir_index, root_dir);
	
	//clear FAT chain
	while(index != FAT_EOC)
	{
		uint16_t next = FAT[index];
		FAT[index] = 0;
		index = next;
	}
	return 0;
}

int fs_ls(void)
{
	if(fs_mounted == 0)
	{
		//not mounted
		return -1;
	}

	printf("FS ls:");
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(root_dir[i].filename[0] != '\0')
			printf("file: %s, size: %d, data_blk: %d\n", root_dir[i].filename, root_dir[i].size, root_dir[i].first_datablock_index);
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

