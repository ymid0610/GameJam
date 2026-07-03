#pragma once

#include "stdafx.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>

class BinaryReader
{
public:
    explicit BinaryReader(const filesystem::path& filePath);

    template <typename T>
    T Read()
    {
        T value{};
        ReadInto(value);
        return value;
    }

    template <typename T>
    void ReadInto(T& value)
    {
        static_assert(is_trivially_copyable_v<T>, "BinaryReader can only read trivially copyable values.");
        ReadBytes(&value, sizeof(T));
    }

    template <typename T>
    vector<T> ReadVector(size_t count)
    {
        static_assert(is_trivially_copyable_v<T>, "BinaryReader can only read trivially copyable values.");

        vector<T> values(count);
        if (!values.empty()) ReadBytes(values.data(), values.size() * sizeof(T));
        return values;
    }

    string ReadString();
    bool End();
    const filesystem::path& GetPath() const { return m_filePath; }

private:
    void ReadBytes(void* destination, size_t byteCount);
    uint32_t Read7BitEncodedUInt();

private:
    filesystem::path m_filePath;
    ifstream m_stream;
};
