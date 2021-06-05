# Simple File System

A simple file system using indexed allocation which stores files in a virtual disk. 
An application that would like to create and use files will be linked with the library.

#### Run the program with:

```
$ ./create_format <FILENAME> <SIZE>
$ ./app <FILENAME>
```
#### How to use:

* **`int create_format_vdisk (char *vdiskname, int m):`** This function is used to create and format a virtual disk. The virtual disk will simply be a Linux file of certain size. The name of the Linux file is vdiskname. The parameter m is used to set the size of the file. Size is 2^m bytes. If success, 0 will be returned. If error, -1 will be returned.
* **`int sfs_mount (char *vdiskname):`**  This function is used to mount the file system, i.e., to prepare the file system to be used. If success, 0 will be returned; if error, -1 will be returned.
* **`int sfs_umount ():`** This function is used to unmount the file system: flush the cached data to disk and close the virtual disk (Linux file) file descriptor. It is a simple to implement function. If success, 0 will be returned, if error, -1 will be returned.
* **`int sfs_create (char *filename):`** With this, an applicaton can create a new file with name filename. If success, 0 is returned. If error, -1 is returned.
* **`int sfs_open (char *filename, int mode):`** With this function an application can open a file. The name of the file to open is filename. The mode paramater specifies if the file is opened in read-only mode or in append-only mode. If error, -1 is returned else the unique file descriptor (fd) for the file is returned. Mode is either MODE_READ or MODE_APPEND.
* **`int sfs_getsize (int fd):`** With this an application learns the size of a file whose descriptor is fd. The file must be opened first. Returns the number of data bytes in the file. A file with no data in it (no content) has size 0. If error, returns -1.
* **`int sfs_close (int fd):`** With this function an application can close a file whose descriptor is fd. 
* **`int sfs_read (int fd, void *buf, int n):`** With this, an application can read data from a file. fd is the file descriptor. buf is pointing to a memory area for which space is allocated earlier with malloc. n is the number of bytes of data to read. Upon failure, -1 is returned. Otherwise, number of bytes sucessfully read is returned.
* **`int sfs_append (int fd, void *buf, int n):`** With this, an application can append new data to the file. The parameter fd is the file descriptor. The parameter buf is pointing to (i.e., is the address of) a dynamically allocated memory space holding the data. The parameter n is the size of the data to write (append) into the file. If error, -1 is returned. Otherwise, the number of bytes successfully appended is returned.
* **`int sfs_delete (char *filename):`** With this, an application can delete a file. The name of the file to be deleted is filename. If succesful, 0 is returned. In case of an error, -1 is returned.
