#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SOURCE_DIR   "/home/bagas/reports"
#define PREFIX       "LAPORAN_"
#define PREFIX_LEN   8
#define ALLOWED_EXT  ".txt"

static int is_txt_file(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return strcmp(dot, ALLOWED_EXT) == 0;
}

// buang prefix LAPORAN_ dari path FUSE → balikin path asli di source dir
// "/LAPORAN_laporan_jan.txt"  ->  "/home/bagas/reports/laporan_jan.txt"
static int fuse_path_to_real(const char *fuse_path, char *real_path, size_t size) {
    if (strcmp(fuse_path, "/") == 0) {
        snprintf(real_path, size, "%s", SOURCE_DIR);
        return 0;
    }

    const char *name = fuse_path + 1;   // skip "/"
    if (strncmp(name, PREFIX, PREFIX_LEN) != 0)
        return -1;

    snprintf(real_path, size, "%s/%s", SOURCE_DIR, name + PREFIX_LEN);
    return 0;
}

static int mfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode  = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    if (lstat(real_path, stbuf) == -1)
        return -errno;

    // matiin bit write biar bener-bener read-only di mata sistem
    stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    return 0;
}

static int mfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;

    // cuma support 1 level (root), no subdir
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    DIR *dp = opendir(SOURCE_DIR);
    if (!dp) return -errno;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (!is_txt_file(de->d_name))
            continue;

        char display[1024];
        snprintf(display, sizeof(display), "%s%s", PREFIX, de->d_name);
        if (filler(buf, display, NULL, 0)) break;
    }

    closedir(dp);
    return 0;
}

static int mfs_open(const char *path, struct fuse_file_info *fi) {
    // tolak semua mode selain read-only
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    // test buka dulu, biar error file asli ke-propagate ke user
    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int mfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

// semua op write ditolak — read-only filesystem
static int mfs_write(const char *p, const char *b, size_t s, off_t o, struct fuse_file_info *fi) {
    (void) p; (void) b; (void) s; (void) o; (void) fi;
    return -EROFS;
}
static int mfs_create(const char *p, mode_t m, struct fuse_file_info *fi) {
    (void) p; (void) m; (void) fi;
    return -EROFS;
}
static int mfs_mkdir(const char *p, mode_t m)        { (void) p; (void) m; return -EROFS; }
static int mfs_unlink(const char *p)                 { (void) p; return -EROFS; }
static int mfs_rmdir(const char *p)                  { (void) p; return -EROFS; }
static int mfs_rename(const char *f, const char *t)  { (void) f; (void) t; return -EROFS; }
static int mfs_truncate(const char *p, off_t s)      { (void) p; (void) s; return -EROFS; }

static struct fuse_operations mfs_oper = {
    .getattr  = mfs_getattr,
    .readdir  = mfs_readdir,
    .open     = mfs_open,
    .read     = mfs_read,
    .write    = mfs_write,
    .create   = mfs_create,
    .mkdir    = mfs_mkdir,
    .unlink   = mfs_unlink,
    .rmdir    = mfs_rmdir,
    .rename   = mfs_rename,
    .truncate = mfs_truncate,
};

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &mfs_oper, NULL);
}