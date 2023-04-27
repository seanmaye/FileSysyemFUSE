/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"


char diskfile_path[PATH_MAX];
struct inode my_inode;
const size_t inode_size = sizeof(my_inode);
const int inum_blocks = ((inode_size*MAX_INUM)/BLOCK_SIZE);
const int total_mem = (BLOCK_SIZE*(3+inum_blocks+MAX_DNUM));
char* phys_disk;

// Declare your in-memory data structures here
struct inode my_inode;
bitmap_t inode_bitmap; 
bitmap_t disk_bitmap; 
struct superblock superblock;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	memcpy(inode_bitmap,&superblock+BLOCK_SIZE*3,sizeof(inode_bitmap));
	int count;
	
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i < MAX_INUM; i++){
		if(inode_bitmap==0){
			count = i;
			break;
		}
	}

	// Step 3: Update inode bitmap and write to disk 
	inode_bitmap[count]=1;
	// put something here ask question to TA/Prof about writing to disk

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	memcpy(inode_bitmap,&superblock+BLOCK_SIZE*4,sizeof(inode_bitmap));

	// Step 2: Traverse data block bitmap to find an available slot
	int count;
	for(int i = 0; i < MAX_DNUM; i++){
		if(disk_bitmap==0){
			count = i;
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	disk_bitmap[count]= 1;
	//ask question to TA/Prof about writing to disk
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
  	int block_num = ino/(BLOCK_SIZE/sizeof(inode));

  // Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(inode));

  // Step 3: Read the block from disk and then copy into inode structure
	memcpy(inode, &superblock+BLOCK_SIZE*(3+block_num)+offset*sizeof(inode),sizeof(inode));

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
  	int block_num = ino/(BLOCK_SIZE/sizeof(inode));

  	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(inode));

	// Step 3: Write inode to disk 


	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
    // Step 1: Resolve the path name, walk through path, and finally, find its inode.
    // Note: You could either implement it in a iterative way or recursive way.

	//dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent)
	struct dirent direntFind;
	dir_find(ino,path, sizeof(inode),&direntFind);
	return 0;
}
/* 
 * Make file system
 */
int rufs_mkfs() {
	char* phys_disk = (char *)malloc(total_mem);
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	superblock = {MAGIC_NUM,MAX_INUM,MAX_DNUM,
	&superblock + sizeof(superblock),
	&superblock + sizeof(superblock) + sizeof(inode_bitmap),
	&superblock + sizeof(superblock) + sizeof(inode_bitmap) + sizeof(disk_bitmap), 
	&superblock + sizeof(superblock) + sizeof(inode_bitmap) + sizeof(disk_bitmap) + sizeof(my_inode)*MAX_INUM};

	// initialize inode bitmap
	inode_bitmap[MAX_INUM]; 
	memset(inode_bitmap, 0, sizeof(inode_bitmap));
	
	// initialize data block bitmap
	disk_bitmap[MAX_DNUM];
	memset(disk_bitmap, 0, sizeof(inode_bitmap));
	 
	// update bitmap information for root directory
	inode_bitmap[0] = 1; 	//first node is set as the root
	disk_bitmap[0] = 1;		//first node is set as the root

	// update inode for root directory
	struct inode root_inode = {
		.ino = 0, // inode number of root directory
		.valid = 1, // root directory is valid
		.size = 0, // root directory has no size (no data block)
		.type = 0, // root directory type is directory
		.link = 1, // root directory has one hard link (itself)
		.direct_ptr = {0}, // root directory doesn't have any direct data block
		.indirect_ptr = {0}, // root directory doesn't have any indirect data block
		.vstat = {0}, // inode stat struct, initialized to zero
	};
	memcpy(&superblock+3*BLOCK_SIZE,&root_inode,sizeof(root_inode));
	
	//ask question to TA/PROF on where to put
	struct dirent first_dirent = {
		.ino = 0,
		.valid = 1,
		.name = ".",
		.len = 1,
	};

	struct dirent second_dirent = {
		.ino = 0,
		.valid = 1,
		.name = "..",
		.len = 2,
	};
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

if(access(diskfile_path, F_OK)==0){
	// bit map is in memory so loop through and find all inodes 
	// initialize memory strutcutres
	struct superblock superblock;
	memcpy(&superblock, &diskfile_path, sizeof(superblock));
	memcpy(&inode_bitmap, &diskfile_path[superblock.i_bitmap_blk], sizeof(inode_bitmap));
	memcpy(&disk_bitmap, &diskfile_path[superblock.d_bitmap_blk], sizeof(disk_bitmap));
}else{
	printf("No disk file found");
	rufs_mkfs();
	
}
	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	memset(inode_bitmap, 0, sizeof(inode_bitmap));
	memset(disk_bitmap, 0, sizeof(disk_bitmap));
	struct inode root_inode = {
		.ino = 0, // inode number of root directory
		.valid = 1, // root directory is valid
		.size = 0, // root directory has no size (no data block)
		.type = 0, // root directory type is directory
		.link = 1, // root directory has one hard link (itself)
		.direct_ptr = {0}, // root directory doesn't have any direct data block
		.indirect_ptr = {0}, // root directory doesn't have any indirect data block
		.vstat = {0}, // inode stat struct, initialized to zero
	};
	memcpy(&diskfile_path+3*BLOCK_SIZE,&root_inode,sizeof(root_inode));

	// Step 2: Close diskfile
	dev_close();	

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode toGetNode;
	
	//search 
	int result = get_node_by_path(path, &stbuf->st_ino, &toGetNode);
	


	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

