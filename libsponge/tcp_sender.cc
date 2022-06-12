#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    , _applying_retransmission_timeout(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }

void TCPSender::fill_window() {
    // fin has sent, we done!
    if (fin_sent) {
        return;
    }

    TCPSegment seg;
    // initia case
    if (!_next_seqno) {
        seg.header().syn = true;
        send_tcp_segment(seg);
        return;
    }

    // if the target has window size 0, we still need to send a segment to try.
    uint16_t receiver_logical_window_size = !_receiver_window_size ? 1 : _receiver_window_size;

    if (_confirmed_seqno + receiver_logical_window_size <= _next_seqno) {
        /* confirmed seqno is what we know has recievid, if that plus target window size
        still smaller than what we has sent, then target window must be full;
        */
        return;
    }

    // otherwise, receiver side has space to take in the data

    if (_stream.eof()) {
        seg.header().fin = true;
        send_tcp_segment(seg);
        fin_sent = true;
        return;
    }

    size_t available_window = static_cast<size_t>(receiver_logical_window_size - (_next_seqno - _confirmed_seqno));
    while (!_stream.buffer_empty() && available_window) {
        size_t payload_size = min({_stream.buffer_size(),

                                   available_window,

                                   static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});

        seg.payload() = _stream.read(payload_size);

        if (_stream.eof() && seg.length_in_sequence_space() < available_window) {
            seg.header().fin = true;
            fin_sent = true;
        }

        send_tcp_segment(seg);
        available_window -= seg.length_in_sequence_space();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timing_begins) {
        return;
    }

    _last_tick_time += ms_since_last_tick;

    if (_last_tick_time >= _applying_retransmission_timeout && !_outstanding_segments.empty()) {
        TCPSegment tcp_segment = _outstanding_segments.front();
        _segments_out.push(tcp_segment);

        _last_tick_time = 0;

        if (_receiver_window_size > 0 || tcp_segment.header().syn) {
            consecutive_retransmissions_++;
            _applying_retransmission_timeout *= 2;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);

    if (_confirmed_seqno > abs_ackno || abs_ackno > _next_seqno) {
        // if _confirmed_seqno > abs_ackno, then it is old ack
        // if abs_ackno > _next_seqno, then it is invalid ack
        return;
    }

    _confirmed_seqno = abs_ackno;
    _receiver_window_size = window_size;

    bool ack = false;
    while (!_outstanding_segments.empty()) {
        if (abs_ackno < front_seg_outstanding_ackno()) {
            return;
        }
        if (!ack) {
            _applying_retransmission_timeout = _initial_retransmission_timeout;
            consecutive_retransmissions_ = 0;
            _last_tick_time = 0;
            ack = true;
        }
        bytes_in_flight_ -= front_seg_outstanding_size();
        _outstanding_segments.pop();
    }

    if (ack) {
        fill_window();
    }

    _timing_begins = !_outstanding_segments.empty();
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutive_retransmissions_; }

void TCPSender::send_empty_segment() {
    TCPSegment tcp_segment;
    tcp_segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(tcp_segment);
}
