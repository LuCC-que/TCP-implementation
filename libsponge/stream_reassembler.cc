#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), assembled_data_list(), temporay_data_list() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    datagram dg{data, index, data.length(), eof};
    bool overlap = false;

    if (dg == _smallest_assembled_index) {
        // good case
        assembled_data_list.push_back(dg);
        _smallest_assembled_index += dg.size;
    } else if (dg > _smallest_assembled_index) {
        // missing_middle_handler
        missing_middle_handler(dg);
    } else {
        // repeat or overlap datagram;

        if (index + dg.size > _smallest_assembled_index) {
            overlap = true;
            overlap_handler(dg);
        }

        // ignore repeat case
    }
    size_t size = assembled_data_list.size();
    for (; size > 0 && !overlap; --size) {
        datagram write_in_data = assembled_data_list.front();
        _output.write(write_in_data._data);
        assembled_data_list.pop_front();

        if (write_in_data.eof) {
            _output.input_ended();
        }
    }

    if (_smallest_assembled_index == _smallest_temporty_index) {
        datagram write_in_data = assembled_data_list.front();
        StreamReassembler::push_substring(write_in_data._data, write_in_data.index, write_in_data.eof);
        assembled_data_list.pop_front();
    }
}

void StreamReassembler::missing_middle_handler(datagram dg) {
    if (dg >= _smallest_temporty_index) {
        temporay_data_list.push_back(dg);
    } else {
        temporay_data_list.push_front(dg);
    }

    // update the smallest temporay index all the time
    _smallest_temporty_index = temporay_data_list.begin()->index;
}

void StreamReassembler::overlap_handler(datagram new_dg) {
    size_t start_at = _smallest_assembled_index - new_dg.index;
    string non_overlap_part = "";
    for (; start_at < new_dg.size; start_at++) {
        non_overlap_part += new_dg._data[start_at];
    }
    _output.write(non_overlap_part);
    _smallest_assembled_index += non_overlap_part.length();

    return;
}

size_t StreamReassembler::unassembled_bytes() const { return 0; }

bool StreamReassembler::empty() const { return unassembled_bytes(); }
