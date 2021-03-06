#include <iostream>

#include "bit_buffer.h"
#include "common_define.h"
#include "util.h"

using namespace std;

BitBuffer::BitBuffer(const string& data)
    :
    data_((uint8_t*)data.data()),
    bit_len_(data.length()*8),
    cur_pos_(0)
{
    cout << Util::Bin2Hex(data) << endl;
}

BitBuffer::BitBuffer(const uint8_t* data, const size_t& len)
    :
    data_(data),
    bit_len_(len*8),
    cur_pos_(0)
{
}

int BitBuffer::PeekBits(const size_t& bits, uint64_t& result)
{
    if (! MoreThanBits(bits))
    {
        cout << LMSG << "no more than " << bits << " bits" << endl;
        return -1;
    }

    result = 0;

    size_t pos = cur_pos_;

    for (size_t i = 0; i != bits; ++i)
    {
        result <<= 1;
        
        // XXX: static mask
        uint8_t mask = (0x01 << (7 - pos%8));

        cout << "pos:" << pos << ",mask:" << (uint16_t)mask << endl;

        if (data_[pos/8] & mask)
        {
            result |= 1;
        }

        ++pos;
    }

    return result;
}

int BitBuffer::GetString(const size_t& len, string& result)
{
    if (! MoreThanBytes(len))
    {
        cout << LMSG << "no more than " << len << " bytes" << endl;
        return -1;
    }

    if (cur_pos_ % 8 != 0)
    {
        cout << LMSG << "cur_pos_:" << cur_pos_ << " %8 != 0" << endl;
        return -1;
    }

    result.assign((const char*)data_ + (cur_pos_/8), len);
    cur_pos_ += len*8;

    return 0;
}
