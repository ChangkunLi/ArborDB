#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cinttypes>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include "library/byte_array.h"
#include "library/fileutil.h"
#include "external/murmurhash3.h"
#include "core/file_manager.h"
#include "core/file_format.h"
#include "library/request.h"

namespace mydb
{
    
class FileWriter{
public:
    FileWriter(): is_closed_(true), has_unfinished_file(false), has_unfinished_request(false) {}

    FileWriter(std::string dbname, std::string file_prefix, std::string file_prefix_compact, uint32_t is_Compaction_writer): 
        dbname_(dbname), file_prefix_(file_prefix), file_prefix_compact_(file_prefix_compact)
    {
        IsCompacted_ = is_Compaction_writer;
        cur_fileid_ = 0;
        next_fileid_ = 0;
        next_timestamp_ = 0;
        reset();
        buf_file = new char[2*max_size];
        buf_map = new char[2*max_size];
    }

    void reset(){
        fm_.reset();
        next_fileid_ = 0;
        next_timestamp_ = 0;
        max_size = 10*1024*1024; // 10 MB per file
        has_unfinished_file = false;
        has_unfinished_request = false;
        is_closed_ = false;
        is_timestamp_locked = false;
        offset_begin_ = 0;
        offset_end_ = 0;
    }

    ~FileWriter() {
        Close();
    }

    void Close(){
        if(is_closed_) return;
        is_closed_ = true;
        FlushCurrentFile();
        FinishCurrentFile();  // the current file may not be large enough to trigger a finish inside FlushCurrentFile()
        delete[] buf_file;    // in this case, we need to do it manually
        delete[] buf_map;
    }

    static std::string num_hex(uint64_t num){
        char hex[32];
        sprintf(hex, "%08" PRIx64, num);
        return std::string(hex);
    }

    static uint32_t hex_num(char* hex){
        uint32_t num;
        sscanf(hex, "%x", &num);
        return num;
    }

    std::string GetFileName(uint32_t fileid) {
        return dbname_ + "/" + file_prefix_ + num_hex(fileid);
    }

    void SetNextFileID(uint32_t fileid_in) {
        std::unique_lock<std::mutex> lock(mutex_fileid_);
        next_fileid_ = fileid_in;
    }

    uint32_t GetFinishedFileID() {
        std::unique_lock<std::mutex> lock(mutex_fileid_);
        if(!has_unfinished_file) return cur_fileid_;
        return cur_fileid_-1;
    }

    uint32_t GetNextFileID() {
        std::unique_lock<std::mutex> lock(mutex_fileid_);
        return next_fileid_;
    }

    uint32_t IncreaseNextFileID(uint32_t inc) {
        std::unique_lock<std::mutex> lock(mutex_fileid_);
        return next_fileid_+= inc;
    }

    void SetNextTimestamp(uint32_t timestamp_in) {
        if(!is_timestamp_locked) next_timestamp_ = timestamp_in;
    }

    uint64_t GetNextTimestamp() {
        return next_timestamp_;
    }

    uint64_t IncreaseNextTimestamp() {
        if(!is_timestamp_locked) next_timestamp_++;
        return next_timestamp_;
    }

    void LockTimestamp(uint64_t timestamp_in) {
        is_timestamp_locked = true;
        next_timestamp_ = timestamp_in;
    }

    void CreateFile() {
        cur_fileid_ = IncreaseNextFileID(1);
        cur_timestamp_ =  IncreaseNextTimestamp();
        cur_file_name = GetFileName(cur_fileid_);
        while((fd_ = open(cur_file_name.c_str(), O_WRONLY|O_CREAT, 0644)) < 0) ;
        has_unfinished_file = true;

        offset_begin_ = 0;
        offset_end_ = FileHeader::GetSize();

        FileHeader file_header;
        file_header.IsCompacted_ = IsCompacted_;
        file_header.timestamp = cur_timestamp_;
        FileHeader::Encoder(&file_header, buf_file);
    }

    uint32_t FlushCurrentFile() {
        if(!has_unfinished_file) return 0;

        if(!has_unfinished_request){
            write(fd_, buf_file + offset_begin_, offset_end_ - offset_begin_);
            fm_.SetFileSize(cur_fileid_, offset_end_);
            offset_begin_ = offset_end_;
            has_unfinished_request = false;
        }

        if(offset_end_ >= max_size) {
            fm_.SetFileSize(cur_fileid_, offset_end_);
            FinishCurrentFile();
        }

        return cur_fileid_;
    }

    void FinishCurrentFile() {
        if(!has_unfinished_file) return;
        FlushInternalMap();
        FileUtil::persist_file(fd_);  // persist finished file to disk
        close(fd_);
        has_unfinished_file = false;
        has_unfinished_request = false;
    }

    void FlushInternalMap() {
        if(!has_unfinished_file) return;
        fm_.SetFileSize(cur_fileid_, offset_end_);
        ftruncate(fd_, offset_end_);
        uint64_t newSize = offset_end_ + Append_InternalMap_Footer(fm_.GetInternalMap(cur_fileid_));
        fm_.ClearInternalMap(cur_fileid_);  // once the internal map is on disk, we no longer need to store it in memory
        fm_.SetFileSize(cur_fileid_, newSize);
    }

    uint64_t Append_InternalMap_Footer(const std::vector<std::pair<uint64_t, uint32_t>>& iMap) {
        uint64_t num_bytes_written = 0;
        InternalMapLine line;
        for(auto& elem : iMap){
            line.hashed_key = elem.first;
            line.offset_record = elem.second;
            InternalMapLine::Encoder(&line, buf_map + num_bytes_written);
            num_bytes_written+= InternalMapLine::GetSize();
        }

        off_t file_offset = lseek(fd_, 0, SEEK_END);
        FileFooter file_footer;
        file_footer.IsCompacted_ = IsCompacted_;
        file_footer.offset_InternalMap = file_offset;
        file_footer.num_records = iMap.size();
        file_footer.magic_number = get_magic_number();
        FileFooter::Encoder(&file_footer, buf_map+num_bytes_written);
        num_bytes_written+= FileFooter::GetSize();

        write(fd_, buf_map, num_bytes_written);
        ftruncate(fd_, file_offset + num_bytes_written);

        return num_bytes_written;
    }

    void Write_Requests_To_Buffer(std::vector<Request>& requests, std::multimap<uint64_t, uint64_t>& map_output) {
        for(Request& req:requests){
            if(offset_end_ >= max_size) FlushCurrentFile();  // finish current file
            if(!has_unfinished_file) CreateFile();  // also reset offset_begin_ and offset_end_
            uint64_t hashed_key = Murmurhash(req.key.bytes(), req.key.size());

            // write request to buffer -- start
            uint64_t loc = WriteOneRequest(hashed_key, req);
            //  write request to buffer -- end
            has_unfinished_request = true;
            map_output.insert(std::make_pair(hashed_key, loc));
        }
        FlushCurrentFile();
    }

    uint64_t WriteOneRequest(const uint64_t& hashed_key, Request& req) {
        uint64_t loc = 0;
        RecordHeader r_header;
        if(req.type == TypeRequest::Delete){
            r_header.SetDelete();
            r_header.size_key = req.key.size();
            r_header.size_val = 0;
            r_header.hashed_key = hashed_key;
            RecordHeader::Encoder(&r_header, buf_file + offset_end_);
            memcpy(buf_file+offset_end_+RecordHeader::GetSize(), req.key.bytes_const(), r_header.size_key);
            loc = cur_fileid_;
            loc <<= 32;
            loc = loc | offset_end_;
            fm_.incInternalMap(cur_fileid_, std::pair<uint64_t, uint32_t>(hashed_key, offset_end_));
            offset_end_ += RecordHeader::GetSize()+r_header.size_key;
        }
        else{
            r_header.SetPut();
            r_header.size_key = req.key.size();
            r_header.size_val = req.size_val;
            r_header.hashed_key = hashed_key;
            RecordHeader::Encoder(&r_header, buf_file + offset_end_);
            memcpy(buf_file+offset_end_ + RecordHeader::GetSize(), req.key.bytes_const(), r_header.size_key);
            memcpy(buf_file+offset_end_ + RecordHeader::GetSize() + r_header.size_key, req.val.bytes_const(), r_header.size_val);
            loc = cur_fileid_;
            loc <<= 32;
            loc = loc | offset_end_;
            fm_.incInternalMap(cur_fileid_, std::pair<uint64_t, uint32_t>(hashed_key, offset_end_));
            offset_end_ += RecordHeader::GetSize() + r_header.size_key + r_header.size_val;
        }
        return loc;
    }

    void LoadAll_InternalMaps(std::multimap<uint64_t, uint64_t>& map_output) {
        FileUtil::remove_files_with_prefix(dbname_.c_str(), file_prefix_compact_);  // remove temporary files used for compaction
        DIR* dirp;
        struct dirent* d_entry;
        dirp = opendir(dbname_.c_str());
        std::multimap<uint64_t, uint32_t> timestamp_fileid;  // load the files into memory in the increasing order of timestamp 
        // we could use a multimap here because if two files have same timestamp, they can only be created during the same compaction process
        // which means that they won't have any identical hashed key, so we can load them in any order
        char path_to_dbfile[1024];
        uint32_t fileid, max_fileid;
        uint64_t max_timestamp;
        struct stat info;
        while((d_entry = readdir(dirp)) != nullptr) {
            snprintf(path_to_dbfile, 1024, "%s/%s", dbname_.c_str(), d_entry->d_name);
            if(stat(path_to_dbfile, &info)!=0 || !S_ISREG(info.st_mode)) continue;
            fileid = hex_num(d_entry->d_name);
            Mmap memo_map(std::string(path_to_dbfile), info.st_size);
            FileHeader file_header;
            FileHeader::Decoder(memo_map.datafile(), &file_header);
            timestamp_fileid.insert(std::pair<uint64_t, uint32_t>(file_header.timestamp, fileid));
            max_fileid = std::max(max_fileid, fileid);
            max_timestamp = std::max(max_timestamp, file_header.timestamp);
        }

        for(auto& elem : timestamp_fileid) {
            fileid = elem.second;
            std::string path = GetFileName(fileid);
            if(stat(path.c_str(), &info) != 0) continue;
            Mmap memo_map(path, info.st_size);
            bool is_compacted;
            LoadOneFile(memo_map, fileid, map_output, &is_compacted);
            fm_.SetFileSize(fileid, info.st_size);
            if(is_compacted) fm_.SetFileCompacted(fileid);
        }

        SetNextFileID(max_fileid);
        SetNextTimestamp(max_timestamp);
        closedir(dirp);
    }

    void LoadOneFile(Mmap& memo_map, const uint32_t& fileid, std::multimap<uint64_t, uint64_t>& map_output, bool* is_compacted=nullptr) {
        FileFooter file_footer;
        FileFooter::Decoder(memo_map.datafile() + memo_map.filesize() - FileFooter::GetSize(), &file_footer);
        if(file_footer.magic_number != get_magic_number()){
            std::cout << "Wrong magic number" << std::endl;
            return;
        }
        if(is_compacted != nullptr) *is_compacted = file_footer.IsCompacted();
        uint64_t offset_imap = file_footer.offset_InternalMap;
        InternalMapLine line;
        for(int i=0; i<file_footer.num_records; i++){
            InternalMapLine::Decorder(memo_map.datafile() + offset_imap, &line);
            uint64_t loc = fileid;
            loc = loc << 32;
            loc = loc | line.offset_record;
            map_output.insert(std::make_pair(line.hashed_key, loc));
            offset_imap+= InternalMapLine::GetSize();
        }
    }

    static uint64_t get_magic_number() { return 0x19970320; }

    // public members
    FileManager fm_;

private:
    bool is_closed_;
    bool has_unfinished_file;
    bool has_unfinished_request;
    bool is_timestamp_locked;
    uint32_t IsCompacted_;

    uint32_t cur_fileid_;
    uint32_t next_fileid_;
    std::mutex mutex_fileid_;

    uint64_t cur_timestamp_;
    uint64_t next_timestamp_;  // unlike fileid, we don't need a mutex to enforce synchronization on timestamp

    uint64_t max_size;
    int fd_;  // current file descriptor

    std::string dbname_;
    std::string file_prefix_;
    std::string file_prefix_compact_;
    std::string cur_file_name;  // file name including the full path to it

    char* buf_file;
    char* buf_map;
    uint64_t offset_begin_;
    uint64_t offset_end_;
};

} // namespace mydb


#endif