/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include "AE_KVEngine.hpp"
#include <string>
#include <fstream>
namespace Aether {
    //持久化策略接口
    class PersistStrategy {
    public:
        PersistStrategy() = default;
        virtual ~PersistStrategy() = default;

        virtual bool save(const std::string& file_path,
                        const KVEngine& engine) = 0;

        virtual bool load(const std::string& file_path,
                        const KVEngine& engine) = 0;
    };

    class JsonPersist : public PersistStrategy {
    public:
        JsonPersist() = default;
        ~JsonPersist() = default;

        bool save(const std::string& file_path,
                const KVEngine& engine) override;

        bool load(const std::string& file_path,
                const KVEngine& engine) override;
    };

    class BinaryPersist : public PersistStrategy {
    public:
        BinaryPersist() = default;
        ~BinaryPersist() = default;

        bool save(const std::string& file_path,
                const KVEngine& engine) override;

        bool load(const std::string& file_path,
                const KVEngine& engine) override;
    };
}