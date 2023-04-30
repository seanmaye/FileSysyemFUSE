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
struct dirent my_dirent;
size_t inode_size = sizeof(my_inode);
void* phys_disk;

// Declare your in-memory data structures here
struct inode my_inode;
bitmap_t inode_bitmap; 
bitmap_t disk_bitmap; 
//i think we should have a global bitmap
struct superblock *superblock;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bio_read(superblock->i_bitmap_blk,inode_bitmap);
	int count;
	
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i < MAX_INUM; i++){
		if(inode_bitmap[i]==0){
			count = i;
			break;
		}
	}

	// Step 3: Update inode bitmap and write to disk 
	inode_bitmap[count]=1;
	bio_write(superblock->i_bitmap_blk,inode_bitmap);

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	bio_read(superblock->d_bitmap_blk,phys_disk);
	disk_bitmap = phys_disk;

	// Step 2: Traverse data block bitmap to find an available slot
	int count;
	for(int i = 0; i < MAX_DNUM; i++){
		if(disk_bitmap[i]==0){
			count = i;
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	disk_bitmap[count]= 1;
	bio_write(superblock->d_bitmap_blk,disk_bitmap);
	
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

 	// Step 1: Get the inode's on-disk block number
  	int block_num = ino/(BLOCK_SIZE/sizeof(inode))+3;

  	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(inode));

  	// Step 3: Read the block from disk and then copy into inode structure
	bio_read(block_num,phys_disk);
	
	inode = &phys_disk+offset;

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
  	int block_num = (ino/(BLOCK_SIZE/sizeof(inode)))+3;

  	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(inode));

	// Step 3: Write inode to disk 


	// use bio read to get 
	// bio_write(block_num,inode);
	
	bio_read(block_num, phys_disk);
	memcpy(&phys_disk+offset, &inode,sizeof(inode));
	bio_write(block_num,phys_disk);
	

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
  struct inode inode;
  readi(ino, &inode);

  // Step 2: Get data block of current directory from inode
	
	int* list_of_ptrs = inode.direct_ptr;
	struct dirent *dir_entry;
  // Step 3: Read directory's data block and check each directory entry.
  int num_dirents = BLOCK_SIZE/sizeof(my_dirent);
  for(int i = 0; i < 16; i++) {
		 bio_read(inode.direct_ptr[i], phys_disk);
		 for(int j = 0; j < num_dirents; j++){
			//open each dirent one by one and compare names
			dir_entry = (struct dirent*)(phys_disk+j * sizeof(dirent));
			if(strcmp(dir_entry->name, fname) == 0){
				dirent = dir_entry;
			}
		 }

		 
  }
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *curr = (struct dirent*)malloc(sizeof(struct inode)*BLOCK_SIZE); 
	for(int i =0; i<16; i++){
		bio_read(dir_inode.direct_ptr[i],curr);
		for(int j = 0;  j< (BLOCK_SIZE / sizeof(struct dirent)); j++){
			if(curr->valid==1){
			if(strncmp(curr->name, fname, name_len)){
				return -1; //name already in use
			}
			}
			curr++;
		}
	}
	
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	struct dirent *newEntry = (struct dirent*)malloc(sizeof(struct dirent));
	newEntry->ino=dir_inode.ino;
	newEntry->valid=1;
	newEntry->len=strlen(fname);
	strcpy(newEntry->name,fname);
	
	for(int i=0; i<16; i++){
		if(dir_inode.direct_ptr[i]==NULL){
			bio_write(dir_inode.direct_ptr[i], newEntry);
		}
		
	}
	

	// Allocate a new data block for this directory if it does not exist
	
	// Update directory inode
	dir_inode.size+=sizeof(newEntry);
	dir_inode.vstat.st_blocks++;
	time(&dir_inode.vstat.st_mtime);
	// Write directory entry
	
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	uint16_t ino = dir_inode.ino;
	int block_num = (ino/(BLOCK_SIZE/sizeof(my_inode)))+3;
	bio_read(block_num,phys_disk);
	
	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
    // Step 1: Resolve the path name, walk through path, and finally, find its inode.
    // Note: You could either implement it in a iterative way or recursive way.
	//can u try to do a dir method? lsure ike dir_add. ill do some googling for this one

	//struct dirent *currentEntry = (struct dirent *)malloc(sizeof(struct dirent));
	//currentEntry->ino =0;
	char *new_path = strdup(path);	//the only way to copy a string apparently
	char *tokenized = strtok(new_path, "/"); //breaks up path into individual parts ex: root/foo/bar = root, foo, bar
	uint16_t curr = ino;
	while(tokenized != NULL){
		struct dirent dir_entry;
		int checker = dir_find(curr,tokenized, strlen(tokenized), &dir_entry);
		if(checker!=0){
			free(new_path);	//free the strdup
			return -1;
		}
		curr = dir_entry.ino; // gets next ino through dirent
		tokenized = strtok(NULL, "/"); //move to next directory
	}
	readi(curr, inode);//reads last inode
	free(new_path);	//free the strdup


	

	return 0;
}
/* 
 * Make file system
 */
int rufs_mkfs() {
	char* phys_disk = (char *)malloc(BLOCK_SIZE);
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	
	// write superblock information
	///* start block of inode bitmap */  just make it 1
	superblock = (struct superblock*)malloc(BLOCK_SIZE);
	superblock->magic_num=MAGIC_NUM;
	superblock->max_inum=MAX_INUM;
	superblock->max_dnum=MAX_DNUM;
	superblock->i_bitmap_blk=1;
	superblock->d_bitmap_blk=2;
	superblock->i_start_blk=3;
	superblock->d_bitmap_blk= superblock->i_start_blk = (sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE);
	bio_write(0, &superblock);

	// initialize inode bitmap
	inode_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	
	
	// initialize data block bitmap
	disk_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	 
	// update bitmap information for root directory

	set_bitmap(inode_bitmap,0);//first node is set as the root
	set_bitmap(disk_bitmap,0);//first node is set as the root
	bio_write(superblock->i_bitmap_blk, &inode_bitmap);
	bio_write(superblock->d_bitmap_blk,&disk_bitmap);


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
	
	
	
	struct stat * rstat = (struct stat*)malloc(sizeof(struct stat));
	rstat->st_mode   = S_IFDIR | 0755;
		rstat->st_nlink  = 2;
		time(&rstat->st_mtime);
		rstat->st_blksize=BLOCK_SIZE;
		rstat->st_blocks=1;
		root_inode.vstat=*rstat;
		bio_write(superblock->i_start_blk, &root_inode);
		free(rstat);

	
	struct dirent *rootDirent1 = (struct dirent*)malloc(BLOCK_SIZE);
	rootDirent1->ino=0;
	strcpy(rootDirent1->name, '.');
	rootDirent1->valid=1;
	rootDirent1->len=1; 
	bio_write(superblock->d_start_blk, rootDirent1);

	struct dirent second_dirent = {
		.ino = 0,
		.valid = 1,
		.name = "..",
		.len = 2,
	};

	struct dirent *rootDirent2 = (struct dirent*)malloc(BLOCK_SIZE);
	rootDirent2->ino=0;
	strcpy(rootDirent2->name, '..');
	rootDirent2->valid=1;
	rootDirent2->len=2; 
	bio_write(superblock->d_start_blk+1, rootDirent2);
	set_bitmap(disk_bitmap,1);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

if(dev_open(diskfile_path)){
	
	superblock = (struct superblock*)malloc(BLOCK_SIZE);
	bio_read(0,superblock);
	inode_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	disk_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	bio_read(superblock->i_bitmap_blk,inode_bitmap);
	bio_read(superblock->d_bitmap_blk,disk_bitmap);
}else{
	rufs_mkfs();
}
	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
free(superblock);
free(inode_bitmap);
free(disk_bitmap);

	// Step 2: Close diskfile
	dev_close();	

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode* toGetNode;
	
	//search 
	int result = get_node_by_path(path, &stbuf->st_ino, toGetNode);
	


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

