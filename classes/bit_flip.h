#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>

class BitFlipInfo
{
public:
    std::uint64_t archiveOffset = 0;
    unsigned bitFromMsb = 0;
    unsigned char oldByte = 0;
    unsigned char newByte = 0;
    std::array<unsigned char, 4> before{};
    std::array<unsigned char, 4> after{};
    std::size_t beforeCount = 0;
    std::size_t afterCount = 0;
};

class BitFlipSimulator
{
private:
    static constexpr std::uint64_t headerSize = 48;

    static bool readOriginalSize(
        std::ifstream& file,
        std::uint64_t& originalSize)
    {
        file.seekg(0, std::ios::beg);
        if (!file)
            return false;

        char magic[4];
        unsigned char metadata[4];
        if (!file.read(magic, 4) ||
            !file.read(reinterpret_cast<char*>(metadata), 4) ||
            std::string(magic, 4) != "LAET" ||
            metadata[0] != 1 || metadata[1] != 0 ||
            metadata[2] != 0 || metadata[3] != 0)
            return false;

        originalSize = 0;
        for (int i = 0; i < 8; ++i)
        {
            char byte;
            if (!file.get(byte))
                return false;

            originalSize = (originalSize << 8) |
                static_cast<unsigned char>(byte);
        }

        return true;
    }

    static bool readByte(
        std::ifstream& file,
        std::uint64_t offset,
        unsigned char& value)
    {
        file.clear();
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        char byte;
        if (!file.get(byte))
            return false;

        value = static_cast<unsigned char>(byte);
        return true;
    }

    static bool readContext(
        const std::string& filename,
        std::uint64_t offset,
        std::uint64_t fileSize,
        BitFlipInfo& info)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
            return false;

        std::uint64_t beforeStart = offset >= 4 ? offset - 4 : 0;
        for (std::uint64_t current = beforeStart;
             current < offset && info.beforeCount < info.before.size();
             ++current)
        {
            if (!readByte(file, current, info.before[info.beforeCount]))
                return false;
            ++info.beforeCount;
        }

        for (std::uint64_t current = offset + 1;
             current < fileSize && info.afterCount < info.after.size();
             ++current)
        {
            if (!readByte(file, current, info.after[info.afterCount]))
                return false;
            ++info.afterCount;
        }

        return true;
    }

public:
    static bool flipRandomBit(
        const std::string& inputFilename,
        const std::string& outputFilename,
        BitFlipInfo& info)
    {
        std::ifstream input(inputFilename, std::ios::binary);
        if (!input.is_open())
            return false;

        input.seekg(0, std::ios::end);
        std::streamoff end = input.tellg();
        if (end <= static_cast<std::streamoff>(headerSize))
            return false;

        std::uint64_t originalSize = 0;
        if (!readOriginalSize(input, originalSize) || originalSize == 0)
            return false;

        std::uint64_t dataBits = originalSize * 14;
        std::uint64_t payloadBytes = (dataBits + 7) / 8;
        std::uint64_t fileSize = static_cast<std::uint64_t>(end);
        if (fileSize != headerSize + payloadBytes || dataBits == 0)
            return false;

        std::random_device random;
        std::mt19937_64 generator(random());
        std::uniform_int_distribution<std::uint64_t> dataBitDistribution(
            0, dataBits - 1);

        info = BitFlipInfo{};
        std::uint64_t dataBit = dataBitDistribution(generator);
        info.archiveOffset = headerSize + dataBit / 8;
        info.bitFromMsb = static_cast<unsigned>(dataBit % 8);

        if (!readByte(input, info.archiveOffset, info.oldByte))
            return false;

        info.newByte = static_cast<unsigned char>(
            info.oldByte ^ (0x80u >> info.bitFromMsb));

        input.clear();
        input.seekg(0, std::ios::beg);
        std::ofstream output(outputFilename, std::ios::binary);
        if (!output.is_open())
            return false;

        char byte;
        std::uint64_t currentOffset = 0;
        while (input.get(byte))
        {
            unsigned char value = static_cast<unsigned char>(byte);
            if (currentOffset == info.archiveOffset)
                value = info.newByte;

            output.put(static_cast<char>(value));
            ++currentOffset;
        }

        if (input.bad() || !output.good())
            return false;

        output.close();
        return readContext(outputFilename, info.archiveOffset, fileSize, info);
    }

    static bool flipRandomBitInPlace(
        const std::string& filename,
        BitFlipInfo& info)
    {
        const std::string temporaryFilename = filename + ".bitflip.tmp";
        std::ifstream existing(temporaryFilename, std::ios::binary);
        if (existing.is_open())
            return false;

        if (!flipRandomBit(filename, temporaryFilename, info))
        {
            std::remove(temporaryFilename.c_str());
            return false;
        }

        if (std::rename(temporaryFilename.c_str(), filename.c_str()) != 0)
        {
            std::remove(temporaryFilename.c_str());
            return false;
        }

        return true;
    }
};
