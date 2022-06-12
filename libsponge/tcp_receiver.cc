#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    string data = seg.payload().copy();
    TCPHeader TCPHeader = seg.header();
    WrappingInt32 seg_seqno = TCPHeader.seqno;

    if (first_segment && !TCPHeader.syn) {
        // received segment_before_SYN
        return;
    }

    if (first_segment && TCPHeader.syn) {
        first_segment = false;
        ISN = seg_seqno;
    }

    // if ((seg_seqno.raw_value() - ISN.raw_value()) + data.length() >= _capacity) {
    //     return;
    // }
    // bool not_first_seg_and_with_data = !first_segment && !data.length();
    uint64_t temp_abs_seqno = unwrap(seg_seqno, ISN, abs_seqno);

    if ((abs_seqno == 1) && (temp_abs_seqno < abs_seqno)) {
        // the syn have to take a index
        // the second seg cant not occupy that
        // otherwise it is invalid
        return;
    }

    size_t index = temp_abs_seqno > 0 ? temp_abs_seqno - 1 : temp_abs_seqno;

    _reassembler.push_substring(data, index, TCPHeader.fin);

    if (temp_abs_seqno != abs_seqno) {
        // tcp miss!
        return;
    }

    if (data.length()) {
        size_t len = _reassembler.stream_out().bytes_written();
        abs_seqno = len + 1;  // reset

    } else if (temp_abs_seqno == 0) {
        // both syn and first seg occupy a bit
        ++abs_seqno;
    }

    if (TCPHeader.fin || _reassembler.stream_out().input_ended()) {
        ++abs_seqno;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (first_segment) {
        return {};
    }

    return wrap(abs_seqno, ISN);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
