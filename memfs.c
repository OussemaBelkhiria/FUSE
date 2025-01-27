#include "memfs.h"
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#define LOG "journal.txt"
static int initialization = 0;
static Tree *root;
static CurrentTree *curr;
static FILE *log;
static FILE *status;
int search_tree(char *path)
{
    if (strcmp(path, "/") == 0)
    {
        curr->parent = NULL;
        curr->current_directory = root;
        curr->filename = root->filename;
        return 0;
    }
    char *token;
    path++;
    token = strtok(path, "/");
    char *filename;
    while (token != NULL)
    {
        filename = strdup(token);
        bool found = false;
        TreesList *current_child = curr->current_directory->children;
        if (current_child == NULL)
        {
            curr->parent = curr->current_directory;
            curr->filename = strdup(filename);
            return -ENOENT;
        }
        curr->parent = curr->current_directory;
        while (current_child->next != NULL)
        {
            if (strcmp(token, current_child->child->filename) == 0)
            {
                curr->current_directory = current_child->child;
                found = true;
                break;
            }

            current_child = current_child->next;
        }
        if (!found)
        {
            curr->filename = strdup(filename);
            return -ENOENT;
        }

        token = strtok(NULL, "/");
    }
    curr->filename = strdup(filename);
    return 0;
};
int add_file(mode_t mode, int filetype)
{
    // check if the filename is bigger than 255;

    if (strlen(curr->filename) > 255)
    {
        // FILENAME TOO LONG
        errno = ENAMETOOLONG;
        return -1;
    }

    struct Tree *added_file = malloc(sizeof(struct Tree));
    // folder : the given mode must be combined with __S_IFDIR : default is 0755
    if (filetype == 0)
    {
        added_file->mode = __S_IFDIR | mode;
        added_file->nlink = 2;
    }
    // regularfile : the given mode must not be combined with __S_IFREG ; default mode is 10644
    else if (filetype == 1)
    {
        added_file->mode = mode;
        added_file->nlink = 1;
        added_file->size = 0;
        added_file->content = calloc(512, 1);
    }
    // symbolic link
    else
    {
        added_file->mode = __S_IFLNK | 0777;
        added_file->nlink = 1;
    }
    added_file->filename = strdup(curr->filename);

    if (curr->parent->children == NULL)
    {
        curr->parent->children = malloc(sizeof(struct TreesList));
    }
    struct TreesList *new_children = malloc(sizeof(struct TreesList));
    new_children->child = malloc(sizeof(struct Tree));
    new_children->child = added_file;
    new_children->next = curr->parent->children;
    curr->parent->children = new_children;
    curr->current_directory = added_file;
    return 0;
}
static int mygetattr(const char *path, struct stat *stats)
{
    stats->st_atime;
    stats->st_mtime;
    curr->current_directory = root;
    curr->parent = NULL;
    if (strcmp(path, "/") == 0)
    {
        stats->st_mode = curr->current_directory->mode;
        stats->st_nlink = curr->current_directory->nlink;
        stats->st_size = curr->current_directory->size;
        stats->st_atime = curr->current_directory->atime;
        stats->st_mtime = curr->current_directory->mtime;
        return 0;
    }
    char *p = strdup(path);
    int rv = search_tree(p);
    free(p);
    // file not found;
    if (rv < 0)
        return rv;

    stats->st_mode = curr->current_directory->mode;
    stats->st_nlink = curr->current_directory->nlink;
    stats->st_size = curr->current_directory->size;
    stats->st_size = curr->current_directory->size;
    stats->st_atime = curr->current_directory->atime;
    stats->st_mtime = curr->current_directory->mtime;
    return 0;
}
static int mymkdir(const char *path, mode_t mode)
{

    if (initialization == 0)
    {
        if (fprintf(log, "%s %s %o\n", "mkdir", path, mode) < 0)
        {
            perror("failed to log the mkdir operation");
            return -1;
        }
        fflush(log);
    }
    return add_file(mode, 0);
}
static int myreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    struct TreesList *ch = curr->current_directory->children;
    if (ch != NULL)
    {
        while (ch->next != NULL)
        {
            filler(buf, ch->child->filename, NULL, 0);
            ch = ch->next;
        }
    }
    return 0;
}
static int mymknod(const char *path, mode_t mode, dev_t dev)
{

    if (initialization == 0)
    {
        if (fprintf(log, "%s %s %o\n", "mknod", path, mode) < 0)
        {
            perror("failed to log the mknod operation");
            return -1;
        }

        fflush(log);
    }

    return add_file(mode, 1);
};
static int myopen(const char *path, struct fuse_file_info *fi)
{
    curr->current_directory = root;
    curr->parent = NULL;
    char *path_cpy = strdup(path);
    return search_tree(path_cpy);
}
static int mycreate(const char *path, mode_t mode, struct fuse_file_info *fi)
{

    if (initialization == 0)
    {
        if (fprintf(log, "%s %s %o\n", "create", path, mode) < 0)
        {
            perror("failed to log the create operation");
            return -1;
        }

        fflush(log);
    }

    return add_file(mode, 1);
}
static int myread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if (offset > curr->current_directory->size)
    {
        return 0;
    }
    // content size > offset : move the cursor to offset
    char *read_point = (curr->current_directory->content) + offset;
    // remaining size
    size_t max_readable_bytes = curr->current_directory->size - offset;
    // if size is bigger than possible max readablebytes we limit ourselves to the max_readablebytes : this detail is implementation defined
    if (size > max_readable_bytes)
    {
        size = max_readable_bytes;
    }
    memcpy(buf, read_point, size);
    return size;
}
static int mywrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if (initialization == 0)
    {
        if (fprintf(log, "%s %s %lu %ld %s\n", "write", path, size, offset, buf) < 0)
        {
            perror("failed to log the write operation");
            return -1;
        }
        fflush(log);
    }
    if (offset > curr->current_directory->size)
    {
        return 0;
    }
    char *write_point = curr->current_directory->content + offset;
    size_t max_writable_bytes = 512 - offset;
    if (size > max_writable_bytes)
    {
        size = max_writable_bytes;
    }
    if (memcpy(write_point, buf, size) < 0)
        return -1;
    curr->current_directory->size += size;
    return size;
}
static int mysimlink(const char *target, const char *linkpath)
{
    if (initialization == 0)
    {

        if (fprintf(log, "%s %s %s \n", "symlink", target, linkpath) < 0)
        {
            perror("failed to log the symlink operation");
            return -1;
        }

        fflush(log);
    }
    if (strlen(target) > 255)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    int rv = add_file(0777, 2);
    if (rv < 0)
        return rv;
    curr->current_directory->content = strdup(target);
    curr->current_directory->size = strlen(target);
    return 0;
}
static int myreadlink(const char *linkpath, char *buf, size_t size)
{
    curr->current_directory = root;
    curr->parent = NULL;
    char *linkpath_cpy = strdup(linkpath);
    search_tree(linkpath_cpy);
    free(linkpath_cpy);
    if (size > curr->current_directory->size)
    {
        memcpy(buf, curr->current_directory->content, curr->current_directory->size);
    }
    else
    {
        memcpy(buf, curr->current_directory->content, size);
    }
    return 0;
}
static void *myinit(struct fuse_conn_info *inf)
{
    // we loop through the jorunals , if we find the journal without return value we replay it : try to modify the path name to actual path name
    struct stat *stats = malloc(sizeof(stat));
    root = malloc(sizeof(struct Tree));
    root->filename = "/";
    root->mode = __S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR |
                 S_IRGRP | S_IWGRP | S_IXGRP |
                 S_IROTH | S_IWOTH | S_IXOTH;
    root->nlink = 2;
    root->size = 0;
    root->children = NULL;

    curr = malloc(sizeof(struct CurrentTree));
    curr->parent = NULL;
    curr->current_directory = root;
    curr->filename = root->filename;

    // read the log :
    initialization = 1;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, log)) != -1)
    {
        char *commands = strtok(line, " ");
        char *command_name;
        const char *path;
        const char *buf;
        mode_t mode;
        size_t size;
        off_t offset;
        int i = 0;
        while (commands != NULL)
        {
            if (i == 0)
            {
                command_name = strdup(commands);
                if (strcmp(command_name, "write") == 0)
                    break;
                if (strcmp(command_name, "symlink") == 0)
                    break;
            }
            else if (i == 1)
            {
                path = strdup(commands);
            }
            else
            {
                mode = strtol(commands, NULL, 8);
            }
            commands = strtok(NULL, " ");
            i++;
        }
        // reexecuting commands
        if (strcmp(command_name, "mkdir") == 0)
        {
            mygetattr(path, stats);
            mymkdir(path, mode);
        }
        else if (strcmp(command_name, "mknod") == 0)
        {
            mygetattr(path, stats);
            mymknod(path, mode, 0);
        }
        else if (strcmp(command_name, "create") == 0)
        {
            mygetattr(path, stats);
            mycreate(path, mode, NULL);
        }
        else if (strcmp(command_name, "write") == 0)
        {
            commands = strtok(NULL, " ");
            int i = 1;
            while (commands != NULL)
            {
                if (i == 1)
                {
                    path = strdup(commands);
                }
                else if (i == 4)
                {
                    buf = strdup(commands);
                }
                else if (i == 2)
                {
                    size = atoi(commands);
                }
                else if (i == 3)
                {
                    offset = atoi(commands);
                }
                commands = strtok(NULL, " ");
                i++;
            }
            mygetattr(path, stats);
            mywrite(path, buf, size, offset, NULL);
        }
        else if (strcmp(command_name, "symlink") == 0)
        {
            commands = strtok(NULL, " ");
            int i = 1;
            while (commands != NULL)
            {
                if (i == 1)
                {
                    path = strdup(commands);
                }
                else if (i == 2)
                {
                    buf = strdup(commands);
                }
                commands = strtok(NULL, " ");
                i++;
            }

            mygetattr(buf, stats);
            mysimlink(path, buf);
        }
    }

    initialization = 0;
    return NULL;
}
static void mydestroy(void *)
{
    // for each successful unmount there is a call of the destroy function : in this case we empty the content of the file :
    fclose(log);
    // write in the file u
    fputc('u', status);
    fclose(status);
}
static struct fuse_operations op = {
    .init = myinit,
    .getattr = mygetattr,
    .mkdir = mymkdir,
    .readdir = myreaddir,
    .mknod = mymknod,
    .open = myopen,
    .create = mycreate,
    .read = myread,
    .write = mywrite,
    .symlink = mysimlink,
    .readlink = myreadlink,
    .destroy = mydestroy};
int main(int argc, char **argv)
{
    // check in the file status if the first char is u : if so remove u and write m for example
    status = fopen("status.txt", "a+");
    char c = fgetc(status);
    // if c == EOF we don't remove the journal
    if (c == 'u')
    {
        remove(LOG);
        int fd = fileno(status);
        // Truncate the status file to zero length (delete its contents)
        if (ftruncate(fd, 0) < 0)
            return -1;
    }
    // open the journal and write ops
    log = fopen(LOG, "a+");
    // the question why when i use -f : i can remove the file , or i can truncate it's length to 0 , while when i run without -f i still can write only : the file is not removed
    return fuse_main(argc, argv, &op, NULL);
}