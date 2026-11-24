#pragma once

#include <fstream>
#include <string>
#include "data.h"
#include <array>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <openssl/evp.h>

// fun fact, a nibble is the word for exactly 4 bits
// 8 bits is a byte

class Archive {
private:
    std::uint64_t originalSize = 0;
    std::array<unsigned char, 32> originalHash{};
    bool archiveReady = false;
    std::string sourceFilename;
    std::size_t correctedCodewords = 0;
    std::string lastDecodeError;

    class BitWriter {
    private:
        std::ostream& out;
        unsigned char currentByte = 0;
        int bitCount = 0;

    public:
        explicit BitWriter(std::ostream& stream) : out(stream) {}

        bool writeBit(bool bit)
        {
            currentByte = static_cast<unsigned char>((currentByte << 1) | bit);
            ++bitCount;

            if (bitCount == 8)
            {
                out.put(static_cast<char>(currentByte));
                currentByte = 0;
                bitCount = 0;
            }

            return out.good();
        }

        bool finish()
        {
            if (bitCount != 0)
            {
                currentByte <<= (8 - bitCount);
                out.put(static_cast<char>(currentByte));
            }

            return out.good();
        }
    };

    class BitReader {
    private:
        std::istream& in;
        unsigned char currentByte = 0;
        int bitsRemaining = 0;

    public:
        explicit BitReader(std::istream& stream) : in(stream) {}

        bool readBit(bool& bit)
        {
            if (bitsRemaining == 0)
            {
                char nextByte;
                if (!in.get(nextByte))
                    return false;

                currentByte = static_cast<unsigned char>(nextByte);
                bitsRemaining = 8;
            }

            bit = (currentByte & 0x80) != 0;
            currentByte <<= 1;
            --bitsRemaining;
            return true;
        }

        bool paddingIsZero() const
        {
            return currentByte == 0;
        }

        bool hasExtraBytes()
        {
            return in.peek() != std::char_traits<char>::eof();
        }
    };

    bool encodeNibble(unsigned char nibble, BitWriter& writer) const
    {
        Data encoder;

        encoder.setData(
            (nibble >> 3) & 1,
            (nibble >> 2) & 1,
            (nibble >> 1) & 1,
            nibble & 1
        );

        encoder.encodeData();

        std::array<bool, 7> result = encoder.getEncodedData();

        for (bool bit : result)
            if (!writer.writeBit(bit))
                return false;

        return true;
    }


    bool correctEncodedData(std::array<bool, 7>& encodedData)
    {
        // based on data bits being in the 3, 5, 6, 7 positions
        // parity bits sit in positions 1, 2, and 4
        // the std parity check matrix. produces syndrom when we multiply it with our encodedData object
        const bool Hmatrix[3][7] = {
            {0, 0, 0, 1, 1, 1, 1},
            {0, 1, 1, 0, 0, 1, 1},
            {1, 0, 1, 0, 1, 0, 1}
        };

        int syndrome = 0;

        // loops over the rows then cols of the h matrix and the encodedData object to produce a syndrom (commonly displayed as a 3 x 1 vector)
        // s = | 0 |
        //     | 0 |
        //     | 0 |
        // every row, all cols are parity checked using ^= or the XOR bool operation
        // since we dont actually build a 3 x 1 vector, we just create a 3 bit "101" syndrome
        for (int row = 0; row < 3; row++)
        {
            bool parity = false;

            for (int col = 0; col < 7; col++)
            {
                parity ^= Hmatrix[row][col] && encodedData[col];
            }
            // shifting bits here to make sure we have a 3 bit syndrom without extra bits
            syndrome = (syndrome << 1) | parity;
        }


        // note that the syndrome should be all 0s
        // Ax = 0 (every valid encodedData or codeword object is in the Nullspace of the H-parity check matrix)
        if (syndrome != 0)
        {
            encodedData[syndrome - 1] = !encodedData[syndrome - 1];
            ++correctedCodewords;
            return true;
        }

        return false;
    }

    // Hash helper used to verify the recovered file.
    static bool calculateHash(
        const std::string& filename,
        std::array<unsigned char, 32>& digest)
    {
        std::ifstream file(filename, std::ios::binary);

        if (!file.is_open())
            return false;

        EVP_MD_CTX* context = EVP_MD_CTX_new();

        if (context == nullptr)
            return false;

        bool success =
            EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;

        char buffer[65536];

        while (success && file.read(buffer, sizeof(buffer)))
        {
            success =
                EVP_DigestUpdate(context, buffer, sizeof(buffer)) == 1;
        }

        if (success && file.bad())
            success = false;

        if (success && file.gcount() > 0)
        {
            success = EVP_DigestUpdate(
                context,
                buffer,
                static_cast<std::size_t>(file.gcount())
            ) == 1;
        }

        unsigned int digestLength = 0;

        if (success)
        {
            success = EVP_DigestFinal_ex(
                context,
                digest.data(),
                &digestLength
            ) == 1 && digestLength == digest.size();
        }

        EVP_MD_CTX_free(context);

        return success;
    }



    unsigned char decodeNibble(std::array<bool, 7> encodedData)
    {
        correctEncodedData(encodedData);

        unsigned char nibble = 0;
        // because we are using the std representation of the parity and data bits (parity ocupy positions 1,2, 4) the rest of the bits are our data bits. storing these in an array for extraction
        const int dataPositions[4] = {2, 4, 5, 6};

        for (int i = 0; i < 4; i++)
        {
            // reconstructing encodedData using left shifts to extract bits
            nibble = (nibble << 1) | encodedData[dataPositions[i]];
        }

        return nibble;
    }

    // helper functions for 64bit ints. using big endian
    static void writeUint64(std::ostream& out, std::uint64_t value)
    {
        for (int i = 7; i >= 0; i--)
        {
            unsigned char byte =
                static_cast<unsigned char>((value >> (i * 8)) & 0xFF);

            out.put(static_cast<char>(byte));
        }
    }

    static bool readUint64(std::istream& in, std::uint64_t& value)
    {
        value = 0;

        for (int i = 0; i < 8; i++)
        {
            char byte;

            if (!in.get(byte))
                return false;

            value = (value << 8)
                | static_cast<unsigned char>(byte);
        }

        return true;
    }

    // writes file header bytes so we have a actual distringuishable file type, not just assuming
    /**
     * @brief information about the file is stored in header. This helper function builds this metadata into the .laetf file
     */
    bool writeHeader(std::ostream& out) const
    {
        // magic number: 4 bytes
        out.write("LAET", 4);

        // version: 1 byte
        out.put(static_cast<char>(1));

        // flags: 1 byte (currently unused)
        out.put(static_cast<char>(0));

        // reserved: 2 bytes
        out.put(static_cast<char>(0));
        out.put(static_cast<char>(0));

        // original size: 8 bytes
        writeUint64(out, originalSize);

        // original SHA-256 digest: 32 bytes
        out.write(
            reinterpret_cast<const char*>(originalHash.data()),
            originalHash.size()
        );

        return out.good();
    }





public:
    bool encodeFile(const std::string& filename)
    {
        archiveReady = false;
        originalSize = 0;
        originalHash.fill(0);
        sourceFilename.clear();

        std::ifstream file(filename, std::ios::binary);
        // opens the file in binary mode so we can read the raw bytes without text conversions

        if (!file.is_open())
        {
            return false;
        }

        char byte;

        while (file.get(byte))
        {
            ++originalSize;
        }


        if (file.bad())
        {
            return false;
        }

        // Calculate SHA-256 of the original file.
        if (!calculateHash(filename, originalHash))
        {
            return false;
        }

        sourceFilename = filename;
        archiveReady = true;

        return true;

    }


    bool decodeFile(const std::string& inputFilename, const std::string& outputFilename)
    {
        correctedCodewords = 0;
        lastDecodeError.clear();

        std::ifstream in(inputFilename, std::ios::binary);

        if (!in.is_open())
        {
            return false;
        }

        // input stream is set at byte position 48
        std::uint64_t originalBytes = 0;
        std::array<unsigned char, 32> expectedHash{};
        if (!readHeader(in, originalBytes, expectedHash))
        {
            return false;
        }

        // Make sure arithmetic cannot overflow.
        if (originalBytes > std::numeric_limits<std::size_t>::max() / 14)
        {
            return false;
        }

        std::ofstream out(outputFilename, std::ios::binary);

        if (!out.is_open())
        {
            return false;
        }

        BitReader reader(in);

        for (std::size_t i = 0; i < originalBytes; i++)
        {
            std::array<bool, 7> upperData{};
            std::array<bool, 7> lowerData{};

            for (bool& bit : upperData)
                if (!reader.readBit(bit))
                    return false;

            for (bool& bit : lowerData)
                if (!reader.readBit(bit))
                    return false;

            // correct errors and recover four data bits each.
            unsigned char upper = decodeNibble(upperData);
            unsigned char lower = decodeNibble(lowerData);

            // combine the two nibbles into one original byte.
            unsigned char originalByte = (upper << 4) | lower;

            out.put(static_cast<char>(originalByte));
        }

        if (!reader.paddingIsZero() || reader.hasExtraBytes())
            return false;

        out.close();

        if (!out.good())
        {
            return false;
        }

        std::array<unsigned char, 32> recoveredHash{};

        if (!calculateHash(outputFilename, recoveredHash))
        {
            return false;
        }

        if (recoveredHash != expectedHash)
        {
            lastDecodeError =
                "sha-256 mismatch, archive may contain multiple-bit corruption";
            std::remove(outputFilename.c_str());
            return false;
        }

        return true;
    }


    std::size_t getCorrectedCodewords() const
    {
        return correctedCodewords;
    }


    const std::string& getLastDecodeError() const
    {
        return lastDecodeError;
    }


    bool saveFile(const std::string& filename) const
    {
        std::ofstream out(filename, std::ios::binary);
        // new logic to make sure file is archivable and has the required header information encoded into it
        if (!archiveReady)
        {
            return false;
        }

        if (!out.is_open())
        {
            return false;
        }


        if (!writeHeader(out))
        {
            return false;
        }

        std::ifstream source(sourceFilename, std::ios::binary);
        if (!source.is_open())
            return false;

        BitWriter writer(out);
        char byte;
        while (source.get(byte))
        {
            unsigned char value = static_cast<unsigned char>(byte);
            if (!encodeNibble((value >> 4) & 0x0F, writer) ||
                !encodeNibble(value & 0x0F, writer))
                return false;
        }

        if (source.bad() || !writer.finish())
            return false;

        return out.good();
    }


    // does a reverse of what writeHeader() does, it will read and validate the header.
    static bool readHeader(
        std::istream& in,
        std::uint64_t& fileSize,
        std::array<unsigned char, 32>& expectedHash)
    {
        char magic[4];

        if (!in.read(magic, 4))
            return false;

        if (std::string(magic, 4) != "LAET")
            return false;

        // read version, flags, and reserved bytes
        unsigned char metadata[4];

        if (!in.read(reinterpret_cast<char*>(metadata), 4))
            return false;

        if (metadata[0] != 1)
            return false;

        // version 1 doesn't support any flags
        if (metadata[1] != 0 ||
            metadata[2] != 0 ||
            metadata[3] != 0)
            return false;

        if (!readUint64(in, fileSize))
            return false;

        if (!in.read(
                reinterpret_cast<char*>(expectedHash.data()),
                expectedHash.size()))
            return false;

        return true;
    }


};
