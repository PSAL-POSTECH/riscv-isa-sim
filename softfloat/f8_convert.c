
#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

/*----------------------------------------------------------------------------
| Widening never rounds -- every fp8 value is exact in f16, f32 and f64 alike.
*----------------------------------------------------------------------------*/
float16_t f8_to_f16( float8_t a )
{
    return f32_to_f16( f8_to_f32( a ) );
}

float64_t f8_to_f64( float8_t a )
{
    return f32_to_f64( f8_to_f32( a ) );
}

/*----------------------------------------------------------------------------
| Narrowing.  f16 is exact in f32, so f16_to_f8 rounds once, in f32_to_f8.
| f64_to_f8 rounds at f32 first, which 24 >= 2p+2 makes innocuous.
*----------------------------------------------------------------------------*/
float8_t f16_to_f8( float16_t a )
{
    return f32_to_f8( f16_to_f32( a ) );
}

float8_t f64_to_f8( float64_t a )
{
    return f32_to_f8( f64_to_f32( a ) );
}

/*----------------------------------------------------------------------------
| bf16 is exact in f32, so bf16 -> fp8 rounds once, in f32_to_f8(_sat).
*----------------------------------------------------------------------------*/
float8_t bf16_to_f8( bfloat16_t a )
{
    return f32_to_f8( bf16_to_f32( a ) );
}

float8_t bf16_to_f8_sat( bfloat16_t a )
{
    return f32_to_f8_sat( bf16_to_f32( a ) );
}

/*----------------------------------------------------------------------------
| Exact and silent: every fp8 value fits bf16, and a NaN of either format,
| signaling E5M2 included, becomes the canonical bf16 NaN without raising.
*----------------------------------------------------------------------------*/
bfloat16_t f8_to_bf16( float8_t a )
{
    bfloat16_t z;

    if ( f8_classify( a ) & 0x300 ) {
        z.v = defaultNaNBF16UI;
        return z;
    }
    return f32_to_bf16( f8_to_f32( a ) );
}
