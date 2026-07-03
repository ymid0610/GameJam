#include "binaryreader.h"

BinaryReader::BinaryReader(const filesystem::path& filePath)
    : m_filePath(filePath), m_stream(filePath, ios::binary)
{
    if (!m_stream)
    {
        throw runtime_error("Failed to open binary file: " + filePath.string());
    }
}

string BinaryReader::ReadString()
{
    uint32_t length = Read7BitEncodedUInt();
    string value(length, '\0');
    if (length > 0) ReadBytes(value.data(), length);
    return value;
}

bool BinaryReader::End()
{
    return m_stream.peek() == char_traits<char>::eof();
}

void BinaryReader::ReadBytes(void* destination, size_t byteCount)
{
    if (byteCount == 0) return;

    m_stream.read(static_cast<char*>(destination), static_cast<streamsize>(byteCount));
    if (!m_stream)
    {
        throw runtime_error("Unexpected end of binary file: " + m_filePath.string());
    }
}

uint32_t BinaryReader::Read7BitEncodedUInt()
{
    uint32_t value = 0;
    int shift = 0;

    while (shift < 35)
    {
        uint8_t byte = Read<uint8_t>();
        value |= static_cast<uint32_t>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) return value;
        shift += 7;
    }

    throw runtime_error("Invalid 7-bit encoded string length in: " + m_filePath.string());
}
