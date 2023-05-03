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

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

char diskfile_path[PATH_MAX];
size_t inode_size = sizeof(struct inode);


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
	printf("inside get_avail_ino\n");

	// Step 1: Read inode bitmap from disk
	bio_read(superblock->i_bitmap_blk,inode_bitmap);
	int count;
	
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i < MAX_INUM; i++){
		if(get_bitmap(inode_bitmap, i)==0){
			count = i;
			break;
		}
	}

	// Step 3: Update inode bitmap and write to disk 
	 set_bitmap(inode_bitmap, count);
	bio_write(superblock->i_bitmap_blk,inode_bitmap);

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
		printf("inside get_avail_blkno\n");

	// Step 1: Read data block bitmap from disk
	
	bio_read(superblock->d_bitmap_blk,disk_bitmap);

	// Step 2: Traverse data block bitmap to find an available slot
	int count;
	for(int i = 0; i < MAX_DNUM; i++){
		if(get_bitmap(disk_bitmap, i)==0){
			count = i;
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(disk_bitmap, count);
	bio_write(superblock->d_bitmap_blk,disk_bitmap);
	
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
		printf("inside readi\n");


 	// Step 1: Get the inode's on-disk block number
	/*printf("superblock start i block number: %d \n",superblock->i_start_blk);
	printf("superblock start i block number: %d \n",superblock->d_bitmap_blk);
	printf("superblock start i block number: %d \n",superblock->i_bitmap_blk);
	printf("superblock start i block number: %d \n",superblock->d_start_blk);*/
  	int block_num = (ino/(BLOCK_SIZE/sizeof(struct inode)))+superblock->i_bitmap_blk;

  	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(struct inode));
	struct inode * inodeBlock = (struct inode*)malloc(BLOCK_SIZE);
  	// Step 3: Read the block from disk and then copy into inode structure
	printf("starting bio_read\n");
	printf("block_num: %d\n", block_num);
	bio_read(block_num,(void *)inodeBlock);
	
	inode = (inodeBlock+offset);
	free(inodeBlock);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {
	printf("inside writei\n");

	// Step 1: Get the inode's on-disk block number
  	int block_num = (ino/(BLOCK_SIZE/sizeof(inode)))+superblock->i_start_blk;

  	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = ino%(BLOCK_SIZE/sizeof(inode));

	// Step 3: Write inode to disk 
	struct inode *inodeblock = (struct inode *)malloc(BLOCK_SIZE) ;
	bio_read(block_num, (void*) inodeblock);

	memcpy(inodeblock+offset, &inode,sizeof(inode));
	bio_write(block_num,inodeblock);
	
	free(inodeblock);
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
		printf("inside dir_find\n");
	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode *inode = (struct inode*)malloc(sizeof(struct inode));
	readi(ino, inode);

	// Step 2: Get data block of current directory from inode
	// int* list_of_ptrs = inode->direct_ptr;
	struct dirent *dir_entry = (struct dirent*)malloc(sizeof(struct inode)*BLOCK_SIZE); 
	// Step 3: Read directory's data block and check each directory entry.
	int num_dirents = BLOCK_SIZE/sizeof(struct dirent);

	for(int i = 0; i < 16; i++) {
		if(inode->direct_ptr[i]==0){
			return -1;
		}
	
		 bio_read(inode->direct_ptr[i], dir_entry);
		 for(int j = 0; j < num_dirents; j++){
			//open each dirent one by one and compare names
			if(dir_entry->valid==1){
				if(strcmp(dir_entry->name, fname) == 0){
				*dirent = *dir_entry;
				return 0;
			}
			
			}
			dir_entry++;
		 }
		 
  }
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	printf("inside dir_add\n");
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *curr = (struct dirent*)malloc(sizeof(struct inode)*BLOCK_SIZE);
	int num_dirents = BLOCK_SIZE/sizeof(struct dirent);
 

	for(int i =0; i<16; i++){
		if(dir_inode.direct_ptr[i]==0){
			break;//data empty
		}
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
	int block=0;
	for(int i=0; i<16; i++){
		if(dir_inode.direct_ptr[i]==0){// need to create whole block
			dir_inode.direct_ptr[i]= get_avail_blkno;
			struct inode* newBlock = (struct inode*)malloc(BLOCK_SIZE);

			bio_write(dir_inode.direct_ptr[i], newBlock);
			free(newBlock);
			dir_inode.vstat.st_blocks++;
		}
		bio_read(dir_inode.direct_ptr[i],curr);
		newEntry = curr;

		for(int j=0; j< num_dirents; j++){
			if(newEntry->valid==0){
				newEntry->ino = f_ino;
				newEntry->valid =1;
				break;
			}
			newEntry++;
		}if(newEntry->valid==1){
			break;
		}
		block++;
	}
	

	// Allocate a new data block for this directory if it does not exist
	
	
	// Update directory inode
	dir_inode.size+=sizeof(newEntry);
	dir_inode.vstat.st_blocks++;
	time(&dir_inode.vstat.st_mtime);
	// Write directory entry
	struct inode* ptrdir = (struct inode*)malloc(sizeof(struct inode));
	writei(dir_inode.ino,ptrdir);
	bio_write(dir_inode.direct_ptr[block],curr);
	return 0;
}
//WE DONT HAVE TO DO THIS WOOHOO!!!!
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// // Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	// uint16_t ino = dir_inode.ino;

	// //this might not find it becasue we are looking for the directory block
	// //do we need to go into its inode directoryblock?
	// struct dirent *curr = (struct dirent*)malloc(sizeof(struct inode)*BLOCK_SIZE); 
	// for(int i =0; i<16; i++){
	// 	bio_read(dir_inode.direct_ptr[i],curr);
	// 	for(int j = 0;  j< (BLOCK_SIZE / sizeof(struct dirent)); j++){
	// 		if(curr->valid==1){
	// 		if(strncmp(curr->name, fname, name_len)){
	// 			memset(curr,0,sizeof(curr));
	// 			bio_write(dir_inode.direct_ptr[i],curr);
	// 			dir_inode.direct_ptr[i] = 0;
	// 		}
	// 		}
	// 		curr++;
	// 	}
	// }
	
	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	printf("inside get_node_by_path\n");
    // Step 1: Resolve the path name, walk through path, and finally, find its inode.
    // Note: You could either implement it in a iterative way or recursive way.

	//struct dirent *currentEntry = (struct dirent *)malloc(sizeof(struct dirent));
	//currentEntry->ino =0;
	char *new_path = strdup(path);	//the only way to copy a string apparently
	char * delim = (char *)malloc(2 * sizeof(char));
	delim[0] = '/' ;
	delim[1] = '\0'; 
	char * tokenized = strtok(new_path, delim) ;
	printf("new_path: %c\n", new_path);
	//char *tokenized = strtok(new_path, "/"); //breaks up path into individual parts ex: root/foo/bar = root, foo, bar
	printf("tokenized: %c\n", tokenized);
	if(strcmp(path,delim)==0){
		tokenized =NULL;
	}
	struct dirent *dir_entry = (struct dirent*)malloc(sizeof(struct dirent));
	uint16_t curr = ino;
	dir_entry->ino = ino; 
	while(tokenized != NULL){
		int checker = dir_find(dir_entry->ino,(const char *)tokenized, (size_t)strlen(tokenized), dir_entry);
		if(checker==-1){
			free(new_path);	//free the strdup
			return -1;
		}
		curr = dir_entry->ino; // gets next ino through dirent
		printf("ino: %d",curr);
		tokenized = strtok(NULL, delim); //move to next directory
	}

	dir_entry->ino = curr;
	readi(curr, inode);//reads last inode
	
	free(new_path);	//free the strdup
	printf("finsihed getnodebypath\n");
	return 0;
}
/* 
 * Make file system
 */
int rufs_mkfs() {
	printf("inside rufs_mkfs\n");
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	printf(" creating superblock\n");
	// write superblock information
	///* start block of inode bitmap */  just make it 1
	superblock = (struct superblock*)malloc(BLOCK_SIZE);
	superblock->magic_num=MAGIC_NUM;
	superblock->max_inum=MAX_INUM;
	superblock->max_dnum=MAX_DNUM;
	superblock->i_bitmap_blk = 1 ;
	superblock->d_bitmap_blk = 2 ;
	superblock->i_start_blk = 3 ;
	superblock->d_start_blk = 3 + ((sizeof(struct inode) * MAX_INUM) / BLOCK_SIZE) ;
	
	/*printf("superblock start i block number: %d \n",superblock->i_start_blk);
	printf("superblock start i block number: %d \n",superblock->d_bitmap_blk);
	printf("superblock start i block number: %d \n",superblock->i_bitmap_blk);
	printf("superblock start i block number: %d \n",superblock->d_start_blk);*/
	bio_write(0, superblock);
	printf("wrote to disk\n");

	// initialize inode bitmap
	printf("creating inode bitmap\n");
	inode_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	
	
	// initialize data block bitmap
	printf("creating disk bitmap\n");
	disk_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	 
	// update bitmap information for root directory
	printf("setting inode bitmap\n");
	set_bitmap(inode_bitmap,0);//first node is set as the root
	printf("setting disk bitmap\n");
	set_bitmap(disk_bitmap,0);//first node is set as the root
	printf("writing inode bitmap to disk\n");
	bio_write(superblock->i_bitmap_blk, &inode_bitmap);
	printf("writing disk bitmap to disk\n");
	bio_write(superblock->d_bitmap_blk,&disk_bitmap);


	// update inode for root directory
	printf("creating inode\n");
	struct inode* root_inode = (struct inode*)malloc(sizeof(struct inode));
	root_inode->ino=1;
	root_inode->valid =1;

	printf("setting root inide direct ptr\n");
	root_inode->direct_ptr[0] = superblock->d_bitmap_blk;
	root_inode->direct_ptr[1]=0;
	
	printf("creating stats for inode\n");
	struct stat * rstat = (struct stat*)malloc(sizeof(struct stat));
	//dont use this for nodes or somthing make sure for non-directories you don't set the mode as this
	rstat->st_mode   = S_IFDIR | 0755;
	rstat->st_nlink  = 2;
	time(&rstat->st_mtime);
	rstat->st_blksize=BLOCK_SIZE;
	rstat->st_blocks=1;
	root_inode->vstat=*rstat;
	bio_write(superblock->i_start_blk, &root_inode);
	free(rstat);

	printf("creating root dirent for root inode\n");
	struct dirent *rootDirent = (struct dirent*)malloc(BLOCK_SIZE);
	printf("creating dirent for root inode0\n");
	rootDirent->ino=1;
	//we need to add a terminating character like this for some reason 
	char nameRoot[2];
	nameRoot[0] = '/' ;
	nameRoot[1] = '\0';
	printf("creating dirent for root inode1\n");
	strncpy(rootDirent->name, nameRoot,2);

	printf("creating dirent for root inode2\n");

	struct dirent *rootDirent1 = rootDirent+1;
	char nameDirent1[2];
	nameDirent1[0] = '.' ;
	nameDirent1[1] = '\0';
	strncpy(rootDirent1->name, nameDirent1,2);
	rootDirent1->ino=1;
	rootDirent1->valid=1;
	
	printf("creating dirent for root inode3\n");

	struct dirent *rootDirent2 = rootDirent1+1;
	rootDirent2->ino=1;
	rootDirent2->valid=1;
	char nameDirent2[3];
	printf("creating name for dirent2");
	nameDirent2[0] = '.' ;
	nameDirent2[1] = '.' ;
	nameDirent2[2] = '\0';
	printf("setting name for dirent2");
	strncpy(rootDirent2->name, nameDirent2,3);
	printf("starting biowrite");
	bio_write(superblock->d_start_blk, rootDirent);
	//for(int )
	set_bitmap(disk_bitmap,1);
	printf("done with rufs_mkfs");
	free(rootDirent);
	free(root_inode);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	  printf("inside rufs_init\n");
	

	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path)==-1){
		rufs_mkfs();
		return NULL;
	}else{superblock = (struct superblock*)malloc(BLOCK_SIZE);
	bio_read(0,superblock);
	inode_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	disk_bitmap= (bitmap_t)malloc(BLOCK_SIZE);
	bio_read(superblock->i_bitmap_blk,inode_bitmap);
	bio_read(superblock->d_bitmap_blk,disk_bitmap);
	}

// Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	

	return 0;
}

static void rufs_destroy(void *userdata) {
	printf("inside rufs_destroy\n");
	// Step 1: De-allocate in-memory data structures
	free(superblock);
	free(inode_bitmap);
	free(disk_bitmap);

	// Step 2: Close diskfile
	dev_close();
}

static int rufs_getattr(const char *path, struct stat *stbuf) {
	printf("inside rufs_getattr\n");
	// Step 1: call get_node_by_path() to get inode from path
	struct inode* toGetNode;
	
	//search 
	int result = get_node_by_path(path, 1, toGetNode);
	
	// Step 2: fill attribute of file into stbuf from inode
	//we might not need this/*
	*stbuf = toGetNode->vstat;
	/*
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = toGetNode->vstat.st_nlink;
		stbuf->st_gid= getgid();
		stbuf->st_uid= getuid();
		stbuf->st_size = toGetNode->vstat.st_size;
		time(&stbuf->st_mtime);*/

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
	printf("inside rufs_opendir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	uint16_t ino = malloc(sizeof(uint16_t));
	struct inode *toGet = malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	return get_node_by_path(path,1,toGet);
    
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
printf("inside rufs_readdir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode dir_inode;
    int node_num = get_node_by_path(path, 1, &dir_inode);

	
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	for (int i = 0; i < BLOCK_SIZE / sizeof(struct dirent); i++){
		if (dir_inode.direct_ptr[i] == 0) {
            continue;
        }

       char data_block[BLOCK_SIZE];
        bio_read(dir_inode.direct_ptr[i], data_block);

        for (int j = 0; j < (BLOCK_SIZE / sizeof(struct dirent)); j++) {
            struct dirent *dir_entry = (struct dirent *) (data_block + j * sizeof(struct dirent));
            if (dir_entry->name[0] == 0) {
                break;  // end of directory entries
            }
            filler(buffer, dir_entry->name, NULL, 0);
        }
	}

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {
	printf("inside rufs_mkdir\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char* path_copy = strdup(path);
    char* parent_dir_path = dirname(path_copy);
    char* target_dir_name = basename(path_copy);
	
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode parent_dir_inode;
    get_node_by_path(parent_dir_path, 1, &parent_dir_inode);

	// Step 3: Call get_avail_ino() to get an available inode number
	uint16_t target_ino = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(parent_dir_inode, target_ino, target_dir_name, strlen(target_dir_name));

	// Step 5: Update inode for target directory
	//initialize direct pointers maybe
	struct inode new_inode = {
		.ino = target_ino, // inode number of root directory
		.valid = 1, // root directory is valid
		.size = 0, //  directory has no size (no data block)
		.type = 0, //  type is directory
		.link = 1, //  directory has one hard link to parent
		.direct_ptr = {0}, 
		.indirect_ptr = {0}, 
	};

	struct stat * rstat = (struct stat*)malloc(sizeof(struct stat));
	//dont use this for nodes or somthing make sure for non-directories you don't set the mode as this
	rstat->st_mode   = S_IFDIR | 0755;
		rstat->st_nlink  = 2;
		time(&rstat->st_mtime);
		rstat->st_blksize=BLOCK_SIZE;
		rstat->st_blocks=1;
		new_inode.vstat=*rstat;

	// Step 6: Call writei() to write inode to disk
	writei(target_ino, &new_inode);

	return 0;
}

//We dont have to do this one too!!!!
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
	printf("inside rufs_create\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name 
	char* path_copy = strdup(path);
    char* parent_dir_path = dirname(path_copy);
    char* target_dir_name = basename(strdup(path));
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_inode = (struct inode*)malloc(sizeof(struct inode)); 

	get_node_by_path(parent_dir_path,1,  parent_inode);
	// Step 3: Call get_avail_ino() to get an available inode number
	int availableInode = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*parent_inode, availableInode,(const char *)target_dir_name, strlen(target_dir_name));
	// Step 5: Update inode for target file
	struct inode* fileInode = (struct inode*)malloc(sizeof(struct inode));
	fileInode->ino=availableInode;
	fileInode->direct_ptr[0]=get_avail_blkno;
	fileInode->valid =1; 
	fileInode->direct_ptr[1]=0;
	fileInode->size=0;
	
	struct stat * rstat = (struct stat*)malloc(sizeof(struct stat));
	
	rstat->st_mode   = S_IFREG | 0666;
		
		time(&rstat->st_mtime);
		rstat->st_nlink=1;
		rstat->st_blocks=1;
		fileInode->vstat=*rstat;
		bio_write(superblock->i_start_blk, fileInode);
		free(rstat);
	// Step 6: Call writei() to write inode to disk
	writei(availableInode,fileInode);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {
	printf("inside rufs_open\n");
	// Step 1: Call get_node_by_path() to get inode from path
	uint16_t ino = malloc(sizeof(uint16_t));
	struct inode *toGet = malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	return get_node_by_path(path,1,toGet);

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("inside rufs_read\n");
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode inode;
	int inum = get_node_by_path(path, 1, &inode);
		
	// Step 2: Based on size and offset, read its data blocks from disk
	size_t bytes_read = 0;
	char block[BLOCK_SIZE];
	for(int i = 0; i < size/BLOCK_SIZE; i++){
		bio_read(inode.direct_ptr[i], block);
		size_t chunk_size = min(size - bytes_read, BLOCK_SIZE - offset % BLOCK_SIZE);
		memcpy(buffer + bytes_read, block + offset % BLOCK_SIZE, chunk_size);
		bytes_read += chunk_size;
	}
    
	// Step 3: copy the correct amount of data from offset to buffer
	time(&inode.vstat.st_mtime);
	return bytes_read;
	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("inside rufs_write\n");
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode inode;
	int inum = get_node_by_path(path, 1, &inode);
		
	// Step 2: Based on size and offset, read its data blocks from disk
	size_t bytes_written = 0;
	char block[BLOCK_SIZE];
	for(int i = 0; i < size/BLOCK_SIZE; i++){
		bio_read(inode.direct_ptr[i], block);
		size_t chunk_size = min(size - bytes_written, BLOCK_SIZE - offset % BLOCK_SIZE);
        memcpy(block + offset % BLOCK_SIZE, buffer + bytes_written, chunk_size);
		bytes_written += chunk_size;
		bio_write(inode.direct_ptr[i], buffer);
		// Update the bytes written and offset for the next block
        bytes_written += chunk_size;
        offset += chunk_size;
	}
	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk
	int blockswritten=bytes_written/BLOCK_SIZE;
	inode.vstat.st_blocks+=blockswritten;
	inode.vstat.st_size+=bytes_written;
	time(&inode.vstat.st_mtime);
	// Note: this function should return the amount of bytes you write to disk
	return size;
}

//DONT HAVE TO DO!!!!
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
