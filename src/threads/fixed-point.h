#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef int32_t fixed_point;

#define __f (1<<14)

// Convert n to fixed point:
#define INT_TO_FP( _n ) ((fixed_point) (_n * __f))

// Convert x to integer (rounding toward zero)
#define FP_TO_INT_TO_ZERO( _x ) (_x / __f)

// Convert x to integer (rounding to nearest)
#define FP_TO_INT_TO_NEAREST( _x ) ((_x >= 0) ? ((_x + __f / 2) / __f) : ((_x - __f / 2) / __f))

// Add x and y
#define FP_FP_ADD( _x, _y ) (_x + _y)

// Add FP_x and INT_n
#define FP_INT_ADD( _x, _n ) (_x + _n * __f)

// Subtract y from x
#define FP_FP_SUB( _x, _y )  (_x - _y)

// Subtract INT_n from FP_x
#define FP_INT_SUB( _x, _n )  (_x - _n * __f)

// Multiply FP_x by FP_y
#define FP_FP_MUL( _x, _y) ((fixed_point)(((int64_t) _x) * _y / __f))

// Multiply FP_x by INT_n
#define FP_INT_MUL( _x, _n ) (_x * _n)

// Divide FP_x by FP_y
#define FP_FP_DIV( _x, _y ) ((fixed_point)(((int64_t) _x) * __f / _y))

// Divide FP_x by INT_n
#define FP_INT_DIV( _x, _n ) (_x / _n)

#endif
