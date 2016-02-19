#include "ooze.hpp"

#include <cstddef> // offsetof
#include <cstring>
#include <ctime>
#include <iostream>

//#include <fstream>
//std::ofstream log;


std::vector<char>::size_type ooze_file::max_size;

void swap(ooze_file& x, ooze_file& y)
{
    using std::swap;
    swap(x.name, y.name);
    swap(x.handle, y.handle);
    swap(x.uid, y.uid);
    swap(x.gid, y.gid);
    swap(x.mode, y.mode);
    swap(x.data, y.data);
}

ooze* ooze::self_ = nullptr;

ooze::ooze(std::size_t max_num_files, std::size_t max_file_size)
  :
    filecounter_(0)
{
    files_.reserve(max_num_files);
    ooze_file::max_size = max_file_size;

    if (self_ == nullptr)
        self_ = this;
}

int ooze::getattr(const char* path, struct stat* buf)
{
    buf->st_uid = getuid();
    buf->st_gid = getgid();

    if (strcmp(path, "/") == 0)
    {
        buf->st_mode = S_IFDIR | 0755;
        buf->st_nlink = 2;
        buf->st_atime = buf->st_mtime = std::time(nullptr);
        return 0;
    }
    else
    {
        auto it = self_->pathlookup_.find(&path[1]);
        if (it != self_->pathlookup_.end() && !it->second->unlinked)
        {
            buf->st_uid  = it->second->uid;
            buf->st_gid  = it->second->gid;
            buf->st_mode = it->second->mode;
            buf->st_size = it->second->data.size();
            buf->st_nlink = 1;
            buf->st_mtim = it->second->mtime;
            buf->st_atim = it->second->atime;
            return 0;
        }
    }

    return -ENOENT;
}

int ooze::create(const char* path, mode_t m, struct fuse_file_info* fi)
{
    //log << "create " << path << std::endl;
    // check if path already exists
    auto it = self_->pathlookup_.find(path);
    if (it != self_->pathlookup_.end())
    {
        // should never happen according to docs
        //log << "path already exists" << std::endl;
        fi->fh = it->second->handle;
        it->second->mode = m;
    }

    ooze_file* fileptr;
    if (!self_->unlinked_.empty())
    {
        // check if there is an unlinked file that we can reuse
        auto it = self_->unlinked_.begin();
        fileptr = it->second;
        self_->unlinked_.erase(it);
    }
    else
    {
        // if not try to create a new file
        if (self_->files_.size() < self_->files_.capacity())
            self_->files_.push_back(ooze_file());

        fileptr = &self_->get_file(self_->filecounter_);
    }

    const struct fuse_context* ctx = fuse_get_context();

    fileptr->handle = self_->filecounter_;
    fileptr->uid = ctx->uid;
    fileptr->gid = ctx->gid;
    fileptr->mode = m;
    fileptr->data.clear();
    fileptr->unlinked = false;

    fileptr->name = &path[1]; // skip leading / character
    self_->pathlookup_[fileptr->name] = fileptr;
    fi->fh = fileptr->handle;

    clock_gettime(CLOCK_REALTIME, &fileptr->mtime);
    fileptr->atime = fileptr->mtime;

    ++self_->filecounter_;

    return 0;
}

int ooze::open(const char* path, struct fuse_file_info* fi)
{
    auto it = self_->pathlookup_.find(&path[1]);
    if (it != self_->pathlookup_.end())
    {
        fi->fh = it->second->handle;
        return 0;
    }

    return -ENOENT;
}

int ooze::unlink(const char* path)
{
    auto it = self_->pathlookup_.find(&path[1]);
    if (it != self_->pathlookup_.end())
    {
        auto& file = *(it->second);
        self_->pathlookup_.erase(it);
        file.unlinked = true;

        self_->unlinked_[file.handle] = &file;

        return 0;
    }

    return -ENOENT;
}

int ooze::read(const char* path, char* buf, size_t count, off_t pos, struct fuse_file_info* fi)
{
    if (pos < 0 || pos > ooze_file::max_size)
        return -1;

    auto& file = self_->get_file(fi->fh);
    if (file.handle != fi->fh)
        return -EBADF;

    count = std::min(count, file.data.size() - pos);
    std::copy(file.data.cbegin() + pos, file.data.cbegin() + pos + count, buf);

    return count;
}

int ooze::write(const char* path, const char* buf, size_t count, off_t pos, struct fuse_file_info* fi)
{
    if (pos < 0)
        return -1;

    if (pos > ooze_file::max_size || pos + count > ooze_file::max_size)
        return -EFBIG;

    auto& file = self_->get_file(fi->fh);
    if (file.handle != fi->fh)
        return -EBADF;

    size_t copycount = 0;
    if (pos < file.data.size())
        copycount = std::min(count, file.data.size() - pos);

    size_t pushcount = count - copycount;
    if (copycount)
        std::copy(buf, buf + copycount, file.data.begin() + pos);
    if (pushcount)
        std::copy(buf + copycount, buf + count, std::back_inserter(file.data));

    return count;
}

int ooze::truncate(const char* path, off_t size)
{
    if (size > ooze_file::max_size)
        return -EFBIG;

    auto it = self_->pathlookup_.find(&path[1]);
    if (it != self_->pathlookup_.end())
    {
        it->second->data.resize(size);
        return 0;
    }

    return -ENOENT;
}

int ooze::utimens(const char* path, const struct timespec tv[2])
{
    return 0;
}

int ooze::readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off,
                    struct fuse_file_info *fi)
{
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);
    for (; off < self_->files_.size(); ++off)
    {
        auto& file = self_->files_[off];
        if (!file.unlinked)
            filler(buf, file.name.c_str(), nullptr, 0);
    }

    return 0;
}

int ooze::chmod(const char* path, mode_t m)
{
    auto it = self_->pathlookup_.find(&path[1]);
    if (it != self_->pathlookup_.end())
    {
        it->second->mode = m;
        return 0;
    }

  return -ENOENT;
}

int ooze::chown(const char* path, uid_t uid, gid_t gid)
{
    if (strcmp(path, "/") == 0)
    {
        // Cannot change owner of mountpoint from within ooze.
        // Instead mount with the options "-o uid=x,gid=y"
        return 0;
    }

    auto it = self_->pathlookup_.find(&path[1]);
    if (it != self_->pathlookup_.end())
    {
        it->second->uid = uid;
        it->second->gid = gid;
        return 0;
    }

    return -ENOENT;
}


// ooze ctor arguments which we need to parse from the cmdline
struct ooze_conf
{
    unsigned long max_num_files;
    unsigned long max_file_size;
};


#define OOZE_OPT(t, p, v) {t, offsetof(ooze_conf, p), v}

struct fuse_opt ooze_opts[] = {
    OOZE_OPT("--max-num-files %ul", max_num_files, 0),
    OOZE_OPT("--max-file-size %ul", max_file_size, 0),
    FUSE_OPT_END
};

int ooze_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs)
{
    return 1;
}


int main(int argc, char** argv)
{
    //log = std::ofstream("ooze.log");

    struct fuse_operations ops;
    std::memset(&ops, 0, sizeof(ops));

    ops.getattr  = ooze::getattr;
    ops.chown    = ooze::chown;
    ops.chmod    = ooze::chmod;
    ops.truncate = ooze::truncate;
    ops.create   = ooze::create,
    ops.open     = ooze::open;
    ops.unlink   = ooze::unlink;
    ops.read     = ooze::read;
    ops.write    = ooze::write;
    ops.utimens  = ooze::utimens;
    ops.readdir  = ooze::readdir;

    ooze_conf config = {0};

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, &config, ooze_opts, ooze_opt_proc) == -1)
    {
        std::cout << "failed to parse options\n";
        return EXIT_FAILURE;
    }

    if (config.max_num_files == 0)
    {
        std::cout << "option --max-num-files must be > 0\n";
        return EXIT_FAILURE;
    }

    ooze oz(config.max_num_files, config.max_file_size);

    return fuse_main(args.argc, args.argv, &ops, nullptr);
}

