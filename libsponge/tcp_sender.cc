#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <random>
#include <time.h>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

void TCPSender::fill_window() {
    _last_sent_segment_time = static_cast<unsigned int>(time(NULL));

    if (!_next_seqno) {
        // first time
        // special case, plus 1
        TCPSegment seg;
        seg.header().syn = true;
        send_TCPSegment(seg);
        ++_next_seqno;
        return;
    }

    if (reciever_logical_window_size) {
        while (reciever_logical_window_size) {
            TCPSegment seg;
            size_t payload_size = min({_stream.buffer_size(),
                                       static_cast<size_t>(reciever_logical_window_size),
                                       static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});
            seg.payload() = _stream.read(payload_size);
            if (_stream.eof()) {
                seg.header().fin = true;
            }
            send_TCPSegment(seg);
            _next_seqno += payload_size;
            reciever_logical_window_size -= payload_size;

            if (_stream.buffer_empty() || seg.header().fin) {
                break;
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    reciever_window_size = window_size;
    reciever_logical_window_size = window_size;
    bool initial_case = bytes_in_flight() == 1;

    if (ackno >= confirm_seqno && !initial_case) {
        // ack received
        // lots of bytes reveiced

        while (confirm_seqno < ackno) {
            confirm_outstanding_seg();
        }
    } else if (initial_case) {
        confirm_outstanding_seg();
        confirm_seqno = confirm_seqno + 1;
    }

    // else if (ackno < temp) {
    //     // segment missed
    //     list::iterator<TCPSegment> it_seg = _outstanding_segments_out.begin();
    //     while (reciever_logical_window_size && it_seg->payload().size() > reciever_logical_window_size &&
    //            it_seg != _outstanding_segments_out.end()) {
    //         reciever_logical_window_size -= it_seg->payload().size();
    //         send_TCPSegment(temp);
    //         ++it_seg;
    //     }

    //     return;
    // }

    if (!reciever_window_size) {
        TCPSegment seg;
        _last_sent_segment_time = static_cast<unsigned int>(time(NULL));
        if (_stream.eof()) {
            seg.header().fin = true;
            send_TCPSegment(seg);
        } else if (!_stream.buffer_empty()) {
            seg.payload() = _stream.read(1);
            send_TCPSegment(seg);
        }
    }

    if (reciever_logical_window_size) {
        // if there are still some space in reciever window
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    list<TCPSegment>::iterator it = _outstanding_segments_out.begin();
    size_t temp = ms_since_last_tick;
    while (temp > 0 && it != _outstanding_segments_out.end()) {
        send_TCPSegment(*it, true);
        ++it;
        temp -= _initial_retransmission_timeout;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}

void TCPSender::send_TCPSegment(TCPSegment &_seg, bool outstanding) {
    _seg.header().seqno = next_seqno();
    _segments_out.push(_seg);
    if (!outstanding) {
        _outstanding_segments_out.push_back(_seg);
    }

    // _outstanding_segments_out.sort(
    //     [](const TCPSegment a, const TCPSegment b) { return a.header().seqno < b.header().seqno; });
}

void TCPSender::confirm_outstanding_seg() {
    TCPSegment temp = _outstanding_segments_out.front();
    confirm_seqno = confirm_seqno + temp.payload().size();
    _outstanding_segments_out.pop_front();

    // if (_segments_out.size()) {
    //     _segments_out.pop();
    // }

    if (!_outstanding_segments_out.size()) {
        _last_sent_segment_time = 0;
    }
}
