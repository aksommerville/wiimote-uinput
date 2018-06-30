#include "wiimote.h"
#include "wm_fs.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Read entire file.
 */

int wm_file_read(void *dstpp,const char *path,int require) {
  if (!dstpp||!path) return -1;

  int fd=open(path,O_RDONLY);
  if (fd<0) {
    if (!require&&(errno==ENOENT)) return 0;
    return -1;
  }

  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }

  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }

  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<0) {
      close(fd);
      free(dst);
      return -1;
    }
    if (!err) break;
    dstc+=err;
  }

  *(void**)dstpp=dst;
  close(fd);
  return dstc;
}
