#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

struct superblock
{
	char signature[8];				 // Signature ( must be equal to "ECS150FS")
	uint16_t total_disk_blocks;		 // Total amount of blocks of virtual disk
	uint16_t root_dir_index;		 // Root directory block index
	uint16_t data_block_start_index; // Data block start index
	uint16_t data_blocks_count;		 // Amount of data blocks
	uint8_t total_FAT_blocks;		 // Number of blocks for FAT
	uint8_t padding[4079];			 // Unused/Padding
} __attribute__((packed));

#define FAT_EOC 0xFFFF			   // End-of-Chain value
#define FAT_ENTRIES_PER_BLOCK 2048 // Number of FAT entries per block

uint16_t *FAT; // pointer to FAT array

/* Initializing FAT array */
void init_FAT(uint16_t data_blocks_count)
{
	/* There are as many entries as data blocks in the disk */
	int num_FAT_blocks = (data_blocks_count + FAT_ENTRIES_PER_BLOCK - 1) / FAT_ENTRIES_PER_BLOCK;
	FAT = malloc(num_FAT_blocks * FAT_ENTRIES_PER_BLOCK * sizeof(uint16_t)); // allocating memory for FAT
	FAT[0] = FAT_EOC;														// Setting first entry to END-of-Chain value
}

/* Getting the next block in the chain of FAT array */
uint16_t get_next_block(uint16_t block_index)
{
	uint16_t entry = FAT[block_index];
	if (entry >= FAT_EOC)
		return FAT_EOC; // End-of-cain
	return entry;
}

/* FAT: set next block in the chain */
void set_next_block(uint16_t block_index, uint16_t next_block_index)
{
	FAT[block_index] = next_block_index;
}

/* FAT: get the block index of the first block of a file */
uint16_t get_first_block_index(uint16_t file_block_index)
{
	return file_block_index + 1; // First block index is one past the file block index
}

/* FAT: get the block index of the last block of a file */
uint16_t get_last_block_index(uint16_t first_block_index)
{
	uint16_t block_index = first_block_index;
	while (get_next_block(block_index) != FAT_EOC)
	{
		block_index = get_next_block(block_index);
	}
	return block_index;
}

/* FAT: allocating a new block for a file */
uint16_t allocate_block(uint16_t prev_block_index, uint16_t num_blocks_allocated)
{
	uint16_t block_index = prev_block_index;
	for (int i = 0; i < num_blocks_allocated; i++)
	{
		uint16_t next_block_index = get_next_block(block_index);
		if (next_block_index == FAT_EOC)
		{
			uint16_t new_block_index = block_index + 1;
			set_next_block(block_index, new_block_index);
			set_next_block(new_block_index, FAT_EOC);
			block_index = new_block_index;
		}
		else
		{
			block_index = next_block_index;
		}
	}
	return block_index;
}

/* Root Directory data structure */
struct root_dir_entry
{
	char filename[FS_FILENAME_LEN]; // Filename ( including NULL character )
	uint32_t size;					// Size of the file (in bytes)
	uint16_t first_datablock_index; // Index of the first data block
	char padding[10];
} __attribute__((packed));

struct file
{
	char filename[FS_FILENAME_LEN];
	size_t offset;
};

struct fd_table
{
	struct file files[FS_OPEN_MAX_COUNT];
	int total_opened;
};

struct root_directory
{
	struct root_dir_entry root_dir_entries[FS_FILE_MAX_COUNT];
	int total_opened;
};

/* Global variables to used in the file system */
static int fs_mounted = 0;
static struct superblock sb; // superblock of the mounter file system
static struct fd_table fd_table;
struct root_directory root_dir;

/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
	printf("...fs_mount() initalize\n");
	/* TODO: Phase 1 */
	/* Opening virtual disk file */
	if (block_disk_open(diskname) == -1)
		return -1;

	/* Read the superblock */
	if (block_read(0, &sb) == -1)
		return -1;

	/* Verify the signature of the file system */
	if (strncmp((char *)sb.signature, "ECS150FS", 8) != 0)
		return -1; // Incorrect signature

	/* verify the size of the virtual disk and the block size */
	if (block_disk_count() != sb.total_disk_blocks)
		return -1; // Currently open disk does not match SB block count

	/* Allocate memory for the FAT table and read it from disk */
	init_FAT(sb.data_blocks_count);
	// FAT = (uint16_t *)malloc(sb.data_blocks_count * sizeof(uint16_t)); //"There are as many entries as data blocks in the disk"

	if (FAT == NULL)
		return -1; // Memory allocation for FAT failed

	for (int i = 0; i < sb.total_FAT_blocks; i++)
	{
		if (block_read( i+1, FAT + i * BLOCK_SIZE ) == -1)
		{
			free(FAT);
			return -1;
		}
	}

	if (FAT[0] != FAT_EOC)
	{
		// First entry is not 0xFFFF
		return -1;
	}

	/* Read the root direcory from disk */
	if (block_read(sb.root_dir_index, root_dir.root_dir_entries) == -1)
	{
		free(FAT);
		return -1;
	}

	// printf("fs_mount() exiting...\n");
	fs_mounted = 1;
	return 0;
}

int fs_umount(void)
{
	// printf("...fs_unmount() intialize\n");
	/* checking if the file system is mounted*/
	if (!fs_mounted)
		return -1;

	if(fd_table.total_opened > 0)
	{
		//still open fd
		return -1;
	}

	/* Rewriting Superblock back to disk */
	if (block_write(0, &sb) == -1)
	{
		// failed to rewrite superblock back to disk
		return -1;
	}

	/* Write FAT table and root directory back to disk */
	for (int i = 0; i < sb.total_FAT_blocks; i++)
	{
		if (block_write(i + 1, FAT + i * BLOCK_SIZE) == -1)
			return -1;
	}
	if (block_write(sb.root_dir_index, root_dir.root_dir_entries) == -1)
		return -1;

	fs_mounted = 0;
	// free(FAT);
	return block_disk_close();
}

int fs_info(void)
{
	printf("starting fs_info\n");
	if(!fs_mounted)
	{
		//no disk mounted
		return -1;
	}

	printf("FS Info:\n"
		   "total_blk_count=%d\n"
		   "fat_blk_count=%d\n"
		   "rdir_blk=%d\n"
		   "data_blk=%d\n"
		   "data_blk_count=%d\n",
		   sb.total_disk_blocks, sb.total_FAT_blocks, sb.root_dir_index, sb.data_block_start_index, sb.data_blocks_count);
	int fat_free = 0;
	for (int i = 0; i < sb.data_blocks_count; i++) // subtract 1 if FAT_EOC doesn't count
	{
		if (FAT[i] == 0)
			fat_free++;
	}
	printf("fat_free_ratio=%d/%d\n", fat_free, sb.data_blocks_count);
	int root_free = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (root_dir.root_dir_entries[i].filename[0] == '\0')
			root_free++;
	}
	printf("rdir_free_ratio=%d/%d\n", root_free, FS_FILE_MAX_COUNT);
	return 0;
}

int fs_create(const char *filename)
{
	/* check if FS is mounted */
	if (!fs_mounted)
		return -1;

	if (filename == NULL)
	{
		// no filename provided
		return -1;
	}
	if (strlen(filename) > FS_FILENAME_LEN)
	{
		// filename too long
		return -1;
	}
	if(root_dir.total_opened == FS_FILE_MAX_COUNT)
	{
		//max files created
		return -1;
	}

	int freeEntry = -1;							// keep track of the first freeEntry in the directory
	int pollingFlag = 1;						// flag to determine whether or not we're still looking for a freeEntry
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) // Purpose: check if filename already exists but also find first free entry
	{
		if (root_dir.root_dir_entries[i].filename[0] == '\0' && pollingFlag) // if we found a freeEntry
		{
			freeEntry = i;	 // take the index
			pollingFlag = 0; // no longer polling for freeEntry
		}
		if (strcmp((char *)root_dir.root_dir_entries[i].filename, filename) == 0)
		{
			// file already exists within directory
			return -1;
		}
	}
	if (freeEntry < 0)
	{
		// directory is full
		return -1;
	}
	else
	{
		memcpy(root_dir.root_dir_entries[freeEntry].filename, filename, FS_FILENAME_LEN);
		root_dir.root_dir_entries[freeEntry].size = 0;
		root_dir.root_dir_entries[freeEntry].first_datablock_index = FAT_EOC;
	}
	root_dir.total_opened++;
	return 0;
}

int fs_delete(const char *filename)
{
	// TODO: if file @filename is currently open
	// Loop through fd_table for filename
	// If loop passes through, file is not open

	if (!fs_mounted)
		return -1;

	if (filename == NULL || strlen(filename) > FS_FILENAME_LEN)
	{
		// provided invalid file name
		return -1;
	}

	int i = 0;
	while (strcmp(root_dir.root_dir_entries[i].filename, filename) != 0 && i <= FS_FILE_MAX_COUNT)
		i++;
	if (i > FS_FILE_MAX_COUNT - 1)
	{
		// File not found
		return -1;
	}
	uint16_t index = root_dir.root_dir_entries[i].first_datablock_index;
	root_dir.root_dir_entries[i].filename[0] = '\0';
	root_dir.root_dir_entries[i].size = 0;
	root_dir.root_dir_entries[i].first_datablock_index = FAT_EOC;

	block_write(sb.root_dir_index, root_dir.root_dir_entries);

	// clear FAT chain
	while (index != FAT_EOC)
	{
		uint16_t next = FAT[index];
		FAT[index] = 0;
		index = next;
	}
	root_dir.total_opened--;
	return 0;
}

int fs_ls(void)
{
	if (!fs_mounted)
	{
		// not mounted
		return -1;
	}

	printf("FS ls:\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (root_dir.root_dir_entries[i].filename[0] != '\0')
			printf("file: %s, size: %d, data_blk: %d\n", root_dir.root_dir_entries[i].filename, root_dir.root_dir_entries[i].size, root_dir.root_dir_entries[i].first_datablock_index);
	}
	return 0;
}

int fs_open(const char *filename) // do we not have to check if the file is already open?
{
	if (!fs_mounted)
		return -1;

	if (filename == NULL || strlen(filename) == 0 || strlen(filename) > FS_FILENAME_LEN)
	{
		//invalid filename
		return -1; 
	}

	/* check if there is already a file with the same name opened */
	for(int i = 0; i < fd_table.total_opened; i++)
	{
		if(strcmp(fd_table.files[i].filename, filename) == 0)
			return -1;
	}

	/* check if there is room to open another file */
	if (fd_table.total_opened == FS_OPEN_MAX_COUNT)
	{
		// max files opened
		return -1;
	}

	/*Find whether the file exists*/
	int i = 0;
	while (strcmp(root_dir.root_dir_entries[i].filename, filename) != 0 && i < FS_FILE_MAX_COUNT)
		i++;

	if (i == root_dir.total_opened - 1)
	{
		// file not found
		return -1;
	}

	/* find the file in the root directory */
	// int file_index = -1;
	// for (int i = 0; i< root_dir.total_opened; i++)
	// {
	// 	if(strcmp(root_dir.root_dir_entries[i].filename, filename) == 0)
	// 	{
	// 		file_index = i;
	// 		break;
	// 	}
	// }
	// if(file_index == -1)
	// 	return -1; //file not found

	/*Find the next available space and open add the entry into the fd_table at offset 0*/
	/*Note, we shouldn't have to worry about there being no space since we would've returned earlier*/
	int j = 0;
	while (fd_table.files[j].filename[0] != '\0' && j < FS_OPEN_MAX_COUNT)
		j++;
	memcpy(fd_table.files[j].filename, filename, FS_FILENAME_LEN);
	fd_table.files[j].offset = 0;
	fd_table.total_opened++;
	return j;

	/*create a new file desciprtor for the opened file */
	// struct file new_file = {0};
	// strncpy(new_file.filename, filename, FS_FILENAME_LEN);
	// new_file.offset = 0;

	// /* Add the new file descriptor to the file descriptor table */
	// fd_table.files[fd_table.total_opened] = new_file;
	// fd_table.total_opened++;
	// return fd_table.total_opened -1;
}

int fs_close(int fd) //gonna assume its 0 based
{
	if (!fs_mounted)
		return -1;

	if(fd > FS_OPEN_MAX_COUNT-1 || fd < 0)
	{
		//invalid fd
		return -1;
	}
	if(fd_table.files[fd].filename[0] == '\0')
	{
		//file not open
		return -1;
	}
	fd_table.files[fd].filename[0] = '\0';
	fd_table.files[fd].offset = 0;
	fd_table.total_opened--;
	return 0;
}

int fs_stat(int fd)
{
	if (!fs_mounted)
		return -1;

	if(fd > FS_OPEN_MAX_COUNT-1 || fd < 0)
	{
		//invalid fd
		return -1;
	}
	if(fd_table.files[fd].filename[0] == '\0')
	{
		//file not open
		return -1;
	}

	char * filename = (char*)fd_table.files[fd].filename;
	int current_file_size = -1;
	int idx = -1;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(strcmp((char*)root_dir.root_dir_entries[i].filename, filename) == 0)
		{
			current_file_size = root_dir.root_dir_entries[i].size;
			idx = i;
			break;
		}
	}

	if(current_file_size == -1)
		return -1;

	// int i = 0;
	// //the file should exist at this point so we don't need to check for missing file
	// while(strcmp(root_dir.root_dir_entries[i].filename, fd_table.files[fd].filename) == 0)
	// {
	// 	printf("root_dir[%d]: %s\n", i, root_dir.root_dir_entries[i].filename);
	// 	i++;
	// 	if(i > FS_FILE_MAX_COUNT)
	// 		return -1;
	// }
	
	printf("root_dir.root_dir_entries.size[%d]: %d\n", idx, current_file_size);
	return root_dir.root_dir_entries[idx].size;
}

int fs_lseek(int fd, size_t offset)
{
	/* size_t is supposed to be unsigned, therefore, will never be negative*/
	// if(offset < 0)
	// {
	// 	/*
	// 	Not asked by Porquet but doesn't make sense to have negative offset
	// 	*/
	// 	//invalid offset
	// 	return -1;
	// }
	if(!fs_mounted)
		return -1;
	
	if( fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table.files[fd].filename[0] == '\0' )
		return -1;

	//add other error checks if program doesn't terminate upon error
	if(offset > (size_t)fs_stat(fd)) //calling fs_stat will run the fd through the error checking in fs_stat
	{
		//offset larger than size
		return -1;
	}

	fd_table.files[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	printf("...fs_read() intialization\n");
	/* TODO: Phase 4 */
	if(!fs_mounted)
		return -1;
	
	if(fd < 0 || fd >= fd_table.total_opened)
		return -1;

	if(buf == NULL)
		return -1;

	if(fd_table.files[fd].filename[0] == '\0')
		return -1;

	char *filename = fd_table.files[fd].filename;
	size_t offset = fd_table.files[fd].offset;
	int file_size = fs_stat(fd);

	int remaining_size = file_size - offset;
	int bytes_to_read = (int)count < remaining_size ? (int)count : remaining_size;

	uint16_t data_block_index = root_dir.root_dir_entries[fd].first_datablock_index;
	int bytes_read = 0;

	while(bytes_to_read > 0 && data_block_index != FAT_EOC)
	{
		void *bounce_buffer = malloc(BLOCK_SIZE);
		block_read(data_block_index, bounce_buffer);

		int block_offset = offset % BLOCK_SIZE;
		int block_bytes_to_read = BLOCK_SIZE - block_offset;
		int copy_size = bytes_to_read < block_bytes_to_read ? bytes_to_read : block_bytes_to_read;
        memcpy(buf + bytes_read, bounce_buffer + block_offset, copy_size);

		offset += copy_size;
		bytes_read += copy_size;
		bytes_to_read -= copy_size;

		data_block_index = FAT[data_block_index];

		free(bounce_buffer);
	}


	fd_table.files[fd].offset = offset;

	printf("fs_read() finished %d\n", bytes_read);
	return bytes_read;
}


/*
	struct file *file = &fd_table.files[fd];
	uint16_t block_index = get_first_block_index(file->offset / BLOCK_SIZE);
	size_t bytes_read = 0;

	while (bytes_read < count && block_index != FAT_EOC)
	{
		block_read(block_index + sb.data_block_start_index, buf + bytes_read);

		bytes_read += BLOCK_SIZE;
		file->offset += BLOCK_SIZE;

		block_index = get_next_block(block_index);
	}
*/