#include "McSolverEngine/ZipExtract.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "mse_zlib_prefix.h"

namespace McSolverEngine::ZipExtract
{

namespace
{

constexpr std::uint32_t kEocdSignature = 0x06054b50;
constexpr std::uint32_t kCentralDirEntrySignature = 0x02014b50;
constexpr std::uint32_t kLocalFileHeaderSignature = 0x04034b50;

constexpr std::size_t kMaxEocdSearch = 65557;  // max EOCD + comment

constexpr std::uint16_t kCompressionDeflated = 8;
constexpr std::uint16_t kCompressionStored = 0;

bool readU16(const unsigned char* data, std::uint16_t& out)
{
    out = static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8);
    return true;
}

bool readU32(const unsigned char* data, std::uint32_t& out)
{
    out = static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8)
          | (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
    return true;
}

bool readFileBytes(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0 || static_cast<std::uint64_t>(size) > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return !!file;
}

bool findEocd(const std::vector<unsigned char>& data, std::size_t& centralDirOffset, std::uint16_t& totalEntries)
{
    const auto fileSize = data.size();
    const auto searchEnd = fileSize < kMaxEocdSearch ? 0 : fileSize - kMaxEocdSearch;

    for (auto i = fileSize - 22; i >= searchEnd && i < fileSize; --i) {
        if (data[i] == 0x50 && data[i + 1] == 0x4b && data[i + 2] == 0x05 && data[i + 3] == 0x06) {
            std::uint16_t diskEntries = 0;
            std::uint16_t diskNumber = 0;
            std::uint16_t totalDisks = 0;
            std::uint32_t cdOffset = 0;
            readU16(&data[i + 8], diskEntries);
            readU16(&data[i + 4], diskNumber);
            readU16(&data[i + 6], totalDisks);
            readU32(&data[i + 16], cdOffset);

            if (diskNumber == 0 && totalDisks <= 1) {
                centralDirOffset = cdOffset;
                totalEntries = diskEntries;
                return true;
            }
        }
    }
    return false;
}

bool findCentralDirEntry(
    const std::vector<unsigned char>& data,
    std::size_t cdOffset,
    std::uint16_t totalEntries,
    std::string_view targetName,
    std::uint32_t& outLocalHeaderOffset,
    std::uint32_t& outCompressedSize,
    std::uint32_t& outUncompressedSize,
    std::uint16_t& outCompressionMethod
)
{
    std::size_t offset = cdOffset;
    for (std::uint16_t i = 0; i < totalEntries; ++i) {
        if (offset + 46 > data.size()) {
            return false;
        }

        std::uint32_t sig = 0;
        readU32(&data[offset], sig);
        if (sig != kCentralDirEntrySignature) {
            return false;
        }

        std::uint16_t compressionMethod = 0;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint16_t filenameLen = 0;
        std::uint16_t extraLen = 0;
        std::uint16_t commentLen = 0;
        std::uint32_t localHeaderOffset = 0;

        readU16(&data[offset + 10], compressionMethod);
        readU32(&data[offset + 20], compressedSize);
        readU32(&data[offset + 24], uncompressedSize);
        readU16(&data[offset + 28], filenameLen);
        readU16(&data[offset + 30], extraLen);
        readU16(&data[offset + 32], commentLen);
        readU32(&data[offset + 42], localHeaderOffset);

        if (offset + 46 + filenameLen > data.size()) {
            return false;
        }

        std::string_view entryName(
            reinterpret_cast<const char*>(&data[offset + 46]), filenameLen);

        if (entryName == targetName) {
            outLocalHeaderOffset = localHeaderOffset;
            outCompressedSize = compressedSize;
            outUncompressedSize = uncompressedSize;
            outCompressionMethod = compressionMethod;
            return true;
        }

        offset += 46 + filenameLen + extraLen + commentLen;
    }
    return false;
}

bool readLocalFileHeader(
    const std::vector<unsigned char>& data,
    std::uint32_t localHeaderOffset,
    std::uint32_t compressedSize,
    const unsigned char*& outCompressedData
)
{
    if (localHeaderOffset + 30 > data.size()) {
        return false;
    }

    std::uint32_t sig = 0;
    readU32(&data[localHeaderOffset], sig);
    if (sig != kLocalFileHeaderSignature) {
        return false;
    }

    std::uint16_t filenameLen = 0;
    std::uint16_t extraLen = 0;
    readU16(&data[localHeaderOffset + 26], filenameLen);
    readU16(&data[localHeaderOffset + 28], extraLen);

    const auto dataOffset = localHeaderOffset + 30 + filenameLen + extraLen;
    if (dataOffset + compressedSize > data.size()) {
        return false;
    }

    outCompressedData = &data[dataOffset];
    return true;
}

bool inflateRaw(
    const unsigned char* compressedData,
    std::uint32_t compressedSize,
    std::uint32_t uncompressedSize,
    std::string& out
)
{
    McSolverEngine_z_stream strm {};
    strm.next_in = const_cast<unsigned char*>(compressedData);
    strm.avail_in = compressedSize;

    const int windowBits = -MAX_WBITS;  // raw deflate
    int ret = McSolverEngine_inflateInit2_(&strm, windowBits, McSolverEngine_zlibVersion(), static_cast<int>(sizeof(McSolverEngine_z_stream)));
    if (ret != Z_OK) {
        return false;
    }

    try {
        out.resize(uncompressedSize);
    } catch (const std::bad_alloc&) {
        McSolverEngine_inflateEnd(&strm);
        return false;
    }

    strm.next_out = reinterpret_cast<unsigned char*>(out.data());
    strm.avail_out = uncompressedSize;

    ret = McSolverEngine_inflate(&strm, Z_FINISH);
    McSolverEngine_inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        out.clear();
        return false;
    }

    return true;
}

}  // namespace

ExtractResult extractDocumentXml(const std::string& fcstdPath)
{
    ExtractResult result;

    std::vector<unsigned char> data;
    if (!readFileBytes(fcstdPath, data)) {
        result.errorMessage = "Failed to open or read FCStd file: " + fcstdPath;
        return result;
    }

    std::size_t cdOffset = 0;
    std::uint16_t totalEntries = 0;
    if (!findEocd(data, cdOffset, totalEntries)) {
        result.errorMessage = "Not a valid ZIP file (EOCD not found).";
        return result;
    }

    std::uint32_t localHeaderOffset = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t uncompressedSize = 0;
    std::uint16_t compressionMethod = 0;
    if (!findCentralDirEntry(
            data,
            cdOffset,
            totalEntries,
            "Document.xml",
            localHeaderOffset,
            compressedSize,
            uncompressedSize,
            compressionMethod
        )) {
        result.errorMessage = "Document.xml not found in FCStd archive.";
        return result;
    }

    const unsigned char* compressedData = nullptr;
    if (!readLocalFileHeader(data, localHeaderOffset, compressedSize, compressedData)) {
        result.errorMessage = "Failed to read local file header for Document.xml.";
        return result;
    }

    if (compressionMethod == kCompressionStored && compressedSize == uncompressedSize) {
        result.documentXml.assign(
            reinterpret_cast<const char*>(compressedData), compressedSize);
        result.success = true;
        return result;
    }

    if (compressionMethod != kCompressionDeflated) {
        result.errorMessage =
            "Unsupported compression method: " + std::to_string(compressionMethod);
        return result;
    }

    if (!inflateRaw(compressedData, compressedSize, uncompressedSize, result.documentXml)) {
        result.errorMessage = "DEFLATE decompression failed.";
        return result;
    }

    result.success = true;
    return result;
}

}  // namespace McSolverEngine::ZipExtract
