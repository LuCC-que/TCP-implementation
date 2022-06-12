#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <iostream>
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
    , _stream(capacity) {
    _applying_retransmission_timeout = _initial_retransmission_timeout;
}

void TCPSender::fill_window() {
    if (!_next_seqno) {
        // first time
        // special case, plus 1
        TCPSegment seg;
        seg.header().syn = true;
        send_TCPSegment(seg);
        // ++_next_seqno;
        // ++_bytes_in_flight;
        return;
    }

    if (!reciever_logical_window_size) {
        return;
    }

    if (stream_in().input_ended() && _stream.eof() && !_fin_sent) {
        send_fin_seg();
        return;
    }

    while (!_stream.buffer_empty() && reciever_logical_window_size && !_fin_sent) {
        TCPSegment seg;
        size_t payload_size = min({_stream.buffer_size(),
                                   static_cast<size_t>(reciever_logical_window_size),
                                   static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});

        seg.payload() = _stream.read(payload_size);
        bool contain_fin = _stream.eof() && payload_size + 1 <= reciever_logical_window_size;
        seg.header().fin = contain_fin;
        send_TCPSegment(seg);

        if (contain_fin) {
            _fin_sent = contain_fin;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    reciever_window_size = window_size;
    reciever_logical_window_size = window_size;
    // bool initial_case = _next_seqno == 1 && confirm_seqno == _isn;
    // last_case_invalid = false;

    if (confirm_seqno >= ackno) {
        return;
    }

    if (window_size == 0) {
        reciever_logical_window_size++;
    }

    int size_c = static_cast<int>(front_outstanding_seg_size());
    int gap = ackno - confirm_seqno;

    bool invalid_cases = gap > static_cast<int>(_bytes_in_flight) || gap < size_c;

    if (invalid_cases) {
        // wrong ack
        // ackno - confirm_seqno - _bytes_in_flight is the impossible gap
        // in between ackno and sent bytes

        // if gap smaller than the first outstanding seg size, then it is outdated

        reciever_logical_window_size = 0;

        return;
    }

    if (stream_in().input_ended() && !_fin_sent) {
        // input has stop
        // if not more string to send then send a fin seg

        if (_stream.eof()) {
            send_fin_seg();
        } else {
            // otherwise
            fill_window();
        }
    }

    if (ackno > confirm_seqno) {
        // ack received
        // lots of bytes reveiced

        _applying_retransmission_timeout = _initial_retransmission_timeout;
        _last_tick_time = 0;
        _consecutive_retransmissions = 0;

        while (confirm_seqno < ackno) {
            confirm_outstanding_seg();
        }
    }

    if (reciever_logical_window_size) {
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _last_tick_time += ms_since_last_tick;
    size_t temp = _last_tick_time;

    if (_last_tick_time < _applying_retransmission_timeout) {
        return;
    } else {
        _last_tick_time %= _applying_retransmission_timeout;
    }

    if (!_outstanding_segments_out.size()) {
        return;
    }

    list<TCPSegment>::iterator it = _outstanding_segments_out.begin();
    list<TCPSegment>::iterator first_sent = it;
    temp -= _last_tick_time;

    do {
        send_TCPSegment(*it, true, true);
        ++_consecutive_retransmissions;
        ++it;

        if (it == _outstanding_segments_out.end()) {
            it = _outstanding_segments_out.begin();
        }
        temp -= _applying_retransmission_timeout;

    } while (temp > 0 && it != first_sent);

    if (reciever_window_size || _next_seqno == 1) {
        _applying_retransmission_timeout *= 2;
    }
    _last_tick_time = 0;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    send_TCPSegment(seg, true);
}

void TCPSender::send_fin_seg() {
    TCPSegment seg;
    seg.header().fin = true;
    send_TCPSegment(seg);

    _fin_sent = true;
}

void TCPSender::send_TCPSegment(TCPSegment &_seg, const bool &&outstanding, const bool &&resend) {
    if (!resend) {
        _seg.header().seqno = next_seqno();
    }
    _segments_out.push(_seg);
    size_t _seg_len = _seg.length_in_sequence_space();
    if (!outstanding) {
        _outstanding_segments_out.push_back(_seg);
        _bytes_in_flight += _seg_len;
        _next_seqno += _seg_len;
    }
    if (reciever_logical_window_size) {
        reciever_logical_window_size -= _seg_len;
    }
}

void TCPSender::confirm_outstanding_seg() {
    TCPSegment temp = _outstanding_segments_out.front();
    confirm_seqno = confirm_seqno + temp.length_in_sequence_space();
    _bytes_in_flight -= temp.length_in_sequence_space();
    _outstanding_segments_out.pop_front();
}
