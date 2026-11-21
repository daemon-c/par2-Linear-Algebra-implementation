#pragma once

#include <fstream>
#include <vector>
#include <string>
#include "data.h"

// fun fact, a nibble is the word for exactly 4 bits
// 8 bits is a byte

class Archive {
private:
    std::vector<bool> encodedBits; // vector here as we need a dynamically growing container of bits for any given file type
    // fun fact, the bool template here is a specialized vector which helps keep memory use down. It stores the bools as individual bits rather than reserving a full byte for each entry

    void encodeBinaryToBool(unsigned char nibble) // helper function to take bit chunks and turn them into bool representation that our logical operations need
    {
        Data encoder; // creates a Data object that holds our 4 input bits and performs the Hamming encoding

        // shift the bits to the right to move the bit we want into the last position.
        // & 1 isolates that bit by setting all the other bits to 0.
        // this lets us pass each of the 4 bits individually into our Data object.
        encoder.setData( 
            (nibble >> 3) & 1,
            (nibble >> 2) & 1,
            (nibble >> 1) & 1,
            nibble & 1
        );

        encoder.encodeData();

        std::array<bool, 7> result = encoder.getEncodedData();

        for (int i = 0; i < 7; i++)
        {
            encodedBits.push_back(result[i]); // loops over the resulting encoded data object and pushes each set of 7 encoded bits into the vector object
        }
    }


    bool correctEncodedData(std::array<bool, 7>& encodedData)
    {
        // based on parity bits being in the 3, 5, 6, 7 positions
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
            return true;
        }

        return false;
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



public:
    bool encodeFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary); 
        // opens the file in binary mode so we can read the raw bytes without text conversions

        if (!file.is_open())
        {
            return false;
        }

        char byte;

        while (file.get(byte))
        {
            unsigned char value = static_cast<unsigned char>(byte);
            
            // each byte contains 8 bits, but Hamming (7,4) only accepts 4 bits at a time.
            // we split the byte into two nibbles so each can be encoded separately.
            unsigned char upper = (value >> 4) & 0x0F;
            unsigned char lower = value & 0x0F;

            encodeBinaryToBool(upper);
            encodeBinaryToBool(lower);
        }

        return !file.bad();
    }


    bool decodeFile(const std::string& inputFilename, const std::string& outputFilename)
    {
        std::ifstream in(inputFilename, std::ios::binary);

        if (!in.is_open())
        {
            return false;
        }

        std::vector<bool> bits;

        char byte;

        // unpack archived bytes into individual bits.
        while (in.get(byte))
        {
            unsigned char value = static_cast<unsigned char>(byte);

            for (int i = 7; i >= 0; i--)
            {
                bits.push_back((value >> i) & 1);
            }
        }

        if (in.bad())
        {
            return false;
        }

        // every original byte generates 14 encoded bits.
        std::size_t originalBytes = bits.size() / 14;

        // validate that the file has a possible encoded size.
        std::size_t expectedBits = ((originalBytes * 14 + 7) / 8) * 8;

        if (expectedBits != bits.size())
        {
            return false;
        }

        std::ofstream out(outputFilename, std::ios::binary);

        if (!out.is_open())
        {
            return false;
        }

        std::size_t position = 0;

        for (std::size_t i = 0; i < originalBytes; i++)
        {
            std::array<bool, 7> upperData{};
            std::array<bool, 7> lowerData{};

            // extract the first seven-bit codeword.
            for (int j = 0; j < 7; j++)
            {
                upperData[j] = bits[position++];
            }

            // extract the second seven-bit codeword.
            for (int j = 0; j < 7; j++)
            {
                lowerData[j] = bits[position++];
            }

            // correct errors and recover four data bits each.
            unsigned char upper = decodeNibble(upperData);
            unsigned char lower = decodeNibble(lowerData);

            // combine the two nibbles into one original byte.
            unsigned char originalByte = (upper << 4) | lower;

            out.put(static_cast<char>(originalByte));
        }

        out.close();

        return out.good();
    }


    const std::vector<bool>& getEncodedBits() const
    {
        return encodedBits;
    }

    
    bool saveFile(const std::string& filename) const
    {
        std::ofstream out(filename, std::ios::binary);

        if (!out.is_open())
        {
            return false;
        }

        unsigned char currentByte = 0;
        int bitCount = 0;

        for (bool bit : encodedBits)
        {
            currentByte = (currentByte << 1) | bit;
            bitCount++;

            if (bitCount == 8)
            {
                out.put(static_cast<char>(currentByte));

                currentByte = 0;
                bitCount = 0;
            }
        }

        // pad the remaining bits with zeros.
        if (bitCount > 0)
        {
            currentByte <<= (8 - bitCount);
            out.put(static_cast<char>(currentByte));
        }

        out.close();
        return out.good();
    }

};