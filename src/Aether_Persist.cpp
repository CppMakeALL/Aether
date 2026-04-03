/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include "Aether_Persist.hpp"
//#include <cista/cista.h>
namespace Aether {
    bool JsonPersist::save(const std::string& file_path,
                          const KVEngine& engine)
    {
        return false;
    }
    bool JsonPersist::load(const std::string& file_path,
                          const KVEngine& engine)
    {
        return false;
    }
    bool BinaryPersist::save(const std::string& file_path,
                              const KVEngine& engine)
    {
        return true;
    }
    bool BinaryPersist::load(const std::string& file_path,
                              const KVEngine& engine)
    {
        return false;
    }
}