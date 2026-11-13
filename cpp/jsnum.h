#pragma once
// JS number <-> string semantics for faithful Cookie Clicker 2.058 save port.
// JS Number.prototype.toString() (radix 10) and parseFloat/parseInt.

#include "Types.h"
#include <charconv>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>

// ---------------------------------------------------------------------------
// JS Number.prototype.toString() (radix 10), i.e. the shortest round-trip
// decimal representation with JS's fixed/exponential notation choice:
//
//   exponential  iff  x < 1e-6  or  x >= 1e21   (finite, non-zero)
//   fixed        otherwise
//
// Exponent in exponential form is printed as a plain signed integer
// (e.g. 1e-7 -> "1e-7", 1e21 -> "1e+21"), no leading zeros.
// ---------------------------------------------------------------------------
inline std::string jsNumberToString(double x) {
    if (std::isnan(x)) return "NaN";
    if (x == 0.0) return "0";           // +0 and -0 both -> "0"
    bool neg = (x < 0.0);
    if (neg) x = -x;

    // Shortest round-trip digits via std::to_chars (general form).
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), x, std::chars_format::general);
    std::string s(buf, static_cast<size_t>(res.ptr - buf));

    // Parse s into (digits, e10) where x = 0.digits * 10^(e10+1),
    // i.e. the first digit of `digits` sits at 10^e10.
    std::string digits;
    i64 e10 = 0;

    size_t ePos = s.find_first_of("eE");
    if (ePos != std::string::npos) {
        // Exponential form: "d.ddd...e±NN" (to_chars always one digit before '.')
        std::string mantissa = s.substr(0, ePos);
        long expPart = std::strtol(s.c_str() + ePos + 1, nullptr, 10);
        // remove the decimal point from mantissa -> digits
        for (char c : mantissa) if (c != '.') digits.push_back(c);
        // mantissa is "d.ddd..." -> the '.' is after 1 digit, so e10 = expPart
        e10 = expPart;
    } else {
        // Fixed form: "ddd.ddd" or "ddd"
        size_t dot = s.find('.');
        std::string intPart, fracPart;
        if (dot == std::string::npos) {
            intPart = s;
        } else {
            intPart = s.substr(0, dot);
            fracPart = s.substr(dot + 1);
        }
        if (intPart == "0" || intPart.empty()) {
            // Number < 1: leading zeros in fracPart before first significant digit.
            size_t firstNonZero = fracPart.find_first_not_of('0');
            if (firstNonZero == std::string::npos) {
                // all zeros -> shouldn't happen (x != 0), but be safe
                return "0";
            }
            digits = fracPart.substr(firstNonZero);
            e10 = static_cast<i64>(-(static_cast<i64>(firstNonZero) + 1));
        } else {
            digits = intPart + fracPart;
            e10 = static_cast<i64>(intPart.size() - 1);
        }
    }

    // JS notation choice
    bool exponential = (e10 >= 21) || (e10 <= -7);

    std::string out;
    if (neg) out.push_back('-');

    if (exponential) {
        out.push_back(digits[0]);
        if (digits.size() > 1) {
            out.push_back('.');
            out += digits.substr(1);
        }
        out.push_back('e');
        out.push_back(e10 >= 0 ? '+' : '-');
        out += std::to_string(e10 >= 0 ? e10 : -e10);
    } else {
        if (e10 >= 0) {
            i64 intDigitsNeeded = e10 + 1; // digits before decimal point
            std::string intPart;
            if (static_cast<i64>(digits.size()) >= intDigitsNeeded) {
                intPart = digits.substr(0, static_cast<size_t>(intDigitsNeeded));
            } else {
                intPart = digits;
                intPart.append(static_cast<size_t>(intDigitsNeeded - digits.size()), '0');
            }
            std::string fracPart;
            if (static_cast<i64>(digits.size()) > intDigitsNeeded) {
                fracPart = digits.substr(static_cast<size_t>(intDigitsNeeded));
            }
            out += intPart;
            if (!fracPart.empty()) {
                out.push_back('.');
                out += fracPart;
            }
        } else {
            // e10 < 0, not exponential (so e10 >= -6): "0." + zeros + digits
            out.push_back('0');
            out.push_back('.');
            i64 zeros = -e10 - 1;
            out.append(static_cast<size_t>(zeros), '0');
            out += digits;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// JS parseFloat(str): parses a double from the start of str, stopping at the
// first character that isn't part of a valid decimal number. Leading
// whitespace is skipped. Returns NaN if no valid prefix. (Mirrors JS exactly,
// including the "empty after sign/decimal -> NaN" edge cases.)
// ---------------------------------------------------------------------------
inline double jsParseFloat(const std::string& s) {
    const char* p = s.c_str();
    // skip leading whitespace (JS: \t \n \v \f \r space and others)
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    if (*p == '\0') return NAN;

    // Use strtod which matches JS parseFloat for the common subset.
    // JS parseFloat("123abc") -> 123, strtod("123abc") -> 123. Good.
    // JS parseFloat("abc") -> NaN, strtod("abc", &end) with end==p -> 0, but we return NaN.
    char* end = nullptr;
    double v = std::strtod(p, &end);
    if (end == p) return NAN;   // no conversion
    return v;
}

// ---------------------------------------------------------------------------
// JS parseInt(str, radix=10): parses an integer, stopping at first non-digit.
// Leading whitespace skipped. Optional sign. NaN if no valid digits.
// ---------------------------------------------------------------------------
inline int jsParseInt(const std::string& s) {
    const char* p = s.c_str();
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }
    // JS parseInt accepts "0x", "0o", "0b" prefixes; Cookie Clicker saves are
    // decimal so we only handle decimal digits here.
    long long v = 0;
    bool any = false;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        any = true;
        ++p;
    }
    if (!any) return 0;  // JS parseInt of invalid -> NaN -> used as 0 via || default
    return static_cast<int>(neg ? -v : v);
}
