#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

/*----------------------------------------------------------------------------
| The one fp8 rounding.  Infinite results saturate to the largest finite value
| when 'saturate'; otherwise E5M2 keeps the infinity and E4M3 gives the NaN 0x7F.
*----------------------------------------------------------------------------*/
static float8_t softfloat_f32ToF8( float32_t a, bool saturate )
{
    union ui32_f32 uA;
    uint_fast32_t uiA;
    bool sign, isE4M3;
    int_fast16_t exp, expOffset, expMax;
    uint_fast32_t frac;
    struct commonNaN commonNaN;
    uint_fast8_t uiZ, uiMax, uiInf, mantBits, shiftDist;
    uint_fast16_t hiddenBit, carryBit, sigOverflow, sig;
    uint_fast8_t roundingMode, roundIncrement, roundBits;
    bool roundNearEven, isTiny;
    union ui8_f8 uZ;

    isE4M3 = (softfloat_fp8Format == softfloat_fp8_e4m3);
    if ( isE4M3 ) {
        mantBits = 3;
        shiftDist = 16;
        hiddenBit = 0x80;
        expOffset = 0x79;
        expMax = 0x0E;
        sigOverflow = 0x0F;
    } else {
        mantBits = 2;
        shiftDist = 17;
        hiddenBit = 0x40;
        expOffset = 0x71;
        expMax = 0x1D;
        sigOverflow = 0x08;
    }
    carryBit = hiddenBit<<1;
    uA.f = a;
    uiA = uA.ui;
    sign = signF32UI( uiA );
    exp  = expF32UI( uiA );
    frac = fracF32UI( uiA );
    uiMax = ((uint_fast8_t) sign<<7) | (isE4M3 ? 0x7E : 0x7B);
    uiInf =
        isE4M3 ? defaultNaNF8E4M3UI : (((uint_fast8_t) sign<<7) | 0x7C);
    if ( exp == 0xFF ) {
        if ( frac ) {
            softfloat_f32UIToCommonNaN( uiA, &commonNaN );
            uiZ = softfloat_commonNaNToF8UI( &commonNaN );
        } else {
            uiZ = saturate ? uiMax : uiInf;
        }
        goto uiZ;
    }
    sig =
        frac>>shiftDist
            | ((frac & (((uint_fast32_t) 1<<shiftDist) - 1)) != 0);
    if ( ! (exp | sig) ) {
        uiZ = (uint_fast8_t) sign<<7;
        goto uiZ;
    }
    sig |= hiddenBit;
    exp -= expOffset;
    roundingMode = softfloat_roundingMode;
    roundNearEven = (roundingMode == softfloat_round_near_even);
    roundIncrement = 0x8;
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        roundIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                ? 0xF
                : 0;
    }
    roundBits = sig & 0xF;
    if ( exp < 0 ) {
        isTiny =
            (softfloat_detectTininess == softfloat_tininess_beforeRounding)
                || (exp < -1) || (sig + roundIncrement < carryBit);
        sig = softfloat_shiftRightJam32( sig, -exp );
        exp = 0;
        roundBits = sig & 0xF;
        if ( isTiny && roundBits ) {
            softfloat_raiseFlags( softfloat_flag_underflow );
        }
    }
    sig = (sig + roundIncrement)>>4;
    sig &= ~(uint_fast16_t) (! (roundBits ^ 8) & roundNearEven);
#ifdef SOFTFLOAT_ROUND_ODD
    if ( roundBits && (roundingMode == softfloat_round_odd) ) sig |= 1;
#endif
    if ( (expMax < exp) || ((exp == expMax) && (sigOverflow <= sig)) ) {
        softfloat_raiseFlags(
            softfloat_flag_overflow | softfloat_flag_inexact );
        uiZ = (saturate || ! roundIncrement) ? uiMax : uiInf;
        goto uiZ;
    }
    if ( roundBits ) softfloat_exceptionFlags |= softfloat_flag_inexact;
    if ( ! sig ) exp = 0;
    uiZ =
        ((uint_fast8_t) sign<<7) + ((uint_fast8_t) exp<<mantBits)
            + (uint_fast8_t) sig;
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

float8_t f32_to_f8( float32_t a )
{
    return softfloat_f32ToF8( a, false );
}
