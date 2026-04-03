/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include "AE_Evict.hpp"

namespace Aether {
    std::string FIFOStrategy::evict(const std::unordered_map<std::string, KVEntry>& entries) {
        // if (entries.empty()) return {};
        // auto oldest = entries.begin();
        // for (auto it = entries.begin(); it != entries.end(); ++it) {
        //     if (it->second.create_time_ < oldest->second.create_time_)
        //         oldest = it;
        // }
        // return oldest->first;
        return std::string();
    }
    std::string LRUStrategy::evict(const std::unordered_map<std::string, KVEntry>& entries) {
        return std::string();
    }
    std::string LFUtrategy::evict(const std::unordered_map<std::string, KVEntry>& entries) {
        return std::string();
    }
    
}