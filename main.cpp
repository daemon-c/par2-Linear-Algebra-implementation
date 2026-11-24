#include <iostream>
#include <fstream>
#include <iomanip>

#include "classes/archive.h"
#include "classes/bit_flip.h"


// Christian Valdez
// Driver program for our Linear group project. 
/**
 * @brief This program is not a novel idea, rather is a proof of concept and practical implementation of 7,4 hamming code that is not "encoding a message".
 * typical implementation focus on messages that live in volitile memory where the consequence of uncorrectable errors are grave for mission critical operations
 * You mainly see this in applications run by businesses, governments, or any other agency with the need for uptime and correctness to be at its maximum for operations to be going smoothly.
 * 
 * My implementation comes from a desire and interest in "homelabbing" where I simulate large IT infrastructure, services, ect in my own home. Part of these operations for me are long
 * term storage for important data. When storing data, in an archival sense, the data generally is not meant to be accessible quickly and might be compressed using a number of 
 * methods to save on hard drive space. In some environments, a form of hard drive setup if you will, called "RAID" is used for parity protection of long term storage. Raid in a very breif
 * and hi level explanation is taking say 3 1TB hard drives (each drive having 1TB worth of writable space on it), setting aside 2 drives to be normal data storage, and using the 3rd to
 * store calculated parity information. 
 * 
 * In a way, this is not feasible for some users to do, raid in this configuration allows a total of just 2TB of usable space. You use 3 drives but only get the capacity of two. 
 * You do get the added benefit of being able to loose one of the two drives but with prices the way they are, this is still quite the expense for a hobbyist homelabber.
 * 
 * As for this implementation of hamming code, I intend to demostrate "parchiving". As I explained this is not novel, it has been done with more advanced code (.par2 standard). 
 * In short: we could injest a file into the program, chunking it into 4 bits, taking those 4 bit chunks of 1s and 0s and generating 7 bit parity encoded chunks which we store in say a file
 * with the new and made up extension .laetf (linear albra encoded this file). If we did this to a file in order, it would be a elementary task to encode and then decode the file
 * file A = 1100 | 1010 | 0001 becomes -> 101-1100 | 111-1010 | 000-0001 (these are made up parity values, just for demonstration). in reverse order we would decode them and construct the 
 * file for use
 * 
 * as I stated earlier, files encoded this way will generally be sent to long term storage so accessing them quickly is not a main concern.
 * This would act as psuedo parity that a user could store on a mirror of drives, say 2 1TB drives. They would still have the benefit of 1TB of space, parity, for likely less money. This 
 * becomes more of an advantage as drive sizes increase . Again this is just a "hey I can do this thing with linear a. example"
 */


int main(int argc, char* argv[])
{
    if (argc == 3 && std::string(argv[1]) == "-bitflip")
    {
        std::cout << "this will permanently flip one random bit in "
                  << argv[2] << ". continue? [y/n] ";

        char answer;
        std::cin >> answer;
        if (answer != 'y' && answer != 'Y')
        {
            std::cout << "bit flip cancelled.\n";
            return 0;
        }

        BitFlipInfo flip;
        if (!BitFlipSimulator::flipRandomBitInPlace(argv[2], flip))
        {
            std::cerr << "failed to flip a bit in the archive.\n";
            return 1;
        }

        auto printBytes = [](const std::array<unsigned char, 4>& bytes,
                             std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(bytes[i]) << ' ';
            }
            std::cout << std::dec << '\n';
        };

        std::cout << "flipped archive byte " << flip.archiveOffset
                  << ", bit " << flip.bitFromMsb
                  << " from 0x" << std::hex << std::setw(2)
                  << std::setfill('0')
                  << static_cast<unsigned>(flip.oldByte)
                  << " to 0x" << std::setw(2)
                  << static_cast<unsigned>(flip.newByte) << std::dec << '\n';
        std::cout << "previous bytes: ";
        printBytes(flip.before, flip.beforeCount);
        std::cout << "following bytes: ";
        printBytes(flip.after, flip.afterCount);
        return 0;
    }

    if (argc != 4)
    {
        std::cerr << "usage: ./main -encode inputfile outputfile.laetf\n"
                  << "       ./main -decode archive.laetf recoveredfile\n"
                  << "       ./main -bitflip archive.laetf\n";
        return 1;
    }

    Archive archive;
    if (std::string(argv[1]) == "-decode")
    {
        if (!archive.decodeFile(argv[2], argv[3]))
        {
            std::cerr << "failed to decode archive";
            if (!archive.getLastDecodeError().empty())
                std::cerr << ": " << archive.getLastDecodeError();
            std::cerr << '\n';
            return 1;
        }

        std::cout << "file recovered: " << argv[3]
                  << ". corrected codewords: "
                  << archive.getCorrectedCodewords() << '\n';
        return 0;
    }

    if (std::string(argv[1]) != "-encode")
    {
        std::cerr << "unknown mode: " << argv[1] << '\n';
        return 1;
    }

    if (!archive.encodeFile(argv[2]))
    {
        std::cerr << "failed to encode input file.\n";
        return 1;
    }

    if (!archive.saveFile(argv[3]))
    {
        std::cerr << "failed to save archive.\n";
        return 1;
    }

    std::cout << "archive created: " << argv[3] << '\n';
    return 0;
}
