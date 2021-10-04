#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <cinttypes>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include "library/event_manager.h"
#include "library/status.h"
#include "library/request.h"
#include "library/byte_array.h"
#include "library/fileutil.h"
#include "library/hash.h"
#include "core/file_format.h"
#include "core/file_manager.h"
#include "core/file_writer.h"

namespace mydb
{
class DiskEngine{
public:
    DiskEngine(EventManager* evm, std::string name_db): evm_(evm), name_db_(name_db), tmp_compact_file_prefix_("tmp_compact_"),
    fwriter_(name_db, "", tmp_compact_file_prefix_, 0), 
    fwriter_compact_(name_db, tmp_compact_file_prefix_, tmp_compact_file_prefix_, 1)
    {
        num_readers_ = 0;
        is_in_compaction_ = false;
        is_stop_requested_ = false;
        is_closed_ = false;
        active_compaction_requested_ = false;
        thd_map_ = std::thread(&DiskEngine::RunLoop_Map, this);
        thd_write_ = std::thread(&DiskEngine::RunLoop_Write, this);
        thd_compact_ = std::thread(&DiskEngine::RunLoop_Compact, this);
        fwriter_.LoadAll_InternalMaps(in_memory_map_);
    }

    void Close() {
        if(is_closed_) return;
        is_closed_ = true;

        mutex_write_.lock();
        while(true) {
            std::unique_lock<std::mutex> lock_read(mutex_read_);
            if(num_readers_ == 0) break;
            cv_read_.wait(lock_read);
        }        
        fwriter_.Close();  // release meory allocated to buffer in FileWriter object
        is_stop_requested_ = true;
        mutex_write_.unlock();

        evm_->update_index.NotifyWait();  // exit runloop_map
        evm_->flush_buffer.NotifyWait();  // exit runloop_write
        cv_compaction_.notify_all();
        thd_map_.join();
        thd_write_.join();
        thd_compact_.join();

        fwriter_compact_.Close();   // release meory allocated to buffer in FileWriter object
    }

    bool IsStopRequested() { return is_stop_requested_; }

    void RunLoop_Map() {
        while(true) {
            std::multimap<uint64_t, uint64_t> map_inc = evm_->update_index.Wait();
            if(is_stop_requested_ == true) return;

            std::multimap<uint64_t, uint64_t>* map_;
            bool is_main_map;
            mutex_compaction_check_.lock();
            if(is_in_compaction_){
                map_ = &in_memory_map_compact_;
                is_main_map = false;
                mutex_map_compact_.lock();
            }
            else{
                map_ = &in_memory_map_;
                is_main_map = true;
                mutex_map_main_.lock();
            }
            mutex_compaction_check_.unlock();

            int max_consecutive_write = 50;
            int count = 0;
            for(auto& p : map_inc){
                if((count % max_consecutive_write) == 0){
                    mutex_write_.lock();  // grab the lock to write in_memory map
                    while(true) {
                        std::unique_lock<std::mutex> lock_read(mutex_read_);
                        if(num_readers_ == 0) break;
                        cv_read_.wait(lock_read);
                    }
                }
                
                count++;
                map_->insert(p);

                if((count % max_consecutive_write) == 0) mutex_write_.unlock();
            }
            if((count % max_consecutive_write) != 0) mutex_write_.unlock();

            if(is_main_map) mutex_map_main_.unlock();
            else mutex_map_compact_.unlock();

            evm_->update_index.Done();
            int status = 1;
            evm_->clear_buffer.StartAndBlockUntilDone(status);            
        }
    }

    void RunLoop_Write() {
        while(true) {
            std::vector<Request> requests = evm_->flush_buffer.Wait();
            if(is_stop_requested_ == true) return;  // is notified in Close(), we should exit the runloop

            std::unique_lock<std::mutex> lock_write(mutex_write_); // grab the lock to write file
            std::multimap<uint64_t, uint64_t> map_inc;
            fwriter_.Write_Requests_To_Buffer(requests, map_inc);
            lock_write.unlock();

            evm_->flush_buffer.Done();
            evm_->update_index.StartAndBlockUntilDone(map_inc);
        }
    }

    void RunLoop_Compact() {
        std::chrono::minutes period(1);  // run compaction at least every 1 minute
        uint32_t max_fileid_last_compacted = 0;
        while(true) {
            uint32_t last_finished_fileid = fwriter_.GetFinishedFileID();
            if(last_finished_fileid > 0){
                Compaction(max_fileid_last_compacted+1, last_finished_fileid);
                max_fileid_last_compacted = last_finished_fileid;
                if(active_compaction_requested_){
                    int signal = 1;
                    evm_->compaction_status.StartAndBlockUntilDone(signal);
                }
            }
            std::unique_lock<std::mutex> lock_compact(mutex_compact_);
            cv_compaction_.wait_for(lock_compact, period);
            if(is_stop_requested_ == true) return;
        }
    }

    Status Compaction(uint32_t fileid_begin, uint32_t fileid_end) {  // central part of this database engine
        mutex_compaction_check_.lock();
        is_in_compaction_ = true;
        mutex_map_main_.lock();
        mutex_map_main_.unlock();   // wait for the current updating of in memory map to finish
        mutex_compaction_check_.unlock();

        FileUtil::remove_files_with_prefix(name_db_.c_str(), tmp_compact_file_prefix_);

        // load uncompacted files
        std::multimap<uint64_t, uint64_t> map_uncompacted_file;
        DIR* dirp;  // directory path
        dirent* d_entry;
        dirp = opendir(name_db_.c_str());

        std::string filepath;
        uint32_t fileid;
        struct stat info;
        while((d_entry = readdir(dirp)) != nullptr) {
            fileid = FileWriter::hex_num(d_entry->d_name);
            if( fileid < fileid_begin || fileid > fileid_end || fwriter_.fm_.isFileCompacted(fileid)) continue;
            filepath = fwriter_.GetFileName(fileid);
            if(stat(filepath.c_str(), &info)!=0 || !S_ISREG(info.st_mode)) continue;
            Mmap memo_map(filepath.c_str(), info.st_size);
            fwriter_.LoadOneFile(memo_map, fileid, map_uncompacted_file);
        }
        closedir(dirp);

        // find all records whose hashed key appears in "map_uncompacted_file"
        std::vector<std::pair<uint64_t, uint64_t>> vec_of_uncompacted_hashed_key;
        uint64_t prev_hashed_key;
        uint64_t cur_hashed_key;
        bool isFirst = true;
        for(auto it = map_uncompacted_file.begin(); it!=map_uncompacted_file.end(); it++) {
            cur_hashed_key = it->first;
            if(isFirst){
                prev_hashed_key = cur_hashed_key;
                isFirst = false;
            }
            else if(cur_hashed_key == prev_hashed_key) continue;
            else prev_hashed_key = cur_hashed_key;
            auto interval = in_memory_map_.equal_range(cur_hashed_key);
            for(auto sit = interval.first; sit != interval.second; sit++){
                vec_of_uncompacted_hashed_key.push_back(*sit);
            }
        }
        map_uncompacted_file.clear();

        // pre-processing
        // loc_delete : location to be deleted
        // fileids_compact : fileids to be compacted
        // keys_set : temporary hash set used to store all visited keys (in original format, not hashed format)
        // hashedkey_to_loc_remain : locations to be kept together with their hashed key
        std::unordered_set<uint64_t> loc_delete;
        std::set<uint32_t> fileids_compact;
        std::unordered_set<std::string> keys_set;
        std::map<uint64_t, std::set<uint64_t>> hashedkey_to_loc_remain;

        int N = vec_of_uncompacted_hashed_key.size();
        uint64_t loc;
        uint64_t hashed_key;
        std::string real_key;
        for(int i=N-1; i>=0; i--) {
            hashed_key = vec_of_uncompacted_hashed_key[i].first;
            loc = vec_of_uncompacted_hashed_key[i].second;
            fileid = (loc >> 32);
            fileids_compact.insert(fileid);
            ByteArray key, value;
            Status s = TestRecord(&key, &value, loc);
            real_key = key.ToString();
            if(!keys_set.count(real_key)) {
                keys_set.insert(real_key);
                if(!s.IsDeleteOrder()) hashedkey_to_loc_remain[hashed_key].insert(loc);
                else loc_delete.insert(loc);
            }
            else loc_delete.insert(loc);
        }
        vec_of_uncompacted_hashed_key.clear();
        keys_set.clear();

        // hashedkeys_group : group of same hashed key indexed by the smallest location
        // trailing_loc_for_hashed_key : stores all the non-smallest location in hashedkeys_group
        std::unordered_map<uint64_t, std::vector<uint64_t>> hashedkeys_group; 
        std::unordered_set<uint64_t> trailing_loc_for_hashed_key;
        uint64_t min_loc;
        for(auto it = hashedkey_to_loc_remain.begin(); it!=hashedkey_to_loc_remain.end(); it++) {
            min_loc = *(it->second.begin());
            hashedkeys_group[min_loc] = std::vector<uint64_t>(1, min_loc);
            for(auto sit = ++(it->second.begin()); sit!=it->second.end(); sit++) {
                hashedkeys_group[min_loc].push_back(*sit);
                trailing_loc_for_hashed_key.insert(*sit);
            }
        }
        hashedkey_to_loc_remain.clear();

        //
    }

    void Compact() {
        active_compaction_requested_ = true;
        cv_compaction_.notify_all();
        evm_->compaction_status.Wait();
        evm_->compaction_status.Done();
        active_compaction_requested_ = false;
    }

    Status Get(ByteArray &key, ByteArray *value_out) {
        mutex_write_.lock();
        mutex_read_.lock();
        num_readers_ += 1;
        mutex_read_.unlock();
        mutex_write_.unlock();

        Status ret;
        ret = SearchMap(key, value_out, in_memory_map_compact_);
        if(!ret.IsOK() && !ret.IsDeleteOrder()) ret = SearchMap(key, value_out, in_memory_map_);

        mutex_read_.lock();
        num_readers_ -= 1;
        mutex_read_.unlock();

        cv_read_.notify_one();
        return ret;
    }

    Status SearchMap(ByteArray &key, ByteArray *value_out, std::multimap<uint64_t, uint64_t>& map_in) {
        uint64_t hashed_key = HashFunction(key.bytes_const(), key.size());

        auto e_range = map_in.equal_range(hashed_key);
        Status ret;
        for(auto it = e_range.second; it != e_range.first; ) {
            it--;
            ByteArray key_cur;
            ret = TestRecord(&key_cur, value_out, it->second);
            if((ret.IsOK() || ret.IsDeleteOrder()) && key_cur == key) return ret;
        }
        return Status::NotFound("Key is not found on disk");
    }

    Status TestRecord(ByteArray *key_out, ByteArray *value_out, uint64_t loc) {
        uint32_t fileid = loc >> 32;
        uint32_t offset = (loc & 0x00000000FFFFFFFF);
        uint64_t filesize = fwriter_.fm_.GetFileSize(fileid);
        std::string filepath = fwriter_.GetFileName(fileid);
        
        Mmap m_map(filepath, filesize);
        RecordHeader r_header;
        RecordHeader::Decoder(m_map.datafile() + offset, &r_header);

        *key_out = ByteArray::NewCopyByteArray(m_map.datafile() + offset + RecordHeader::GetSize(), r_header.size_key);
        *value_out = ByteArray::NewCopyByteArray(m_map.datafile() + offset + RecordHeader::GetSize() + r_header.size_key, r_header.size_val);

        if(r_header.IsDelete()) return Status::DeleteOrder();
        return Status::OK();
    }

    uint32_t FlushFile() {
        uint32_t ret = fwriter_.FlushCurrentFile();
        fwriter_.FinishCurrentFile();
        return ret;
    }

private:
    EventManager* evm_;

    FileWriter fwriter_;
    FileWriter fwriter_compact_;

    std::string name_db_;
    std::string tmp_compact_file_prefix_;

    uint32_t num_readers_;

    bool is_in_compaction_;
    bool is_stop_requested_;
    bool is_closed_;
    bool active_compaction_requested_;

    std::multimap<uint64_t, uint64_t> in_memory_map_;
    std::multimap<uint64_t, uint64_t> in_memory_map_compact_;

    std::thread thd_map_;
    std::thread thd_write_;
    std::thread thd_compact_;

    std::mutex mutex_write_;
    std::mutex mutex_read_;
    std::mutex mutex_compaction_check_;
    std::mutex mutex_compact_;      // will only be used in RunLoop_Compact
    std::mutex mutex_map_main_;
    std::mutex mutex_map_compact_;  // used to store map when compaction is in process (when compaction is in process,
                                    // we can not modify main map)

    std::condition_variable cv_read_;   // notify other thread after finish one read
    std::condition_variable cv_compaction_;  // notify compaction thread to start compacting or ready to close
};    
} // namespace mydb

#endif