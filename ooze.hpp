#include <map>
#include <string>
#include <vector>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <unistd.h>

// Ooze is a fuse filesystem.
// It supports a fixed number of files with a fixed maximum file size. Creating
// new files after the maximum number of files has been reached will overwrite
// the oldest files.
// It does not support subdirectories.


struct ooze_file
{
    static std::vector<char>::size_type max_size;

    std::string name;
    uint64_t    handle;

    // owner
    uid_t       uid;
    gid_t       gid;

    // file mode
    mode_t      mode;

    // with nanosecond resolution
    struct timespec atime;
    struct timespec mtime;

    std::vector<char> data;

    bool unlinked;

    ooze_file()
    {
        data.reserve(max_size);
    }
};

void swap(ooze_file& x, ooze_file& y);


class ooze
{
    uint64_t filecounter_;

    std::vector<ooze_file> files_;

    // path to file lookup
    std::map<std::string, ooze_file*> pathlookup_;

    // handle to unlinked file lookup
    std::map<uint64_t, ooze_file*> unlinked_;

    static ooze* self_;

public:

    ooze(std::size_t max_num_files, std::size_t max_file_size);

    ooze_file& get_file(uint64_t handle)
    {
        return files_[handle % files_.size()];
    }

    const ooze_file& get_file(uint64_t handle) const
    {
        return files_[handle % files_.size()];
    }

    static int getattr(const char* path, struct stat*);
    static int chmod(const char* path, mode_t);
    static int chown(const char* path, uid_t, gid_t);
    static int create(const char* path, mode_t m, struct fuse_file_info* fi);
    static int open(const char* path, struct fuse_file_info*);
    static int unlink(const char* path);
    static int read(const char*, char*, size_t, off_t, struct fuse_file_info*);
    static int write(const char*, const char*, size_t, off_t, struct fuse_file_info*);
    static int utimens(const char *, const struct timespec tv[2]);
    static int truncate(const char* path, off_t size);
    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off,
                       struct fuse_file_info *fi);
};

