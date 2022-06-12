#include "byte_stream.hh"

#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity{capacity} {}

size_t ByteStream::write(const string &data) {
    size_t remaining_capacity = _capacity - _buffer_size;
    if (remaining_capacity <= 0) {
        return 0;  // no _capacity left
    }

    size_t writeable_size = data.length() > remaining_capacity ? remaining_capacity : data.length();
    for (size_t i = 0; i < writeable_size; ++i) {
        _stream.push_back(data[i]);
        ++_buffer_size;
        ++_bytes_written;  // total bytes get wrote in
    }

    return writeable_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t peek_len = len > _buffer_size ? _buffer_size : len;
    list<char>::const_iterator ptr = _stream.begin();
    advance(ptr, peek_len);               // move to that char
    return string(_stream.begin(), ptr);  // return it
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = len > _buffer_size ? _buffer_size : len;
    list<char>::const_iterator ptr = _stream.begin();
    advance(ptr, pop_len);
    _stream.erase(_stream.begin(), ptr);
    _buffer_size -= pop_len;
    _bytes_read += pop_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const string read = peek_output(len);
    pop_output(len);
    return read;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _buffer_size; }

bool ByteStream::buffer_empty() const { return _stream.size() == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer_size; }
