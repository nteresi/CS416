/*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	tfs.c
 *  Author: Yujie REN
 *	Date:	April 2019
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
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

struct superblock superblock;
bitmap_t inodeBitmap;
bitmap_t dataBitmap;
int num_blocks;
int diskstat = -1;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	printf("/// getting avail ino ///\n");

	void* buf = malloc(BLOCK_SIZE);
	bio_read(1, buf);
	memcpy(inodeBitmap, buf, MAX_INUM/8);

	int i;
	for(i = 0; i < MAX_INUM; i++) {

		if(get_bitmap(inodeBitmap, i) == 0) {
			set_bitmap(inodeBitmap, i);
			bio_write(1, inodeBitmap);
			
			printf("/// get successful %d ///\n", i);

			return i;
		}
	
	}

	printf("/// get failed ///\n");

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	printf("/// getting avail blk ///\n");

	/*void* buf = malloc(BLOCK_SIZE);
	bio_read(2, buf);
	memcpy(dataBitmap, buf, num_blocks/8);*/

	bio_read(2, dataBitmap);
	
	int i;
	for(i = 0; i < num_blocks; i++) {

		if(get_bitmap(dataBitmap, i)== 0) {
			set_bitmap(dataBitmap, i);
			bio_write(2, dataBitmap);

			printf("/// get successful %d ///\n", i+67);
			
			return i+67;
		}
	
	}

	printf("/// get failed ///\n");

	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number

	// Step 2: Get offset of the inode in the inode on-disk block

	// Step 3: Read the block from disk and then copy into inode structure

	printf("/// attempting readi ///\n");

	int block_num = ino/16 + 3;

	struct inode* inodes = malloc(BLOCK_SIZE);
	bio_read(block_num, inodes);

	memcpy(inode, &inodes[ino % 16], sizeof(struct inode));

	printf("/// readi successful ///\n");

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk

	printf("/// attempting writei ///\n");

	int block_num = ino/16 + 3;

	struct inode* inodes = malloc(BLOCK_SIZE);
	bio_read(block_num, inodes);

	memcpy(&inodes[ino % 16], inode, sizeof(struct inode));

	bio_write(block_num, inodes);

	printf("/// writei successful ///\n");

	return 0;
}

/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

	// Step 1: Call readi() to get the inode using ino (inode number of current directory)

	// Step 2: Get data block of current directory from inode

	// Step 3: Read directory's data block and check each directory entry.
	// If the name matches, then copy directory entry to dirent structure

	printf("/// attempting dir_find for %s ///\n", fname);

	struct inode* inode = malloc(sizeof(struct inode));
	readi(ino, inode);

	struct dirent* dirents = malloc(BLOCK_SIZE);

	int b, d;
	for (b = 0; b < 16; b++) {
		if (inode->direct_ptr[b] != 0) {
			bio_read(inode->direct_ptr[b], dirents);
			for (d = 0; d < 16; d++) {
				if (dirents[d].valid == 1) {
					if (strcmp(dirents[d].name, fname) == 0) {
						memcpy(dirent, &dirents[d], sizeof(struct dirent));

						printf("/// dir_find successful ///\n");

						return inode->direct_ptr[b];
					}
				}
			}
		}
	}

	printf("/// dir_find failed ///\n");

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	// Step 2: Check if fname (directory name) is already used in other entries
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	// Allocate a new data block for this directory if it does not exist
	// Update directory inode
	// Write directory entry

	printf("/// attempting dir_add for %s ///\n", fname);

	int b, d;

	struct dirent* dirents, *dirent;
	dirent = malloc(sizeof(struct dirent));
	dirents = malloc(BLOCK_SIZE);

	if (dir_find(dir_inode.ino, fname, name_len, dirent) != 0) {

		printf("/// dir_add failed ///\n");

		return 0;
	} else {
		dirent->ino = f_ino;
		dirent->valid = 1;
		strncpy(dirent->name, fname, sizeof(dirent->name));
		dirent->name[name_len] = '\0';
		//memcpy(dirent->name, fname, 252);
		for (b = 0; b < 16; b++) {
			if (dir_inode.direct_ptr[b] == 0) {
				dir_inode.direct_ptr[b] = get_avail_blkno();
				writei(dir_inode.ino, &dir_inode);
				bio_write(dir_inode.direct_ptr[b], dirent);

				printf("/// dir_add successful new block///\n");

				return 0;
			} else {
				bio_read(dir_inode.direct_ptr[b], dirents);
				for (d = 0; d < 16; d++) {
					if (dirents[d].valid == 0) {
						memcpy(&dirents[d], dirent, sizeof(struct dirent));
						bio_write(dir_inode.direct_ptr[b], dirents);
						dir_inode.link++;
						writei(dir_inode.ino, &dir_inode);

						printf("/// dir_add successful ///\n");

						return 0;
					}
				}
			}
		}
	}

	printf("/// dir_add failed ///\n");

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	
	printf("/// attempting dir_remove ///\n");

	int blk, d;

	struct dirent* dirents, *dirent;
	dirent = malloc(sizeof(struct dirent));

	if ((blk = dir_find(dir_inode.ino, fname, name_len, dirent)) != 0) {
		dirents = malloc(BLOCK_SIZE);
		bio_read(blk, dirents);
		for (d = 0; d < 16; d++) {
			if (dirents[d].valid == 1) {
				if (strcmp(dirents[d].name, fname) == 0) {
					dirents[d].valid = 0;
					bio_write(blk, dirents);

					printf("/// dir_remove successful ///\n");

					free(dirent);
					free(dirents);
					return 0;
				}
			}
		}
	}

	printf("/// dir_remove failed ///\n");

	free(dirent);
	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	printf("/// getting node by path for %s ///\n", path);

	if (strcmp(path, "/") == 0) {
		readi(0, inode);

		printf("/// node found ///\n");

		return 0;
	}

	struct inode* root = malloc(sizeof(struct inode));
	readi(ino, root);

	char* first = strtok(strdup(path), "/");
	if (first == NULL) {

		printf("/// can't get node first ///\n");	

		return -ENOENT;
	}

	char* next = strdup(first);
	int curr_ino = root->ino;
	struct dirent* next_dir = malloc(sizeof(struct dirent));

	while (next != NULL) {
		if (dir_find(curr_ino, next, 0, next_dir) == 0) {

			printf("/// can't get node next ///\n");

			return -ENOENT;
		}
		curr_ino = next_dir->ino;
		next = strtok(NULL, "/");
	}

	readi(curr_ino, inode);

	printf("/// node found ///\n");

	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	printf("/// attempting mkfs ///\n");

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	
	// write superblock information
	struct superblock* sb = malloc(BLOCK_SIZE);
	sb->magic_num = MAGIC_NUM;
	sb->max_inum = MAX_INUM;
	sb->max_dnum = MAX_DNUM;

	// initialize inode bitmap
	inodeBitmap = malloc(BLOCK_SIZE);
	memset(inodeBitmap, 0, BLOCK_SIZE);
	sb->i_bitmap_blk = 1;
	sb->i_start_blk = 3;

	// initialize data block bitmap
	num_blocks = (DISK_SIZE/BLOCK_SIZE) - 67;
	dataBitmap = malloc(BLOCK_SIZE);
	memset(dataBitmap, 0, BLOCK_SIZE);
	sb->d_bitmap_blk = 2;
	sb->d_start_blk = 67;

	// update bitmap information for root directory
	set_bitmap(inodeBitmap, 0);

	// update inode for root directory
	struct inode root;
	root.ino = 0;
	root.valid = 1;
	root.size = BLOCK_SIZE;
	root.type = 0;
	root.link = 2;
	memset(root.direct_ptr, 0, 16*sizeof(int));
	memset(root.indirect_ptr, 0, 8*sizeof(int));

	bio_write(0, sb);
	printf("/// sb write ///\n");
	bio_write(1, inodeBitmap);
	printf("/// ib write ///\n");
	bio_write(2, dataBitmap);
	printf("/// db write ///\n");
	writei(0, &root);
	printf("/// root write ///\n");

	diskstat = 0;

	printf("/// mkfs successful ///\n");

	return 0;
}

/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

        // Step 1a: If disk file is not found, call mkfs
	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk

	printf("/// attempting init ///\n");

        if(diskstat < 0) {
                tfs_mkfs();
        } else {
                bio_read(0, &superblock);
		bio_read(1, inodeBitmap);
		bio_read(2, dataBitmap);
        }

	printf("/// init successful ///\n");

	return NULL;

}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	// Step 2: Close diskfile

	printf("/// attempting destroy ///\n");

	free(inodeBitmap);
	free(dataBitmap);
	diskstat = -1;
	dev_close();

	printf("/// destroy successful ///\n");

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	// Step 2: fill attribute of file into stbuf from inode

	printf("/// attempting getattr for %s ///\n", path);

	struct inode* inode = malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, inode) == -ENOENT) {

		printf("/// getattr failed ///\n"); 

		return -ENOENT;
	}

	if (inode->type == 0){
		stbuf->st_mode   = S_IFDIR;
		stbuf->st_nlink  = inode->link;

		printf("/// is dir ///\n");

	} else {

		printf("/// is file ///\n");

		stbuf->st_mode   = S_IFREG;
		stbuf->st_nlink  = inode->link;
	}
	
	stbuf->st_ino    = inode->ino;
	stbuf->st_uid    = getuid();
	stbuf->st_gid    = getgid();
	stbuf->st_size   = inode->size;
	stbuf->st_mtime  = time(NULL);

	//printf("/// ino = %d, mode = %d, nlink = %d, uid = %d, gid = %d, size = %d ///\n", stbuf->st_ino, stbuf->st_mode, stbuf->st_nlink, stbuf->st_uid, stbuf->st_gid, stbuf->st_size);

	printf("/// getattr successful ///\n");

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
	
	// Step 1: Call get_node_by_path() to get inode from path
	// Step 2: If not find, return -1

	printf("/// attempting opendir ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	if ((get_node_by_path(path, 0, inode)) == 0) {
		
		printf("/// opendir successful ///\n");

		return 0;
	}

	printf("/// opendir not successful ///\n");

	return -1;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	// Step 2: Read directory entries from its data blocks, and copy them to filler

	printf("/// attempting readdir ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	if ((get_node_by_path(path, 0, inode)) == -ENOENT) {
		
		printf("/// readdir unsuccessful ///\n");

		return -ENOENT;
	}

	struct dirent* dirents = malloc(BLOCK_SIZE);
	int b, d;
	for (b = 0; b < 16; b++) {
		if (inode->direct_ptr[b] != 0) {
			bio_read(inode->direct_ptr[b], dirents);
			for (d = 0; d < 16; d++) {
				if (dirents[d].valid == 1) {
					filler(buffer, dirents[d].name, NULL, 0);
				}
			}
		} 
	}

	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	printf("/// readdir successful ///\n");

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
	
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	// Step 2: Call get_node_by_path() to get inode of parent directory
	// Step 3: Call get_avail_ino() to get an available inode number
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	// Step 5: Update inode for target directory
	// Step 6: Call writei() to write inode to disk

	printf("/// attempting mkdir for %s ///\n", path);

	struct inode* inode, *new;
	inode = malloc(sizeof(struct inode));
	if ((get_node_by_path(dirname(strdup(path)), 0, inode)) == -ENOENT) {

		printf("/// mkdir unsuccessful ///\n");

		return -ENOENT;
	}

	new = malloc(sizeof(struct inode));
	new->ino = get_avail_ino();
	new->valid = 1;
	new->size = BLOCK_SIZE;
	new->type = 0;
	new->link = 2;
	memset(new->direct_ptr, 0, sizeof(int)*16);
	memset(new->indirect_ptr, 0, sizeof(int)*8);

	dir_add(*inode, new->ino, basename(strdup(path)), strlen(basename(strdup(path))));
	writei(new->ino, new);

	/*readi(0, inode);
	struct dirent* dirents = malloc(sizeof(struct dirent)*16);
	bio_read(inode->direct_ptr[0], dirents);
	printf("/// size = %d, name = %s, ino = %d ///\n", inode->size, dirents[0].name, dirents[0].ino);

	readi(new->ino, inode);
	printf("/// ino = %d, val = %d, size = %d ///\n", inode->ino, inode->valid, inode->size);*/

	printf("/// mkdir successful ///\n");

	return 0;

}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	// Step 2: Call get_node_by_path() to get inode of target directory
	// Step 3: Clear data block bitmap of target directory
	// Step 4: Clear inode bitmap and its data block
	// Step 5: Call get_node_by_path() to get inode of parent directory
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	printf("/// attempting rmdir ///\n");

	struct inode* inode = malloc(sizeof(struct inode));	
	get_node_by_path(basename(strdup(path)), 0, inode);

	bio_read(1, inodeBitmap);
	unset_bitmap(inodeBitmap, inode->ino);
	bio_write(1, inodeBitmap);

	bio_read(2, dataBitmap);
	int p;
	for (p = 0; p < 16; p++) {
		if (inode->direct_ptr[p] != 0) {
			unset_bitmap(dataBitmap, inode->direct_ptr[p]-67);
		}
	}
	bio_write(2, dataBitmap);

	get_node_by_path(dirname(strdup(path)), 0, inode);
	dir_remove(*inode, basename(strdup(path)), 0);

	printf("/// rmdir successful ///\n");

	return 0;

}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	// Step 2: Call get_node_by_path() to get inode of parent directory
	// Step 3: Call get_avail_ino() to get an available inode number
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	// Step 5: Update inode for target file
	// Step 6: Call writei() to write inode to disk

	printf("/// attempting create ///\n");

	struct inode* inode, *new;
	inode = malloc(sizeof(struct inode));
	if ((get_node_by_path(dirname(strdup(path)), 0, inode)) == -ENOENT) {

		printf("/// create unsuccessful ///\n");

		return -ENOENT;
	}

	new = malloc(sizeof(struct inode));
	new->ino = get_avail_ino();
	new->valid = 1;
	new->size = 0;
	new->type = 1;
	new->link = 1;
	memset(new->direct_ptr, 0, sizeof(int)*16);
	memset(new->indirect_ptr, 0, sizeof(int)*8);

	dir_add(*inode, new->ino, basename(strdup(path)), strlen(basename(strdup(path))));
	writei(new->ino, new);

	readi(new->ino, inode);

	printf("/// create successful ///\n");

	return 0;

}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	printf("/// attempting open ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	if ((get_node_by_path(path, 0, inode)) == 0) {
		return 0;
	}

	printf("/// open successful ///\n");

	return -1;

}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	// Step 2: Based on size and offset, read its data blocks from disk
	// Step 3: copy the correct amount of data from offset to buffer
	// Note: this function should return the amount of bytes you copied to buffer

	printf("/// attempting read ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	int blk_strt = offset/BLOCK_SIZE;
	int blk_end = (offset+size)/BLOCK_SIZE;
	int num_blks = blk_end - blk_strt;
	int blks_read = 0;

	printf("/// strt = %d, end = %d, size = %d, offset = %d ///\n", blk_strt, blk_end, size, offset);

	void* buf = malloc(BLOCK_SIZE);
	int* indir_blks = malloc(BLOCK_SIZE);

	int p;
	for (p = blk_strt; p < blk_strt + num_blks; p++) {
		if (p < 16) {
			bio_read(inode->direct_ptr[p], buf);
			memcpy(buffer + BLOCK_SIZE*blks_read, buf, BLOCK_SIZE);
			blks_read++;
		} else {
			bio_read(inode->indirect_ptr[p/1024], indir_blks);
			bio_read(indir_blks[p % 1024], buf);
			memcpy(buffer + BLOCK_SIZE*blks_read, buf, BLOCK_SIZE);
		}
	}

	printf("/// read successful, buf[0] = %d ///\n", buffer[0]);

	return size;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	// Step 2: Based on size and offset, read its data blocks from disk
	// Step 3: Write the correct amount of data from offset to disk
	// Step 4: Update the inode info and write it to disk
	// Note: this function should return the amount of bytes you write to disk

	printf("/// attempting write ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	int blk_strt = offset/BLOCK_SIZE;

	printf("/// strt = %d ///\n", blk_strt);

	void* buf = malloc(BLOCK_SIZE);
	
	if (blk_strt < 16) {
		if (inode->direct_ptr[blk_strt] == 0) {
			inode->direct_ptr[blk_strt] = get_avail_blkno();
		}

		bio_read(inode->direct_ptr[blk_strt], buf);
		memcpy(buf, buffer, BLOCK_SIZE);
		bio_write(inode->direct_ptr[blk_strt], buf);

		inode->size += size;
		writei(inode->ino, inode);

		printf("/// write successful: %d, buf[0] = %c ///\n", inode->size, ((char*)buf)[0]);

		return size;
	}

	int* indir_blks = malloc(BLOCK_SIZE);

	blk_strt = (blk_strt-16) % 1024;
	
	int ind_blk_strt = blk_strt/1024;

	if (inode->indirect_ptr[ind_blk_strt] == 0) {
		inode->indirect_ptr[ind_blk_strt] = get_avail_blkno();
		memset(indir_blks, 0, BLOCK_SIZE);
	} else {
		bio_read(inode->indirect_ptr[ind_blk_strt], indir_blks);
	}

	if (indir_blks[blk_strt] == 0) {
		indir_blks[blk_strt] = get_avail_blkno();
		bio_write(inode->indirect_ptr[ind_blk_strt], indir_blks);
	}
	
	bio_read(indir_blks[blk_strt], buf);
	memcpy(buf, buffer, BLOCK_SIZE);
	bio_write(indir_blks[blk_strt], buf);

	inode->size += size;

	writei(inode->ino, inode);

	printf("/// write successful: %d, buf[0] = %c ///\n", inode->size, ((char*)buf)[0]);

	/*if (blk_strt == 1000) {
		exit(0);
	}*/

	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	// Step 2: Call get_node_by_path() to get inode of target file
	// Step 3: Clear data block bitmap of target file
	// Step 4: Clear inode bitmap and its data block
	// Step 5: Call get_node_by_path() to get inode of parent directory
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	printf("/// attempting unlink ///\n");

	struct inode* inode = malloc(sizeof(struct inode));
	get_node_by_path(basename(strdup(path)), 0, inode);

	bio_read(1, inodeBitmap);
	unset_bitmap(inodeBitmap, inode->ino);
	bio_write(1, inodeBitmap);

	bio_read(2, dataBitmap);
	int p;
	for (p = 0; p < 16; p++) {
		if (inode->direct_ptr[p] != 0) {
			unset_bitmap(dataBitmap, inode->direct_ptr[p]-67);
		}
	}
	bio_write(2, dataBitmap);

	get_node_by_path(dirname(strdup(path)), 0, inode);
	dir_remove(*inode, basename(strdup(path)), 0);

	printf("/// unlink successful ///\n");

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}
