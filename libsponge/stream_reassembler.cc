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
    : _output(capacity), _capacity(capacity), temporarily_descrete_data_storage() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    bool data_exceeded = false;
    size_t re_cap = _output.remaining_capacity();
    if (!re_cap) {
        // invalid cases
        return;
    }
    size_t indicator = first_unassemble_byte_index() >= _next_assembling_index
                           ? _capacity + first_unassemble_byte_index()
                           : _capacity + _next_assembling_index;
    if (index >= indicator) {
        return;
    }
    if (index == _next_assembling_index) {
        // good case
        // assembled_data_list.push_back(dg);
        _next_assembling_index += data.length() > re_cap ? re_cap : data.length();
        _output.write(data);

    }

    else {
        datagram dg{data, index, data.length(), eof};
        // missing_middle_handler
        if (index > _next_assembling_index) {
            missing_middle_handler(dg);
            return;

        }
        // overlap
        else if (index + data.length() > _next_assembling_index) {
            data_exceeded = overlap_handler(dg);
        }
        // skip the repeating case
    }

    bool gap_fixed = _next_assembling_index >= _smallest_temporty_index;
    bool data_exists_and_not_empty_list = data.length() && temporarily_descrete_data_storage.size();

    if (gap_fixed && data_exists_and_not_empty_list) {
        datagram write_in_data = temporarily_descrete_data_storage.front();
        temporarily_descrete_data_storage.pop_front();
        _unassembled_bytes -= write_in_data.size;
        StreamReassembler::push_substring(write_in_data._data, write_in_data.index, write_in_data.eof);
    }

    if (eof && !data_exceeded) {
        _output.end_input();
    }
}

void StreamReassembler::missing_middle_handler(datagram dg) {
    // bool overlap = false;

    if (temporarily_descrete_data_storage.size() > 0) {
        list<datagram>::iterator ptr = temporarily_descrete_data_storage.begin();
        for (; ptr != temporarily_descrete_data_storage.end(); ++ptr) {
            size_t ptr_tail = ptr->index + ptr->size;
            size_t dg_tail = dg.size + dg.index;

            if (dg == *ptr && ptr_tail == dg_tail) {
                return;
            }

            if (dg > *ptr && dg_tail > ptr_tail) {
                // discreate
                break;
            }

            if (dg > *ptr && dg < ptr_tail) {
                if (dg_tail <= ptr_tail) {
                    // repeating case
                    // ptr: *****
                    //  dg:   ***

                    // overlap = true;
                    return;

                } else {
                    // overlap
                    //  ptr: *****
                    //   dg:   ******
                    overlap_merger(*ptr, dg);
                    dg = ptr;
                    _unassembled_bytes -= ptr->size;
                    temporarily_descrete_data_storage.erase(ptr);
                    ptr = --temporarily_descrete_data_storage.begin();
                    // overlap = true;
                }
            } else if (dg < *ptr && *ptr < dg_tail) {
                if (ptr_tail <= dg_tail) {
                    // replace case
                    // ptr:  ***
                    //  dg: ******

                    _unassembled_bytes -= ptr->size;
                    temporarily_descrete_data_storage.erase(ptr);
                } else {
                    // merge case:
                    //  ptr:   ******
                    //   dg: ******
                    overlap_merger(dg, *ptr);
                    _unassembled_bytes -= ptr->size;
                    temporarily_descrete_data_storage.erase(ptr);
                }

                if (!temporarily_descrete_data_storage.size()) {
                    break;
                }
                ptr = --temporarily_descrete_data_storage.begin();  // may effect other

            } else if (dg == *ptr) {
                if (dg.size > ptr->size) {
                    // take the longer one:
                    // ptr: *****
                    // dg:  *******  <-take the longer one
                    _unassembled_bytes -= ptr->size;
                    temporarily_descrete_data_storage.erase(ptr);
                }
                if (!temporarily_descrete_data_storage.size()) {
                    break;
                }

                ptr = --temporarily_descrete_data_storage.begin();
            }
        }
    }

    temporarily_descrete_data_storage.push_front(dg);
    _unassembled_bytes += dg.size;
    temporarily_descrete_data_storage.sort([](const datagram a, const datagram b) { return a.index < b.index; });

    _smallest_temporty_index = temporarily_descrete_data_storage.begin()->index;
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
    size_t start_at = _next_assembling_index - new_dg.index;
    string::iterator ptr = new_dg._data.begin();
    advance(ptr, start_at);
    string non_overlap_part(ptr, new_dg._data.end());
    _output.write(non_overlap_part);
    _next_assembling_index += non_overlap_part.length();

    return _next_assembling_index > _output.bytes_written();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes(); }
