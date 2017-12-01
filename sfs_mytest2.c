#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"
#define MAX_FNAME_LENGTH 20   /* Assume at most 20 characters (16.3) */

/* The maximum number of files to attempt to open or create.  NOTE: we
 * do not _require_ that you support this many files. This is just to
 * test the behavior of your code.
 */
#define MAX_FD 64

/* The maximum number of bytes we'll try to write to a file. If you
 * support much shorter or larger files for some reason, feel free to
 * reduce this value.
 */
#define MAX_BYTES 30000 /* Maximum file size I'll try to create */
#define MIN_BYTES 10000
static char test_str[] = "The quick brown fox jumps over the lazy dog.\n";
char *rand_name()
{
  char fname[MAX_FNAME_LENGTH];
  int i;

  for (i = 0; i < MAX_FNAME_LENGTH; i++) {
    if (i != 16) {
      fname[i] = 'A' + (rand() % 26);
    }
    else {
      fname[i] = '.';
    }
  }
  fname[i] = '\0';
  return (strdup(fname));
}
int main(int argc, char const *argv[]) {
  int i, j, k;
  int chunksize;
  int readsize;
  char *buffer;
  char fixedbuf[1024];
  int fds[MAX_FD];
  char *names[MAX_FD];
  int filesize[MAX_FD];
  int nopen;                    /* Number of files simultaneously open */
  int ncreate;                  /* Number of files created in directory */
  int error_count = 0;
  int tmp;

  mksfs(1);
  ncreate = 0;
  for (i = 0; i < MAX_FD; i++) {
    names[i] = rand_name();
    fds[i] = sfs_fopen(names[i]);
    if (fds[i] < 0) {
      break;
    }
    sfs_fclose(fds[i]);
    ncreate++;
  }

  printf("Created %d files in the root directory\n", ncreate);

  nopen = 0;
  for (i = 0; i < ncreate; i++) {
    fds[i] = sfs_fopen(names[i]);
    printf("fd : %d\n", fds[i]);
    if (fds[i] < 0) {
      break;
    }
    nopen++;
  }
  printf("Simultaneously opened %d files\n", nopen);

  for (i = 0; i < nopen; i++) {
    tmp = sfs_fwrite(fds[i], test_str, strlen(test_str));
    if (tmp != strlen(test_str)) {
      fprintf(stderr, "ERROR: Tried to write %d, returned %d\n",
              (int)strlen(test_str), tmp);
      error_count++;
    }
    if (sfs_fclose(fds[i]) != 0) {
      fprintf(stderr, "ERROR: close of handle %d failed\n", fds[i]);
      error_count++;
    }
  }
  printf("error: %d\n", error_count);
}
