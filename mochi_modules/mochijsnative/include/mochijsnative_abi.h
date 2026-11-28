#ifndef MB_MOCHIJSNATIVE_ABI_H
#define MB_MOCHIJSNATIVE_ABI_H
#ifdef __cplusplus
extern "C" {
#endif

/* ABI for mochijsnative — engine plugin. Entry point: mbjs_init_module().
   Version 0.4.0 adds runtime state, atom, conversion, constructor, promise,
   symbol and property-descriptor accessors on top of the MBC1 handle ABI. */

#define MB_MOCHIJSNATIVE_ABI_VERSION "MBC1"
#define MB_MOCHIJSNATIVE_MODULE_VERSION "0.4.0"

#define MB_MOCHIJSNATIVE_CAP_ARRAY 1
#define MB_MOCHIJSNATIVE_CAP_OBJECT 1
#define MB_MOCHIJSNATIVE_CAP_ARRAYBUFFER 1
#define MB_MOCHIJSNATIVE_CAP_JIT 1

#ifdef __cplusplus
}
#endif
#endif
