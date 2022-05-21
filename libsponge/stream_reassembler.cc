#include "stream_reassembler.hh"

#include <iostream>

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
    bool data_exceeded = false;
    if (index == _smallest_assembled_index) {
        // good case
        // assembled_data_list.push_back(dg);
        _smallest_assembled_index += data.length();
        _output.write(data);

    }

    else {
        datagram dg{data, index, data.length(), eof};
        // missing_middle_handler
        if (index > _smallest_assembled_index) {
            missing_middle_handler(dg);
            return;

        }
        // overlap
        else if (index + data.length() > _smallest_assembled_index) {
            data_exceeded = overlap_handler(dg);
        }
        // skip the repeating case
    }
    bool gap_fixed = _smallest_assembled_index >= _smallest_temporty_index;
    bool data_exists_and_not_empty_list = data.length() && temporay_data_list.size();

    if (gap_fixed && data_exists_and_not_empty_list) {
        datagram write_in_data = temporay_data_list.front();
        temporay_data_list.pop_front();
        _unassembled_bytes -= write_in_data.size;
        StreamReassembler::push_substring(write_in_data._data, write_in_data.index, write_in_data.eof);
    }
    if (eof && !data_exceeded) {
        _output.end_input();
    }
}

void StreamReassembler::missing_middle_handler(datagram dg) {
    bool overlap = false;

    if (temporay_data_list.size() > 0) {
        list<datagram>::iterator ptr = temporay_data_list.begin();
        for (; ptr != temporay_data_list.end(); ++ptr) {
            if (dg > *ptr) {
                size_t covered = ptr->index + ptr->size;
                if (dg.index + dg.size <= covered) {
                    // front overlap
                    _unassembled_bytes -= ptr->size;
                    temporay_data_list.erase(ptr);
                    _unassembled_bytes += dg.size;
                    temporay_data_list.push_back(dg);
                    overlap = true;

                } else {
                    _unassembled_bytes += overlap_merger(*ptr, dg);
                    overlap = true;
                }
            } else if (dg < *ptr) {
                size_t covered = dg.size + dg.index;

                if (covered <= ptr->index + ptr->size) {
                    // latter overlap

                    _unassembled_bytes += overlap_merger(dg, *ptr);
                    overlap = true;
                }

            } else {
                if (dg.size > ptr->size) {
                    _unassembled_bytes -= ptr->size;
                    temporay_data_list.erase(ptr);
                    _unassembled_bytes += dg.size;
                    temporay_data_list.push_back(dg);
                    overlap = true;
                }
            }
        }
        // } else if (temporay_data_list.size() == 1) {
        //     datagram dg_temp = *temporay_data_list.begin();
        //     if (dg == dg_temp && dg.size > dg_temp.size) {
        //         overlap = true;
        //         temporay_data_list.pop_back();
        //         temporay_data_list.push_back(dg);
        //         _unassembled_bytes += dg.size - dg_temp.size;
        //     } else if (dg > dg_temp) {
        //         if (dg < dg_temp.index + dg_temp.size) {
        //             overlap = true;
        //             temporay_data_list.pop_back();
        //             _unassembled_bytes += overlap_merger(dg_temp, dg);
        //             temporay_data_list.push_back(dg_temp);
        //         }
        //     } else {
        //         if (dg_temp < dg.index + dg.size) {
        //             overlap = true;
        //             temporay_data_list.pop_back();
        //             _unassembled_bytes += overlap_merger(dg, dg_temp);
        //             temporay_data_list.push_back(dg);
        //         }
        //     }
    }
    if (!overlap) {
        temporay_data_list.push_front(dg);
        _unassembled_bytes += dg.size;
    }
    temporay_data_list.sort([](const datagram a, const datagram b) { return a.index < b.index; });

    _smallest_temporty_index = temporay_data_list.begin()->index;
}

size_t StreamReassembler::overlap_merger(datagram &dg1, datagram &dg2) {
    size_t start_at = dg1.index + dg1.size - dg2.index;
    if (!start_at) {
        return 0;
    }
    string::iterator ptr = dg2._data.begin();
    advance(ptr, start_at);
    string non_overlap_part(ptr, dg2._data.end());

    dg1._data += non_overlap_part;
    dg1.size += non_overlap_part.size();
    return non_overlap_part.size();
}

bool StreamReassembler::overlap_handler(datagram new_dg) {
    size_t start_at = _smallest_assembled_index - new_dg.index;
    string::iterator ptr = new_dg._data.begin();
    advance(ptr, start_at);
    string non_overlap_part(ptr, new_dg._data.end());
    _output.write(non_overlap_part);
    _smallest_assembled_index += non_overlap_part.length();

    return _smallest_assembled_index > _output.bytes_written();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes(); }
