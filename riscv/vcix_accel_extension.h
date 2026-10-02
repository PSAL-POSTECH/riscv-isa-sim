// See vcix_accel_extension.cc.

#ifndef _RISCV_VCIX_ACCEL_EXTENSION_H
#define _RISCV_VCIX_ACCEL_EXTENSION_H

class extension_t;

// A new extension `vcixaccel`. The simulator registers it under that name;
// it is built in, where every other extension comes from a library.
extension_t* vcix_accel_extension();

#endif
