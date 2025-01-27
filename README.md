# Implement file system operations using FUSE 
[FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) allows one to create their own file system without editing kernel code.The [high level API](https://libfuse.github.io/doxygen/fuse_8h.html) is used in this implementation. As constraints we have : 
- Maximum file name length of 255 ascii characters.
- Maximum file size of 512 bytes.

Furthermore, a simple journaling mechansim that works as a backup in case of an interruption is implemented. The goal is to provide crash-consistency. 