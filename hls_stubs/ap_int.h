// Minimal stubs for Xilinx ap_int.h / ap_ufixed — native C++ compilation only.
// Arithmetic and sign-extension semantics match Vitis HLS for widths <= 64 bits.
#pragma once
#include <cstdint>
#include <type_traits>

// Forward declaration needed for cross-type constructors
template<int W> struct ap_uint;

// ---------------------------------------------------------------------------
// ap_int<W>: signed, W-bit integer
// ---------------------------------------------------------------------------
template<int W>
struct ap_int {
    int64_t val;

    static int64_t _sext(int64_t v) {
        if (W >= 64) return v;
        const int64_t mask = (int64_t(1) << W) - 1;
        v &= mask;
        if (v & (int64_t(1) << (W - 1))) v |= ~mask;
        return v;
    }

    ap_int() : val(0) {}

    // Single arithmetic constructor via template (avoids overload ambiguity)
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    ap_int(T v) : val(_sext(static_cast<int64_t>(v))) {}

    // Cross-width conversions (signed ↔ unsigned)
    template<int W2> ap_int(const ap_int<W2>&  o) : val(_sext(o.val)) {}
    template<int W2> ap_int(const ap_uint<W2>& o) : val(_sext(static_cast<int64_t>(o.val))) {}

    // Single implicit conversion — let callers cast via int64_t
    operator int64_t() const { return val; }

    ap_int  operator+(const ap_int& o) const { return ap_int(val + o.val); }
    ap_int  operator-(const ap_int& o) const { return ap_int(val - o.val); }
    ap_int  operator*(const ap_int& o) const { return ap_int(val * o.val); }
    ap_int& operator+=(const ap_int& o)      { val = _sext(val + o.val); return *this; }
    ap_int  operator&(int64_t m) const       { return ap_int(val & m); }
    ap_int  operator|(int64_t m) const       { return ap_int(val | m); }
    ap_int  operator>>(int s) const          { return ap_int(val >> s); }
    ap_int  operator~() const                { return ap_int(~val); }

    bool operator==(const ap_int& o) const { return val == o.val; }
    bool operator!=(const ap_int& o) const { return val != o.val; }
    bool operator< (const ap_int& o) const { return val <  o.val; }
    bool operator<=(const ap_int& o) const { return val <= o.val; }
    bool operator> (const ap_int& o) const { return val >  o.val; }
    bool operator>=(const ap_int& o) const { return val >= o.val; }
};

// ---------------------------------------------------------------------------
// ap_uint<W>: unsigned, W-bit integer
// ---------------------------------------------------------------------------
template<int W>
struct ap_uint {
    uint64_t val;

    static uint64_t _mask() { return (W < 64) ? ((uint64_t(1) << W) - 1) : UINT64_MAX; }

    ap_uint() : val(0) {}

    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    ap_uint(T v) : val(static_cast<uint64_t>(static_cast<int64_t>(v)) & _mask()) {}

    template<int W2> ap_uint(const ap_uint<W2>& o) : val(o.val & _mask()) {}
    template<int W2> ap_uint(const ap_int<W2>&  o) : val(static_cast<uint64_t>(o.val) & _mask()) {}

    // Single conversion to uint64_t; explicit int64_t conversion for signed contexts
    operator uint64_t() const { return val; }

    ap_uint  operator+(const ap_uint& o) const { return ap_uint(val + o.val); }
    ap_uint  operator-(const ap_uint& o) const { return ap_uint(val - o.val); }
    ap_uint& operator+=(const ap_uint& o)      { val = (val + o.val) & _mask(); return *this; }
    ap_uint  operator&(uint64_t m) const       { return ap_uint(val & m); }
    ap_uint  operator|(uint64_t m) const       { return ap_uint(val | m); }
    ap_uint  operator>>(int s) const           { return ap_uint(val >> s); }
    ap_uint  operator<<(int s) const           { return ap_uint(val << s); }
    ap_uint  operator~() const                 { return ap_uint(~val & _mask()); }

    bool operator==(const ap_uint& o) const { return val == o.val; }
    bool operator!=(const ap_uint& o) const { return val != o.val; }
    bool operator< (const ap_uint& o) const { return val <  o.val; }
    bool operator<=(const ap_uint& o) const { return val <= o.val; }
    bool operator> (const ap_uint& o) const { return val >  o.val; }
    bool operator>=(const ap_uint& o) const { return val >= o.val; }
    // Compare against plain int/long without ambiguity
    bool operator==(int  o) const { return val == (uint64_t)(int64_t)o; }
    bool operator!=(int  o) const { return val != (uint64_t)(int64_t)o; }
    bool operator==(long o) const { return val == (uint64_t)(int64_t)o; }
};

// ---------------------------------------------------------------------------
// ap_ufixed<W, I>: unsigned fixed-point, I integer bits, W-I fractional bits
// ---------------------------------------------------------------------------
template<int W, int I>
struct ap_ufixed {
    static constexpr int      FRAC  = W - I;
    static constexpr uint64_t SCALE = (uint64_t(1) << FRAC);
    uint64_t raw;

    ap_ufixed() : raw(0) {}

    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    ap_ufixed(T v) : raw(static_cast<uint64_t>(static_cast<double>(v) * SCALE)) {}

    static ap_ufixed from_raw(uint64_t r) { ap_ufixed f; f.raw = r; return f; }

    ap_ufixed  operator+ (const ap_ufixed& o) const { return from_raw(raw + o.raw); }
    ap_ufixed& operator+=(const ap_ufixed& o)       { raw += o.raw; return *this; }
    ap_ufixed  operator~ ()                   const { return from_raw(~raw); }

    bool operator< (const ap_ufixed& o) const { return raw < o.raw; }
    bool operator> (const ap_ufixed& o) const { return raw > o.raw; }
    bool operator==(const ap_ufixed& o) const { return raw == o.raw; }

    int    to_int()    const { return (int)(raw >> FRAC); }
    double to_double() const { return (double)raw / SCALE; }
};
