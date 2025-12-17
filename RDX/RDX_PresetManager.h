#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <vector>
#include "RDX_Types.h"

enum class FS_Type { LITTLEFS, SD_MMC };

class PresetManager {
public:
    PresetManager() = default;

    bool begin(FS_Type fsType = FS_Type::LITTLEFS) {
        fsType_ = fsType;
        switch (fsType_) {
            case FS_Type::LITTLEFS: return LittleFS.begin(true);
            case FS_Type::SD_MMC:   return SD_MMC.begin();
        }
        return false;
    }

    // -------------------------------------------------------
    // Open either a directory or a dump file (.syx)
    // -------------------------------------------------------
    bool open(FS_Type fs, const char* path) {
        currentFS_ = fs;
        currentPath_ = String(path);
        entries_.clear();
        currentIndex_ = 0;

        fs::File f = openFile(currentPath_);
        if(!f) return false;

        if(f.isDirectory()) {
            // Directory mode
            fs::File entry;
            while(entry = f.openNextFile()) {
                if(!entry.isDirectory()) {
                    String name = String(entry.name());
                    if(name.endsWith(".syx"))
                        entries_.push_back(Entry{name, 0, false});
                }
                entry.close();
            }
            ESP_LOGI("PM","Opened DIR: %s, %d files", path, entries_.size());
            return !entries_.empty();
        } 
        else {
            // Dump file mode
            uint32_t len = f.size();
            if(len < 150) return false;

            uint8_t* buf = new uint8_t[len];
            f.read(buf, len);

            uint32_t i = 0;
            while (i + 10 < len) {
                if (buf[i] == 0xF0 && buf[i + 1] == 0x43) {
                    // find next F7
                    uint32_t end = i + 1;
                    while (end < len && buf[end] != 0xF7) end++;
                    if (end >= len) break; // malformed

                    uint32_t msgLen = end - i + 1;
                    RDX_Patch tmp;
                    if (syxToPatch(buf + i, msgLen, tmp)) {
                        entries_.push_back(Entry{"", i, true});
                        i = end + 1; // jump past the message
                        continue;
                    }
                }
                i++;
            }

            delete[] buf;
            ESP_LOGI("PM","Opened DUMP: %s, %d patches", path, entries_.size());
            return !entries_.empty();
        }
    }


    // -------------------------------------------------------
    // Load next / previous / by index (unified)
    // -------------------------------------------------------
    bool loadNext(RDX_Patch &patch) {
        if(entries_.empty()) return false;
        currentIndex_ = (currentIndex_ + 1) % entries_.size();
        return loadCurrent(patch);
    }

    bool loadPrev(RDX_Patch &patch) {
        if(entries_.empty()) return false;
        currentIndex_ = (currentIndex_ + entries_.size() - 1) % entries_.size();
        return loadCurrent(patch);
    }

    bool openByIndex(uint32_t index, RDX_Patch &patch) {
        if(index >= entries_.size()) return false;
        currentIndex_ = index;
        return loadCurrent(patch);
    }

    void rewind() { currentIndex_ = 0; }

    std::vector<String> listFiles() const {
        std::vector<String> list;
        for (auto &e : entries_)
            if(!e.isDump) list.push_back(e.name);
        return list;
    }

    uint32_t size() const { return entries_.size(); }
    uint32_t currentIndex() const { return currentIndex_; }

private:
    struct Entry {
        String name;     // directory entry name
        uint32_t offset;   // dump offset
        bool isDump;     // true = dump patch
    };

    FS_Type fsType_;
    FS_Type currentFS_;
    String currentPath_;
    std::vector<Entry> entries_;
    uint32_t currentIndex_ = 0;

    // -------------------------------------------------------
    // Helpers
    // -------------------------------------------------------
    fs::File openFile(const String &path) {
        switch(currentFS_) {
            case FS_Type::LITTLEFS: return LittleFS.open(path, "r");
            case FS_Type::SD_MMC:   return SD_MMC.open(path, "r");
        }
        return fs::File();
    }

    bool loadCurrent(RDX_Patch &patch) {
        if(entries_.empty()) return false;
        const Entry &e = entries_[currentIndex_];
        if(e.isDump)
            return loadFromDump(patch, e.offset);
        else
            return loadFromFile(patch, e.name);
    }

    bool loadFromFile(RDX_Patch &patch, const String &fname) {
        fs::File f = openFile(currentPath_ + "/" + fname);
        if(!f) return false;
        uint32_t len = f.size();
        if(len < 150) return false;

        uint8_t* buf = new uint8_t[len];
        f.read(buf, len);
        bool ok = syxToPatch(buf, len, patch);
        delete[] buf;
        ESP_LOGI("PM","[DIR] idx %d/%d: %s", currentIndex_, entries_.size(), fname.c_str());
        return ok;
    }

    bool loadFromDump(RDX_Patch &patch, uint32_t offset) {
        fs::File f = openFile(currentPath_);
        if(!f) return false;
        uint32_t len = f.size();

        uint8_t* buf = new uint8_t[len];
        f.read(buf, len);
        bool ok = syxToPatch(buf + offset, len - offset, patch);
        delete[] buf;
        ESP_LOGI("PM","[DUMP] idx %d/%d, offset=%u", currentIndex_, entries_.size(), (unsigned)offset);
        return ok;
    }
};
