// types.h
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

enum class OptionType : uint8_t { Call = 0, Put = 1 };

struct BSParams { 
    double S, K, r, sigma, T; 
    OptionType type; 
    short steps = 0; 
};

struct BSResult { 
    double price, delta, vega; 
    
    BSResult(double p = 0.0, double d = 0.0, double v = 0.0) 
        : price(p), delta(d), vega(v) {}
};

#endif