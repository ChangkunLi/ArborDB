#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <unistd.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <memory>

#include "library/status.h"

namespace mydb
{
    class FileUtil
    {
    public:
        static std::string getcwd_str()
        {
            static char *path = nullptr;
            if (path != nullptr)
            {
                return path;
            }
            char *buffer = nullptr;
            int size = 64;
            do
            {
                buffer = new char[size];
                if (getcwd(buffer, size) != NULL)
                {
                    break;
                }
                size *= 2;
                delete[] buffer;
            } while (true);
            path = buffer;
            return path;
        }

        static Status fallocate(int fd, int64_t length)
        {
            struct stat buf;
            if (fstat(fd, &buf) != 0)
                return Status::IOError("fallocate() - fstat()", strerror(errno));
            if (buf.st_size >= length)
                return Status::IOError("file is large enough, no need to pre-allocate");

            const int blocksize = buf.st_blksize;
            if (ftruncate(fd, length) != 0)
                return Status::IOError("ftruncate()", strerror(errno));

            bool is_write_ok;
            int64_t offset_write = ((buf.st_size + 2 * blocksize - 1) / blocksize) * blocksize - 1;
            do
            {
                is_write_ok = false;
                if (lseek(fd, offset_write, SEEK_SET) == offset_write)
                {
                    is_write_ok = (write(fd, "", 1) == 1);
                }
                offset_write += blocksize;
            } while (is_write_ok == 1 && offset_write < length);
            return Status::OK();
        }

        static Status fallocate_filepath(std::string filepath, int64_t length)
        {
            int fd;
            if ((fd = open(filepath.c_str(), O_WRONLY | O_CREAT, 0644)) < 0) // chmod 0644 stands for read && write permission, do not allow execution
            {
                return Status::IOError("fallocate_filepath() - open()", strerror(errno));
            }
            Status s = fallocate(fd, length);
            close(fd);
            return s;
        }

        static int64_t file_system_free_space(const char *filepath)
        {
            struct statvfs stat;
            if (statvfs(filepath, &stat) != 0)
                return -1;

            if (stat.f_frsize) // On macOS, f_frsize is /* Fundamental file system block size */
            {
                return stat.f_frsize * stat.f_bavail;
            }
            else
            {
                return stat.f_bsize * stat.f_bavail;
            }
        }

        static Status remove_files_with_prefix(const char *dirpath, const std::string prefix)
        {
            DIR *directory;
            struct dirent *entry;
            if ((directory = opendir(dirpath)) == NULL)
            {
                return Status::IOError("Could not open directory", dirpath);
            }
            char filepath[FileUtil::maximum_path_size()];
            Status s;
            struct stat info;
            while ((entry = readdir(directory)) != NULL)
            {
                int ret = snprintf(filepath, FileUtil::maximum_path_size(), "%s/%s", dirpath, entry->d_name); // d_name is usually a char[]
                if (ret < 0 || ret >= FileUtil::maximum_path_size())
                    continue;
                if (strncmp(entry->d_name, prefix.c_str(), prefix.size()) != 0 || stat(filepath, &info) != 0 || !S_ISREG(info.st_mode))
                    continue;
                std::remove(filepath);
            }
            closedir(directory);
            return Status::OK();
        }

        static int64_t maximum_path_size()
        {
            return 4096;
        }

        static int persist_file(int fd)
        {
            int ret;
#ifdef F_FULLFSYNC
            // For Mac OS X
            ret = fcntl(fd, F_FULLFSYNC);
#else
            ret = fdatasync(fd);
#endif // F_FULLFSYNC
            return ret;
        }
    };

    class Mmap
    {
    public:
        Mmap(): is_valid_(false), fd_(0), filesize_(0), datafile_(nullptr) {}

        Mmap(std::string filepath, int64_t filesize): is_valid_(false), fd_(0), filesize_(filesize),
                     datafile_(nullptr), filepath_(filepath) { Open(); }

        virtual ~Mmap()
        {
            Close();
        }

        void Open()
        {
            if ((fd_ = open(filepath_.c_str(), O_RDONLY)) < 0) return;

            datafile_ = static_cast<char *>(mmap(0, filesize_, PROT_READ, MAP_SHARED, fd_, 0));
            if (datafile_ == MAP_FAILED) return;

            is_valid_ = true;
        }

        void Close()
        {
            if (datafile_ != nullptr)
            {
                munmap(datafile_, filesize_);
                close(fd_);
                datafile_ = nullptr;
                is_valid_ = false;
            }
        }

        char *datafile() { return datafile_; }
        int64_t filesize() { return filesize_; }
        bool is_valid_;
        bool is_valid() { return is_valid_; }

        int fd_;
        int64_t filesize_;
        char *datafile_;
        std::string filepath_;
    };
}

#endif