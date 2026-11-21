#pragma once
// object that stores original message and includes logic to encore the message




class Data {
    private:
        // pre defining the size 4 here since 74 hamming codes takes 4 bits of data and make a encoded vecotr of data with it. 
        // Note to self: we will be taking the bool values and turning them to digits somehow
        bool data[4] = {0, 0, 0, 0};

        // traditional generator matrix style "G"
        bool encoderMatrix[4][7] = {    
            {1, 1, 1, 0, 0, 0, 0},
            {1, 0, 0, 1, 1, 0, 0},
            {0, 1, 0, 1, 0, 1, 0},
            {1, 1, 0, 1, 0, 0, 1}
        };

        bool encodedData[7] = {0, 0, 0, 0, 0, 0, 0};
        
    public:
        // default constructor.
        Data() = default;

        // set data to sepcified values
        void setData(bool one, bool two, bool three, bool four)
        {
            this->data[0] = one;
            this->data[1] = two;
            this->data[2] = three;
            this->data[3] = four;
        }

        void encodeData()
        {
            // outer loop moves once every full run of inner loop here, encoding message using encoderMatrix.
            for (int col = 0; col < 7; col++)
            {
                this->encodedData[col] = false;

                for (int row = 0; row < 4; row++)
                {
                    // && is logical AND, equivalent to multiplcation for bool bits
                    // ^= is XOR assignment, equivalent to addition modulo 2 (if the matrix used ints and not bools we would need modulo to make sure resulting values are only 0 or 1.)
                    this->encodedData[col] ^= this->data[row] && this->encoderMatrix[row][col];
                }
            }
        }

        const bool* getData()
        {
            return this->data;
        }

        std::array<bool, 7> getEncodedData()
        {
            std::array<bool, 7> result{};

            for (int i = 0; i < 7; i++)
            {
                result[i] = encodedData[i];
            }

            return result;
        }



};