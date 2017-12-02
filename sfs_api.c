
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <fuse.h>
#include <strings.h>
#include <limits.h>
#include "disk_emu.h"
#define ZHU_DISHI_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows.
#define BLOCK_SIZE 1024
#define NUM_INODES 500
#define NUM_OF_FILES 256


superblock_t super_block;
file_descriptor file_descriptors[NUM_OF_FILES];
directory_entry directory_entry_tbl[NUM_OF_FILES];
inode_t inode_tbl[NUM_INODES];
/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)


//initialize all bits to high
// uint8_t free_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

//initialize super block
void init_super_block(){
  super_block.magic = 0;
  super_block.block_size = BLOCK_SIZE;
  super_block.fs_size = BLOCK_SIZE * NUM_BLOCKS;
    super_block.inode_table_len = NUM_INODES;
    int num = get_index();
    super_block.root_dir_inode = num;
  // todo
}

// init_directory entry table
void init_directory_entry(){
  for (int i = 0; i < NUM_OF_FILES; i ++){
    directory_entry_tbl[i].num = -1;
    for (int j = 0; j < MAX_FILE_NAME; j ++){
      directory_entry_tbl[i].name[j] = '\0';
    }
  }
}
//initialize file descriptor
void init_file_descriptor(){
  for (int i = 0; i < NUM_OF_FILES; i ++) {
    file_descriptors[i].inodeIndex = -1;
    file_descriptors[i].inode = NULL;
    file_descriptors[i].rwptr = 0;
  }
}

void init_inode_table() {
  for (int i = 0; i < NUM_INODES; i++) {
    inode_tbl[i].mode = 0;
    inode_tbl[i].link_cnt = 0;
    inode_tbl[i].uid = 0;
    inode_tbl[i].gid = 0;
    inode_tbl[i].size = -1;
    for (int j = 0; j < 12; j ++) {
      inode_tbl[i].data_ptrs[j] = -1;
    }
    inode_tbl[i].indirectPointer = -1;
  }
}

/* Formats the virtual disk implemented by the disk emulator and creates an instance of the simple
file system on top of it. The mksfs() has a fresh flag to signal that the file system should be created
scratch. If flag is false, the file system is opened from the disk. The support for persistence is
important so you can reuse existing data or create a new file system. */
void mksfs(int fresh) {
	if (fresh == 0){ // the flag is false, open from disk
    init_disk(ZHU_DISHI_DISK, BLOCK_SIZE, NUM_BLOCKS);
    // read super block from disk
    // read_blocks(0, 1, &super_block);
    // read inode table

    // read directory entry table

    // read bit map table
    init_inode_table();
    init_directory_entry();
    init_file_descriptor();
  }
  else{ // the flag is true, create from scratch
    init_fresh_disk(ZHU_DISHI_DISK, BLOCK_SIZE, NUM_BLOCKS);

    // init_super_block();
    // write superblock in sfs
    // write_blocks(get_index(), 1, &super_block)
    // printf("%d \n", free_bit_map[0]);
    // write bit map in sfs
    init_inode_table();
    init_directory_entry();
    init_file_descriptor();
  }
}

/* Copies the name of the next file in the directory into fname and returns non zero if there
is a new file. Once all the files have been returned, this function returns 0. So, you should
be able to use this function to loop through the directory. */
int sfs_getnextfilename(char *fname){

}

/* Returns the size of a given file */
int sfs_getfilesize(const char* path){

}
int sfs_fopen(char *name){
  if (strlen(name) > MAX_FILE_NAME){
    printf("File name to long\n");
    return -1;
  }
  // return the fd num if the file exists; fd represents the files that are open
  int i, j;
  for (i = 0; i < NUM_OF_FILES; i++) {
    if (strcmp(name, directory_entry_tbl[i].name) == 0){
      // check if the file is opened already
      for (j = 0; j < NUM_OF_FILES; j++) {
        if (directory_entry_tbl[i].num == file_descriptors[j].inodeIndex){
          printf("File already opened\n");
          return j;
        }
      }
      // allocate the first unused fd slot
      for (int k = 0; k < NUM_OF_FILES; k++) {

        if (file_descriptors[k].inode == NULL){
          file_descriptors[k].inodeIndex = directory_entry_tbl[i].num;
          file_descriptors[k].inode = &inode_tbl[directory_entry_tbl[i].num];
          file_descriptors[k].rwptr = inode_tbl[directory_entry_tbl[i].num].size;
          return k;
        }
      }

    }
  }
  // new file
  int createdEntry = 0, createdFD = 0, createdInode = 0;
  int fd = -1;
  // find the first unused inode
  for (i = 0; i < NUM_INODES; i++){
    if (inode_tbl[i].size == -1 && createdInode == 0){
      int num = get_index();
      if (num > 1022)return 0;
      inode_tbl[i].data_ptrs[0] = num;
      inode_tbl[i].size = 0;
      createdInode = 1;

      for (j = 0; j < NUM_OF_FILES; j++){
        if (directory_entry_tbl[j].num == -1){
          directory_entry_tbl[j].num = i;
          strcpy(directory_entry_tbl[j].name, name);
          createdEntry = 1;
          break;
        }
      }
      for (j = 0; j < NUM_OF_FILES; j++){
        if (file_descriptors[j].inode == NULL){
          file_descriptors[j].inode = &inode_tbl[i];
          file_descriptors[j].inodeIndex = i;
          file_descriptors[j].rwptr = 0;
          createdFD = 1;
          fd = j;
          break;
        }
      }
    }
  }
  return fd;
}

int sfs_fclose(int fileID) {

  int inode_num = file_descriptors[fileID].inodeIndex;
  if (inode_num == -1) return -1;
  file_descriptors[fileID].inodeIndex = -1;
  file_descriptors[fileID].inode = NULL;
  file_descriptors[fileID].rwptr = 0;
  return 0;
}

int sfs_fread(int fileID, char *buf, int length) {
  // Check if the file is opened or  the write length is valid
  int inode_num = file_descriptors[fileID].inodeIndex;
  int file_opened = 0;
  int i;
  for(i = 0; i < NUM_OF_FILES; i++){
    int num = directory_entry_tbl[i].num;
    if (num == -1)
      break;
    if (num == inode_num){
      file_opened = 1;
      break;
    }
  }
  if (file_opened == 0){
    printf("Required file is not opened\n");
    return -1;
  }
  if (length <= 0){
    printf("Length empty\n");
    return -1;
  }

  // read file
  int file_size = file_descriptors[fileID].inode->size;
  char read_temp[BLOCK_SIZE];
  int cur_read_len = 0;
  int cur_block = file_descriptors[fileID].rwptr / BLOCK_SIZE;
  int link = cur_block / 12;
  int cur_offset = file_descriptors[fileID].rwptr % BLOCK_SIZE;
  // check if the required read length is greater than the file size with the rwptr positioned
  // if so, only read the best we can
  int len = length;
  memset(buf, 0, len);
  length = (file_descriptors[fileID].rwptr + length) > file_size ? (file_size - file_descriptors[fileID].rwptr) : length;
  int new_len = length;
  inode_t cur_inode = inode_tbl[file_descriptors[fileID].inodeIndex];
  // navigate to the block where current rwptr is pointing to
  while (link != 0) {
    cur_inode = inode_tbl[cur_inode.indirectPointer];
    link --;
  }
  read_blocks(cur_inode.data_ptrs[cur_block % 12], 1, read_temp);
  int read_len = length > BLOCK_SIZE - cur_offset ? BLOCK_SIZE - cur_offset : length;
  memcpy(buf, read_temp + cur_offset, read_len);
  cur_read_len += read_len;
  file_descriptors[fileID].rwptr += read_len;
  length -= read_len;
  cur_block++;
  while (length > 0){
    if (cur_block % 12 == 0){
      cur_inode = inode_tbl[cur_inode.indirectPointer];
    }
    read_blocks(cur_inode.data_ptrs[cur_block % 12], 1, read_temp);
    read_len = length > BLOCK_SIZE ? BLOCK_SIZE : length;
    memcpy(buf + cur_read_len, read_temp, read_len);
    cur_read_len += read_len;
    file_descriptors[fileID].rwptr += read_len;
    length -= read_len;
    cur_block++;
  }
  return new_len;
}

int sfs_fwrite(int fileID, const char *buf, int length) {
  // Check if the file is opened or  the write length is valid
  int inode_num = file_descriptors[fileID].inodeIndex;
  int file_opened = 0;
  int i;
  for(i = 0; i < NUM_OF_FILES; i++){
    int num = directory_entry_tbl[i].num;
    if (num == -1)
      break;
    if (num == inode_num){
      file_opened = 1;
      break;
    }
  }
  if (file_opened == 0){
    printf("Required file is not opened\n");
    return -1;
  }
  if (length <= 0){
    printf("Length empty\n");
    return -1;
  }
  // write to the file
  int cur_len = length, num = 0, len_written = 0;
  int cur_ptr = file_descriptors[fileID].rwptr;
  int cur_block = cur_ptr / BLOCK_SIZE;
  int link = cur_block / 12;
  char write_temp[BLOCK_SIZE];
  inode_t* cur_inode = file_descriptors[fileID].inode;
  while (link != 0) {
    cur_inode = &inode_tbl[cur_inode->indirectPointer];
    link --;
  }
  if (cur_inode->data_ptrs[cur_block % 12] == -1){
    num = get_index();
    if (num > 1022)return 0;
    cur_inode->data_ptrs[cur_block % 12] = num;
  }
  read_blocks(cur_inode->data_ptrs[cur_block % 12], 1, write_temp);
  int write_len = cur_len <= BLOCK_SIZE - file_descriptors[fileID].rwptr % BLOCK_SIZE ? cur_len : BLOCK_SIZE - file_descriptors[fileID].rwptr % BLOCK_SIZE;
  memcpy(write_temp + file_descriptors[fileID].rwptr % BLOCK_SIZE, buf + len_written, write_len);
  write_blocks(cur_inode->data_ptrs[cur_block % 12], 1, write_temp);
  cur_block ++;
  len_written += write_len;
  cur_len -= write_len;
  file_descriptors[fileID].rwptr += write_len;

  while (cur_len > 0) {
    if (cur_block % 12 == 0) {
        // create new link when 12 blocks are filled and indirect in not created
      if(cur_inode->indirectPointer == -1){
        for (int i = 0; i < NUM_INODES; i++){
          if (inode_tbl[i].size == -1){
            inode_tbl[i].size = 0;
            cur_inode->indirectPointer = i;
            break;
          }
        }
        // navigate to next inode
        cur_inode = &inode_tbl[cur_inode->indirectPointer];
      }
    }
    // assign new block
    if (cur_inode->data_ptrs[cur_block % 12] == -1){
      num = get_index();
      if (num > 1022)return 0;
      cur_inode->data_ptrs[cur_block % 12] = num;
    }


    int write_len = cur_len > BLOCK_SIZE ? BLOCK_SIZE : cur_len;
    memset(write_temp, 0, BLOCK_SIZE);
    memcpy(write_temp, buf + len_written, write_len);
    write_blocks(cur_inode->data_ptrs[cur_block % 12], 1, write_temp);
    cur_block ++;
    len_written += write_len;
    cur_len -= write_len;
    file_descriptors[fileID].rwptr += write_len;
  }
  file_descriptors[fileID].inode->size = file_descriptors[fileID].rwptr > file_descriptors[fileID].inode->size ? file_descriptors[fileID].rwptr : file_descriptors[fileID].inode->size;
  return len_written;
}

int sfs_fseek(int fileID, int loc) {
  if (fileID < 0 || file_descriptors[fileID].inodeIndex == -1 || loc < 0) {
    printf("Error : Invalid input!\n");
    return -1;
  }
  if (loc > file_descriptors[fileID].inode->size) {
    printf("Error : New location larger than file size\n");
    return -1;
  }else{
    file_descriptors[fileID].rwptr = loc;
  }
  return 0;
}

int sfs_remove(char *file) {


}
