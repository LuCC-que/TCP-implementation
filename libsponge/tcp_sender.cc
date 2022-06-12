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
        ++_next_seqno;
        ++_bytes_in_flight;
        return;
    }

    if (!reciever_logical_window_size) {
        return;
    }

    if (stream_in().input_ended() && _stream.eof()) {
        send_fin_seg();
        return;
    }

    if (!_stream.buffer_empty()) {
        while (reciever_logical_window_size) {
            TCPSegment seg;
            size_t payload_size = min({_stream.buffer_size(),
                                       static_cast<size_t>(reciever_logical_window_size),
                                       static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});

            // if (stream_in().input_ended() && payload_size >= _stream.buffer_size() &&
            //     payload_size + 1 > reciever_logical_window_size) {
            //     return;
            // }
            seg.payload() = _stream.read(payload_size);
            if (_stream.eof() && payload_size + 1 <= reciever_logical_window_size) {
                seg.header().fin = true;
                _fin_sent = true;
                --reciever_logical_window_size;
            }

            // if (!stream_in().input_ended()) {
            //     seg.payload() = _stream.read(payload_size);
            // }

            // if (payload_size >= _stream.buffer_size()) {
            //     size_t temp_p = payload_size + 1;
            //     bool is_greater = temp_p > reciever_logical_window_size;
            //     if (is_greater) {
            //         return;  // reciever cannot contain this additional bit
            //     }

            //     seg.header().fin = true;
            //     _fin_sent = true;
            // }

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
    reciever_window_size = ackno - _isn;
    reciever_logical_window_size = window_size;
    // bool initial_case = _next_seqno == 1 && confirm_seqno == _isn;

    if (confirm_seqno >= ackno) {
        return;
    }

    bool invalid_cases = (ackno - confirm_seqno) > static_cast<int>(_bytes_in_flight) ||
                         ackno - _isn <= static_cast<int>(front_outstanding_seg_size());

    if (invalid_cases) {
        // wrong ack
        // ackno - confirm_seqno - _bytes_in_flight is the impossible gap
        // in between ackno and sent bytes
        // reciever_logical_window_size = 0;
        reciever_logical_window_size = 0;
        return;
    }

    // if (ackno - _isn < _outstanding_segments_out.front().payload().size()) {
    //     return;
    // }

    if (stream_in().input_ended()) {
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
        // } else if (initial_case) {
        //     confirm_outstanding_seg();
        //     confirm_seqno = confirm_seqno + 1;
        //     --_bytes_in_flight;
        // }
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
    // advance(it, _next_resent_index);
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

    _applying_retransmission_timeout *= 2;
}

void TCPSender::send_empty_segment() {}

void TCPSender::send_fin_seg() {
    if (_fin_sent) {
        return;
    }
    TCPSegment seg;
    seg.header().fin = true;
    send_TCPSegment(seg);
    ++_next_seqno;
    ++_bytes_in_flight;
    --reciever_logical_window_size;
    _fin_sent = true;
}

void TCPSender::send_TCPSegment(TCPSegment &_seg, bool &&outstanding, bool &&resend) {
    if (!resend) {
        _seg.header().seqno = next_seqno();
    }
    _segments_out.push(_seg);
    if (!outstanding) {
        _outstanding_segments_out.push_back(_seg);
        _bytes_in_flight += _seg.payload().size();
    }
}

void TCPSender::confirm_outstanding_seg() {
    TCPSegment temp = _outstanding_segments_out.front();
    size_t payload_size = temp.payload().size();
    confirm_seqno = confirm_seqno + temp.payload().size();
    _bytes_in_flight -= temp.payload().size();
    if (!payload_size && (temp.header().fin || temp.header().syn)) {
        confirm_seqno = confirm_seqno + 1;
        --_bytes_in_flight;
    }

    _outstanding_segments_out.pop_front();
}
