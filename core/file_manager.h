#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdio>
#include <ctime>

#include "library/byte_array.h"
#include "library/fileutil.h"
#include "external/murmurhash3.h"

namespace mydb
{

class FileManager{
public:
    FileManager() {
        reset();
    }

    void reset() {
        fileID_to_size.clear();
        compactedFiles.clear();
        fileID_to_internalMap.clear();
    }

    void ClearInternalMap(uint32_t fileid){
        fileID_to_internalMap.erase(fileid);
    }

    void ClearFileID(uint32_t fileid) {
        fileID_to_size.erase(fileid);
        compactedFiles.erase(fileid);
        ClearInternalMap(fileid);
    }

    uint64_t GetFileSize(uint32_t fileid) { return fileID_to_size[fileid]; }

    void SetFileSize(uint32_t fileid, uint64_t filesize) {
        fileID_to_size[fileid] = filesize;
    }

    bool isFileCompacted(uint32_t fileid) {
        return compactedFiles.count(fileid) == 1;
    }

    void SetFileCompacted(uint32_t fileid) {
        compactedFiles.insert(fileid); // duplicates will be ignored automatically
    }

    std::vector<std::pair<uint64_t,uint32_t>> GetInternalMap(uint32_t fileid) {
        return fileID_to_internalMap[fileid];
    }

    void incInternalMap(uint32_t fileid, std::pair<uint64_t, uint32_t> line) {
        fileID_to_internalMap[fileid].push_back(line);
    }

private:
    std::unordered_map<uint32_t, uint64_t> fileID_to_size;
    std::unordered_set<uint32_t> compactedFiles;
    std::unordered_map<uint32_t, std::vector<std::pair<uint64_t, uint32_t>>> fileID_to_internalMap;
};

} // namespace mydb


#endif