#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPConnection::segment_received(const TCPSegment &seg) {
    _last_tick_time = 0;
    if (seg.header().rst) {
        // set inbound and outboud stream to error
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
    //     seg.header().seqno == _receiver.ackno().value() - 1) {
    //     _sender.send_empty_segment();
    //     send_segments_out();
    //     return;
    // }

    _receiver.segment_received(seg);

    if (check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if (seg.header().ack) {
        // received ACK
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        send_segments_out();
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        bool is_sent = send_segments_out();

        if (!is_sent) {
            _sender.send_empty_segment();
            send_segments_out();
        }
    }
}

size_t TCPConnection::write(const string &data) {
    if (!data.length()) {
        return 0;
    }
    size_t result = _sender.stream_in().write(move(data));
    _sender.fill_window();
    send_segments_out();
    return result;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _last_tick_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    send_segments_out();

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _sender.send_empty_segment();
        TCPSegment seg = _sender.read_a_segment();
        seg.header().rst = true;
        _segments_out.push(seg);
        segment_received(seg);
    }

    if (check_inbound_ended() && check_outbound_ended()) {
        _active = !(!_linger_after_streams_finish || _last_tick_time >= 10 * _cfg.rt_timeout);
    }
}

bool TCPConnection::send_segments_out() {
    bool has_sent = false;
    while (!_sender.is_empty()) {
        has_sent = true;
        TCPSegment seg = _sender.read_a_segment();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = static_cast<uint16_t>(_receiver.window_size());
        _segments_out.push(seg);
    }
    return has_sent;
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _last_tick_time; }

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments_out();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segments_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
