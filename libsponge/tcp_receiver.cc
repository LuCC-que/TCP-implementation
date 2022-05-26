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

    if (first_segment && TCPHeader.syn) {
        first_segment = false;
        ISN = seg_seqno;
    }
    // bool not_first_seg_and_with_data = !first_segment && !data.length();
    abs_seqno = unwrap(seg_seqno, ISN, abs_seqno) + 1;

    _reassembler.push_substring(data, abs_seqno - 1, TCPHeader.fin);

    if (TCPHeader.fin) {
        abs_seqno++;
    }

    if (data.length()) {
        abs_seqno += data.length() - 1;
    }

    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (first_segment) {
        return {};
    }

    return wrap(abs_seqno, ISN);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().bytes_written(); }
