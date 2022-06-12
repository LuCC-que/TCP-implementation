#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // they should have the same distance in seqno or Aseqno

    WrappingInt32 rlt = wrap(checkpoint, isn);
    int32_t distance = n - rlt;
    int64_t Abs_seqno = checkpoint + distance;
    return Abs_seqno >= 0 ? Abs_seqno : Abs_seqno + (1UL << 32);

    // uint64_t tmp = 0;
    // uint64_t tmp1 = 0;
    // if (n - isn < 0) {
    //     tmp = uint64_t(n - isn + (1l << 32));
    // } else {
    //     tmp = uint64_t(n - isn);
    // }
    // if (tmp >= checkpoint)
    //     return tmp;
    // tmp |= ((checkpoint >> 32) << 32);
    // while (tmp <= checkpoint)
    //     tmp += (1ll << 32);
    // tmp1 = tmp - (1ll << 32);
    // return (checkpoint - tmp1 < tmp - checkpoint) ? tmp1 : tmp;
}
