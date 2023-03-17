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
	// printf("...fs_mount() initalize\n");
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
	// init_FAT(sb.data_blocks_count);
	FAT = (uint16_t*)malloc(sb.data_blocks_count * BLOCK_SIZE * sizeof(uint16_t)); // allocating memory for FAT
	// FAT = (uint16_t *)malloc(sb.data_blocks_count * sizeof(uint16_t)); //"There are as many entries as data blocks in the disk"

	if (FAT == NULL)
		return -1; // Memory allocation for FAT failed

	for (int i = 0 ; i < sb.root_dir_index; i++)
	{
		if (block_read( i +1 , FAT + (i) * BLOCK_SIZE ) == -1)
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
			break;
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
	int j = 0;

	/*Find whether the file exists*/
	// printf("root_dir.total_opened: %d\n", fd_table.total_opened);
	if(fd_table.total_opened > 0)
	{
		int i = 0;
		while (strcmp(root_dir.root_dir_entries[i].filename, filename) != 0 && i < FS_FILE_MAX_COUNT) //Check each root_dir_entry for filename up til 128
		{
			i++;
			// printf("searching through file: %d\n", i);
		}

		if (i == fd_table.total_opened - 1)
		{
			// file not found
			// printf("file not found\n");
			return -1;
		}
	}
	else
	{
		/*Find the next available space and open add the entry into the fd_table at offset 0*/
		/*Note, we shouldn't have to worry about there being no space since we would've returned earlier*/

		while (fd_table.files[j].filename[0] != '\0' && j < FS_OPEN_MAX_COUNT)
			j++;
		memcpy(fd_table.files[j].filename, filename, FS_FILENAME_LEN);
		fd_table.files[j].offset = 0;
		fd_table.total_opened++;
	}
	return j;
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
	// printf("FS STAT: filename = %s\n", filename);
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
	
	// printf("root_dir.root_dir_entries.size[%d]: %d\n", idx, current_file_size);
	// printf("FS STAT: size = %d\n", root_dir.root_dir_entries[idx].size);
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
	// printf("offset changed to: %zu\n", offset);
	fd_table.files[fd].offset = offset;
	return 0;
}

uint16_t get_first_data_block_index(int fd)
{
	char* filename = fd_table.files[fd].filename;
	// printf("get_first data filename: %s\n", filename);
	for( int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		// printf("root_dir filename: %s\n", root_dir.root_dir_entries[i].filename);
		if(strcmp(root_dir.root_dir_entries[i].filename, filename) == 0)
		{
			// printf("get_first_data_block_index filename: %s\n", fd_table.files[fd].filename);
			// printf("fd: %d, file: %s, size: %d, data_blk: %d\n", fd, root_dir.root_dir_entries[i].filename, root_dir.root_dir_entries[i].size, root_dir.root_dir_entries[i].first_datablock_index);
			return root_dir.root_dir_entries[i].first_datablock_index + sb.data_block_start_index;
		}
	}
}

uint16_t get_last_data_block_index(int fd)
{
	char* filename = fd_table.files[fd].filename;
	for(int i = 0; i< FS_FILE_MAX_COUNT; i++)
	{
		//printf("........comparing: %s | %s\n", root_dir.root_dir_entries[i].filename, filename);
		if(strcmp(root_dir.root_dir_entries[i].filename, filename) == 0)
		{
			uint16_t index =  root_dir.root_dir_entries[i].first_datablock_index;
			uint16_t next = FAT[index];
			while(next != FAT_EOC)
			{
				//printf("index: %d\n", index);
				index = next;
				next = FAT[index];
			}
			return index + sb.data_block_start_index;
		}
	}
}

uint16_t allocate_newblock(int fd)
{
	uint16_t FAT_idx;
	/* Iterate through FAT entries to search for free space */
	/* Skip FAT[0] because it is always FAT_EOC */
	for (FAT_idx = 1; FAT_idx < sb.data_blocks_count; FAT_idx++)
	{
		if(FAT[FAT_idx] == 0)
		{
			FAT[FAT_idx] = FAT_EOC;
			for(int i = 0; i < root_dir.total_opened; i++)
			{
				if(strcmp(root_dir.root_dir_entries[i].filename, fd_table.files[fd].filename) == 0)
				{
					root_dir.root_dir_entries[i].first_datablock_index = FAT_idx;
					break;
				}
			}
			return FAT_idx + sb.data_block_start_index;
		}
	}
	return FAT_EOC;	// meaning there is no available space
}

void update_file_size(int fd, size_t bytes_written)
{
	char* filename = fd_table.files[fd].filename;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) //total_opened = how many files open != what file is what 
	{
		// printf("%d: comparing: %s == %s\n", i, root_dir.root_dir_entries[i].filename, filename);
		if(strcmp(root_dir.root_dir_entries[i].filename, filename) == 0)
		{
			root_dir.root_dir_entries[i].size += bytes_written;
			// printf("UPDATE FILE SIZE: size = %d\n", root_dir.root_dir_entries[i].size);
			break;
		}
	}
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	if(!fs_mounted)
		return -1;
	
	if(fd < 0 || fd >= fd_table.total_opened)
		return -1;

	if(buf == NULL)
		return -1;

	if(fd_table.files[fd].filename[0] == '\0')
		return -1;

	struct file f = fd_table.files[fd];

	// uint16_t block_index = get_first_data_block_index(fd);
	uint16_t block_index; 
	if(fs_stat(fd) == 0)
	{
		// printf("getting new data block\n");
		block_index = allocate_newblock(fd); //potential problem
	}
	else
	{
		// printf("getting the last datablock\n");
		block_index = get_last_data_block_index(fd); //potential problem
	}
	//at this point, block_index = the block we're writing to
	size_t offset_in_block = f.offset % BLOCK_SIZE; //get the last offset in the block -- 0 if newly allocated otherwise 0 or >

	size_t bytes_written = 0; //we will return this 
	while(bytes_written < count)
	{
		void* bounce_buffer = malloc(BLOCK_SIZE);

		/* Read block at @block_nr into @buf */
		if(block_read(block_index, bounce_buffer) == -1)		// Reading any content at block_index to bounce_buffer
			return -1;
		
		// printf("block_index: %d\n", block_index);
		// printf("From fs_write: block read to bounce_buffer from %d:\n", block_index);
		// for (int i = 0; i < 20; i++) {
		// 	printf("%02x ", ((unsigned char*)bounce_buffer)[i]);
		// 	if ((i + 1) % 16 == 0) printf("\n"); // print a newline every 16 bytes
		// }
		// printf("\n");

		size_t available_space = BLOCK_SIZE - offset_in_block; 
		if(bytes_written + available_space > count) //if we have a need for this then we did something wrong LMAO
			available_space = count - bytes_written;
		
		memcpy(bounce_buffer + offset_in_block, buf + bytes_written, available_space);
		// printf("offset_in_block: %zu\n", offset_in_block);
		// printf("bytes_written: %zu\n", bytes_written);
		// printf("available_space: %zu\n", available_space);
		// printf("From fs_write: bounce_buffer content:\n");
		// for (int i = 0; i < 20; i++) {
		// 	printf("%02x ", ((unsigned char*)bounce_buffer)[i]);
		// 	if ((i + 1) % 16 == 0) printf("\n"); // print a newline every 16 bytes
		// }
		// printf("\n");

		/* Write block at @block_nr with @buf's contents */
		if(block_write(block_index, bounce_buffer) == -1)
			return -1;
		/* bytes_written: total bytes written,  available_space: bytes_written in this iteration */
		size_t remaining_bytes = count - bytes_written - available_space; 
		// printf("remaining_bytes: %zu\n", remaining_bytes);
		if(remaining_bytes > 0)			//If we have to write more bytes
		{
			uint16_t next_block_index = allocate_newblock(fd);
			FAT[block_index] = next_block_index;
			block_index = next_block_index;
			offset_in_block = 0;
		}
		else
		{
			// printf("REMAINING BYTES = 0\n");
			bytes_written += available_space;
			update_file_size(fd, bytes_written);
			// printf("file size: %d\n", fs_stat(fd));
			break;
		}
		bytes_written += available_space; //available_size: bytes written in this iteration
		// printf("last bytes_written: %zu\n", bytes_written);
	}
	// printf("returned bytes_written: %zu\n", bytes_written);
	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
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
	// printf("OFFSET: %ld\n", offset);


	int file_size = fs_stat(fd);

	int remaining_size = file_size - offset;
	int bytes_to_read = 0;
	if((int) count < remaining_size)
		bytes_to_read = count;
	else
		bytes_to_read = remaining_size;
	uint16_t data_block_index = get_first_data_block_index(fd);
	int bytes_read = 0; // tracking the amount of bytes read into @buf
	printf("%s: @ %ld\n", filename, offset);
	printf("count: %ld\n", count);
	printf("filesize: %d\n", file_size);
	printf("remaining_size: %d\n", remaining_size);
	printf("bytes_to_read: %d\n", bytes_to_read);
	printf("data_block_index: %d\n", data_block_index);

	while(bytes_to_read > 0 && data_block_index != FAT_EOC)
	{
		void *bounce_buffer = malloc(BLOCK_SIZE);

		if(block_read(data_block_index, bounce_buffer) == -1)
			return -1;
		// printf("block_read to bounce_buffer: \n");
		// for (int i = 0; i < 20; i++) {
		// 	printf("%02x ", ((unsigned char*)bounce_buffer)[i]);
		// 	if ((i + 1) % 16 == 0) printf("\n"); // print a newline every 16 bytes
		// }
		// printf("\n");
		int block_offset = offset % BLOCK_SIZE;	//offset goes from [0, size_of_file in bytes] % BLOCK_SIZE -> choosing which data block 
		int block_bytes_to_read = BLOCK_SIZE - block_offset; // checking if there is more than or less than (bytes_to_read)
		int copy_size = 0;
		if(bytes_to_read < block_bytes_to_read)
			copy_size = bytes_to_read;
		else
			copy_size = block_bytes_to_read;

        memcpy((char*)buf + bytes_read, (char*)bounce_buffer + block_offset, copy_size);
		printf("copy_size: %d\n", copy_size);
		offset += copy_size;
		bytes_read += copy_size;
		bytes_to_read -= copy_size;

		data_block_index = FAT[data_block_index];	// go to the next data block for current file

		free(bounce_buffer);
	}

	fd_table.files[fd].offset = offset;

	// printf("fs_read() finished %d\n", bytes_read);
	return bytes_read;
}
