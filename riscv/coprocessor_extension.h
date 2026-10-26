// See coprocessor_extension.cc.

#ifndef _RISCV_COPROCESSOR_EXTENSION_H
#define _RISCV_COPROCESSOR_EXTENSION_H

class extension_t;

// A new extension `coprocessor`. The simulator registers it under that name;
// it is built in, where every other extension comes from a library.
extension_t* coprocessor_extension();

#endif
