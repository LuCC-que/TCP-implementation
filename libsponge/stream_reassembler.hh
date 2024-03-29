#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <list>
#include <string>
using namespace std;
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    size_t _next_assembling_index = 0;
    size_t _smallest_temporty_index = 0;
    size_t _unassembled_bytes = 0;

    struct datagram {
        string _data;
        size_t index;
        size_t size;
        bool eof;

        // operator overflow
        bool operator>(const size_t &rhs) const { return this->index > rhs; }
        bool operator>(const datagram &rhs) const { return this->index > rhs.index; }

        bool operator>=(const size_t &rhs) const { return this->index >= rhs; }

        bool operator>=(const datagram &rhs) const { return this->index >= rhs.index; }
        bool operator<=(const datagram &rhs) const { return this->index <= rhs.index; }

        bool operator<(const size_t &rhs) const { return this->index < rhs; }
        bool operator<(const datagram &rhs) const { return this->index < rhs.index; }

        bool operator==(const size_t &rhs) const { return this->index == rhs; }
        bool operator==(const datagram &rhs) const { return this->index == rhs.index; }

        size_t operator-(const datagram &rhs) const { return this->index - rhs.index; };

        datagram &operator=(datagram &rhs) {
            this->index = rhs.index;
            this->size = rhs.size;
            this->_data = rhs._data;
            this->eof = rhs.eof;

            delete &rhs;
            return *this;
        }

        datagram &operator=(list<datagram>::iterator rhs) {
            this->index = rhs->index;
            this->size = rhs->size;
            this->_data = rhs->_data;
            this->eof = rhs->eof;

            return *this;
        }
    };
    list<datagram> temporarily_descrete_data_storage;

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    void missing_middle_handler(datagram dg);

    bool overlap_handler(datagram new_dg);

    size_t overlap_merger(datagram &dg1, datagram &dg2);
    size_t first_unassemble_byte_index() const {
        if (!temporarily_descrete_data_storage.size()) {
            return 0;
        }
        return temporarily_descrete_data_storage.front().index;
    };

    bool input_ended() const { return _output.input_ended(); }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
