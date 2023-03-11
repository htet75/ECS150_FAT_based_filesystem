#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

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

struct fat_entry{
	uint32_t value;
}__attribute__((packed));

struct root_entry{
	uint8_t filename[FS_FILENAME_LEN];
	uint32_t start_block;
	uint32_t num_blocks;
}

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
	if (block_read(0, &sb) = -1)
		return -1;

	/* Verify the signature and version of the file system */
	if(strncmp((char*)sb.signature, "ECS150FS", 8) != 0 || sb.version != 0x0001)
		return -1;

	/* verify the size of the virtual disk and the block size */
	if(block_disk_count() != sb.fs_size || sb.block_size != BLOCK_SIZE)
		return -1;

	/* Allocate memory for the FAT table and read it from disk */
	fat_table = malloc(sb.fat_blocks * BLOCK_SIZE)
	if(fat_table == NULL)
		return -1;
	for(uint32_t i = 0; i < sb.fat_blocks; i++)
	{
		if(block_read(sb.fat_start + i, fat_table + i * BLOCK_SIZE) == -1)
		{
			free(fat_table);
			return -1;
		}
	}

	/* Read the root direcory from disk */
	for(uint32_t i=0; i<sb.root_blocks; i++)
	{
		if(block_read(sb.root_start + i, root_table + i * BLOCK_SIZE) == -1)
		{
			free(fat_table);
			return -1;
		}
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

