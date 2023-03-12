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

/*
struct superblock{
	uint8_t signature[8];
	uint16_t version;
	uint16_t block_size;
	uint32_t fs_size;
	uint32_t fat_start;
	uint32_t fat_blocks;
	uint32_t root_start;
	uint32_t root_blocks;
};
*/

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
	char filename[16];	// Filename ( including NULL character )
	uint32_t size; // Size of the file (in bytes)
	uint16_t first_datablock_index; // Index of the first data block
	char padding[10];
}__attribute__((packed));

struct root_dir_entry root_dir[128];


/* Global variables to used in the file system */
static int fs_mounted = 0;	//flag to check if a file system is mounted
static struct superblock sb; // superblock of the moutner file system
static struct fat_entry *fat_table = NULL; // FAT tabe of the mounter file system
static struct root_entry root_table[FS_FILE_MAX_COUNT]; //root directory of the mounter file system

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

	for(uint8_t i = 1; i < sb.total_FAT_blocks; i++)
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
		free(fat_table);
		return -1;
	}

	fs_mounted = 1;

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	/* checking if the file system is mounted*/
	if(fs_mounted == 0)
		return -1;

	/* checking if all the files are closed */
	for(int i = 0 ; i < FS_OPEN_MAX_COUNT; i++)
	{
		if(file_table[i].used == 1)
			return -1;	
	}
	
	/* Write FAT table and root directory back to disk */
	for(uint32_t i = 0; i < sb.fat_blocks; i++)
	{
		if(block_write(sb.fat_start + i, fat_table + i * BLOCK_SIZE) == -1)
			return -1;
	}
	for(uint32_t i = 0; i < sb.root_blocks; i++)
	{
		if(block_write(sb.root_start + i, root_table + i * BLOCK_SIZE) == -1)
			return -1;
	}

	fs_mounted = 0;
	if(block_disk_closed() == -1)
		return -1;

	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
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

