# Simple File System

A simple file using indexed allocation which stores files in a virtual disk. An application that would like to create and use files will be linked with the library.

### Run the program with

```
$ ./create_format <FILENAME> <SIZE>
$ ./app <FILENAME>
```
### How to Use

• int create_format_vdisk (char *vdiskname, int m): This function is used to create and format a virtual disk. The virtual disk will simply be a Linux file of certain size. The name of the Linux file is vdiskname. The parameter m is used to set the size of the file. Size is 2^m bytes. If success, 0 will be returned. If error, -1 will be returned.
• int sfs_mount (char *vdiskname): This function is used to mount the file system, i.e., to prepare the file system to be used. If success, 0 will be returned; if error, -1 will be returned.
• int sfs_umount (). This function is used to unmount the file system: flush the cached data to disk and close the virtual disk (Linux file) file descriptor. It is a simple to implement function. If success, 0 will be returned, if error, -1 will be returned.
