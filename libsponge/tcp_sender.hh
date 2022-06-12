#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    std::queue<TCPSegment> _outstanding_segments{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    unsigned int _applying_retransmission_timeout;
    size_t _last_tick_time = 0;
    bool _timing_begins = false;
    uint16_t consecutive_retransmissions_ = 0;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
    uint64_t _confirmed_seqno{0};

    // bool syn_ = false;
    bool fin_sent = false;
    uint64_t bytes_in_flight_ = 0;
    uint16_t _receiver_window_size = 0;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }

    TCPSegment read_a_segment() {
        TCPSegment temp = _segments_out.front();
        _segments_out.pop();
        return temp;
    }

    bool is_empty() const { return _segments_out.empty(); }

    void send_tcp_segment(TCPSegment &tcp_segment) {
        tcp_segment.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(tcp_segment);
        _outstanding_segments.push(tcp_segment);
        _next_seqno += tcp_segment.length_in_sequence_space();
        bytes_in_flight_ += tcp_segment.length_in_sequence_space();
        if (!_timing_begins) {
            _timing_begins = true;
            _last_tick_time = 0;
        }
    }

    size_t front_seg_outstanding_ackno() const {
        if (!_outstanding_segments.size()) {
            return 0;
        }
        TCPSegment front = _outstanding_segments.front();
        return unwrap(front.header().seqno, _isn, _next_seqno) + front.length_in_sequence_space();
    }

    size_t front_seg_outstanding_size() const {
        if (!_outstanding_segments.size()) {
            return 0;
        }

        return _outstanding_segments.front().length_in_sequence_space();
    }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
