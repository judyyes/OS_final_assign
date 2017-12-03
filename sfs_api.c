
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include <limits.h>
#include <libgen.h>
#include "disk_emu.h"
#define ZHU_DISHI_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows.
#define BLOCK_SIZE 1024
#define NUM_INODES 200
#define NUM_OF_FILES 128


superblock_t super_block;
file_descriptor file_descriptors[NUM_OF_FILES];
directory_entry directory_entry_tbl[NUM_OF_FILES];
inode_t inode_tbl[NUM_INODES];
int cur_file_index = 0;
int inode_blocks = sizeof(inode_t) * NUM_INODES / BLOCK_SIZE + 1; // 15
int dir_block_num = sizeof(directory_entry) * NUM_OF_FILES / BLOCK_SIZE + 1; // 4

//initialize super block
void init_super_block(){
    super_block.magic = 0xACBD0005;
    super_block.block_size = BLOCK_SIZE;
    super_block.fs_size = BLOCK_SIZE * NUM_BLOCKS;
    super_block.inode_table_len = NUM_INODES;
    super_block.root_dir_inode = 0;
    inode_tbl[0].size = 0;
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

void update_disk(){
    write_blocks(1, inode_blocks, &inode_tbl);
    write_blocks(inode_blocks + 1, dir_block_num, &directory_entry_tbl);
    write_blocks(1023, 1, free_bit_map);
}

void mksfs(int fresh) {
    if (fresh == 0){ // the flag is false, open from disk
        init_disk(ZHU_DISHI_DISK, BLOCK_SIZE, NUM_BLOCKS);
        // read super block from disk
        read_blocks(0, 1, &super_block);

        // read inode table
        read_blocks(1, inode_blocks, &inode_tbl);

        // read directory entry table
        read_blocks(inode_blocks+1, dir_block_num, &directory_entry_tbl);

        // read bit map table
        read_blocks(1023, 1, free_bit_map);

        init_file_descriptor();
    }
    else{ // the flag is true, create from scratch
        init_inode_table();
        init_directory_entry();
        init_file_descriptor();
        init_super_block();

        init_fresh_disk(ZHU_DISHI_DISK, BLOCK_SIZE, NUM_BLOCKS);

        // write super block on disk
        force_set_index(0);
        write_blocks(0, 1, &super_block);

        // write inode table on disk
        for (int i = 0; i < inode_blocks; i++) {
            get_index();
        }
        write_blocks(1, inode_blocks, &inode_tbl);

        //write rootDir on disk
        inode_tbl[0].size = sizeof(directory_entry) * NUM_OF_FILES;
        for (int i = 0; i < dir_block_num; i ++){
            int num = get_index();
            inode_tbl[0].data_ptrs[i] = num;
        }
        write_blocks(inode_blocks + 1, dir_block_num, &directory_entry_tbl);

        // write bit map in sfs
        force_set_index(1023);
        write_blocks(1023, 1, free_bit_map);
    }
}

/* Copies the name of the next file in the directory into fname and returns non zero if there
is a new file. Once all the files have been returned, this function returns 0. So, you should
be able to use this function to loop through the directory. */
int sfs_getnextfilename(char *fname){
    int found_file = 0;
    for (int i = cur_file_index; i < NUM_OF_FILES; i++) {
        if (directory_entry_tbl[i].name[0] != '\0') {
            int j;
            for (j = 0; j < MAX_FILE_NAME; j++){
                fname[j] = directory_entry_tbl[i].name[j];
            }
            fname[j] = '\0';

            found_file = 1;
            cur_file_index++;
            return 1;
        }
    }
    cur_file_index = found_file == 1 ? cur_file_index : 0;
    return 0;
}

/* Returns the size of a given file */
int sfs_getfilesize(const char* path){
    int file_size = 0;
    for (int i = 0; i < NUM_OF_FILES; i++){
        if (compare_string(directory_entry_tbl[i].name, basename((char*)path))){
            return inode_tbl[directory_entry_tbl[i].num].size;
        }
    }
    return file_size;
}
int sfs_fopen(char *name){
  if (strlen(name) > MAX_FILE_NAME){
    printf("File name to long\n");
    return -1;
  }
  // return the fd num if the file exists; fd represents the files that are open
  int i, j;
  for (i = 0; i < NUM_OF_FILES; i++) {
    if (compare_string(name, directory_entry_tbl[i].name)){
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
      if (num >= 1022)return 0;
      inode_tbl[i].data_ptrs[0] = num;
      inode_tbl[i].size = 0;
      createdInode = 1;

      for (j = 0; j < NUM_OF_FILES; j++){
        if (directory_entry_tbl[j].num == -1){
          directory_entry_tbl[j].num = i;
            for (int k = 0; k < MAX_FILE_NAME; k ++){
                directory_entry_tbl[j].name[k] = name[k];
            }
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
    update_disk();
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
    if (num >= 1022)return 0;
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
      if (num >= 1022)return 0;
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
  update_disk();
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
    // Iterate through all the files and find the one with match file name
    for (int i =0; i < NUM_OF_FILES; i++){
        if (compare_string(directory_entry_tbl[i].name, file)) {
            // remove file name
            for (int j = 0; j < MAX_FILE_NAME; j++){
                directory_entry_tbl[i].name[j] = '\0';
            }
            // remove in inode table
            inode_t* cur_node = &inode_tbl[directory_entry_tbl[i].num];

            while (cur_node->indirectPointer != -1) {
                inode_t* nxt_node = &inode_tbl[cur_node->indirectPointer];
                cur_node->indirectPointer = -1;
                cur_node->size = -1;
                for (int j = 0; j < 12; j ++) {
                    if (cur_node->data_ptrs[j] != -1) {
                        rm_index(cur_node->data_ptrs[j]);
                        cur_node->data_ptrs[j] = -1;
                    }
                }
                cur_node = nxt_node;
            }
            cur_node->indirectPointer = -1;
            cur_node->size = -1;
            for (int j = 0; j < 12; j ++) {
                if (cur_node->data_ptrs[j] != -1) {
                    rm_index(cur_node->data_ptrs[j]);
                    cur_node->data_ptrs[j] = -1;
                }
            }
            // remove file descriptors
            for (int j = 0; j < MAX_FILE_NAME; j++){
                if (directory_entry_tbl[i].num == file_descriptors[j].inodeIndex){
                    file_descriptors[j].inodeIndex = -1;
                    file_descriptors[j].rwptr = 0;
                    file_descriptors[j].inode = NULL;
                }
            }

            //remove file num in directory table
            directory_entry_tbl[i].num = -1;

        }
    }
    update_disk();
    return 0;

}
// return 1 when equal
int compare_string(char *str1, char *str2){
    for(int i = 0; i < MAX_FILE_NAME; i++){
        if (str1[i] != str2[i]){
            return 0;
        }
    }
    return 1;
}