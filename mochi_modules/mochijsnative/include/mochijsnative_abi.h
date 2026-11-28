#ifndef MB_MOCHIJSNATIVE_ABI_H
#define MB_MOCHIJSNATIVE_ABI_H
#ifdef __cplusplus
extern "C" {
#endif

/* ABI for mochijsnative — engine plugin. Entry point: mbjs_init_module().
   Version 0.3.0 adds optional engine API capability probing plus array/object/
   ArrayBuffer accessors on top of the MBC1 context/value handle ABI. */

#define MB_MOCHIJSNATIVE_ABI_VERSION "MBC1"
#define MB_MOCHIJSNATIVE_MODULE_VERSION "0.3.0"

#define MB_MOCHIJSNATIVE_CAP_ARRAY 1
#define MB_MOCHIJSNATIVE_CAP_OBJECT 1
#define MB_MOCHIJSNATIVE_CAP_ARRAYBUFFER 1
#define MB_MOCHIJSNATIVE_CAP_JIT 1

#ifdef __cplusplus
}
#endif
#endif
