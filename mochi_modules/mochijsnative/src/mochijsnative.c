#include "mbjs_runtime.h"
#include "mbjs-jit.h"
#include "dynalib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#define MODULE_VERSION "mochijsnative 0.3.0"

static MBJSRuntime rt;

typedef struct {
    MBJSRuntime *(*NewRuntime)(void);
    void (*FreeRuntime)(MBJSRuntime *);
    MBJSContext *(*NewContext)(MBJSRuntime *);
    void (*FreeContext)(MBJSContext *);
    void (*SetMaxStackSize)(MBJSRuntime *, size_t);
    void (*SetMemoryLimit)(MBJSRuntime *, size_t);
    MBJSValue (*Eval)(MBJSContext *, const char *, size_t, const char *, int);
    bool (*DetectModule)(const char *, size_t);
    MBJSValue (*GetException)(MBJSContext *);
    void (*FreeValue)(MBJSContext *, MBJSValue);
    MBJSValue (*DupValue)(MBJSContext *, MBJSValueConst);
    int (*ToInt32)(MBJSContext *, int32_t *, MBJSValueConst);
    int (*ToInt64)(MBJSContext *, int64_t *, MBJSValueConst);
    int (*ToFloat64)(MBJSContext *, double *, MBJSValueConst);
    MBJSValue (*NewObject)(MBJSContext *);
    MBJSValue (*GetPropertyStr)(MBJSContext *, MBJSValueConst, const char *);
    int (*SetPropertyStr)(MBJSContext *, MBJSValueConst, const char *, MBJSValueConst);
    MBJSValue (*Call)(MBJSContext *, MBJSValueConst, MBJSValueConst, int, MBJSValueConst *);
    MBJSValue (*GetGlobalObject)(MBJSContext *);
    MBJSValue (*ParseJSON)(MBJSContext *, const char *, size_t, const char *);
    MBJSValue (*JSONStringify)(MBJSContext *, MBJSValueConst, MBJSValueConst, MBJSValueConst);
    bool (*IsError)(MBJSValueConst);
    bool (*IsFunction)(MBJSContext *, MBJSValueConst);
    void (*SetModuleLoaderFunc2)(MBJSRuntime *, MBJSModuleNormalizeFunc *,
                                 MBJSModuleLoaderFunc2 *,
                                 MBJSModuleCheckSupportedImportAttributes *, void *);
    MBJSModuleDef *(*ModuleLoader)(MBJSContext *, const char *, void *);
    int (*ModuleCheckAttrs)(MBJSContext *, void *, MBJSValueConst);
    MBJSModuleDef *(*InitStd)(MBJSContext *, const char *);
    MBJSModuleDef *(*InitOs)(MBJSContext *, const char *);
    void (*StdInitHandlers)(MBJSRuntime *);
    void (*StdFreeHandlers)(MBJSRuntime *);
    void (*StdAddHelpers)(MBJSContext *, int, char **);
    int (*StdLoop)(MBJSContext *);
    int (*StdLoopOnce)(MBJSContext *);
    MBJSValue (*StdAwait)(MBJSContext *, MBJSValue);
    void (*StdDumpError)(MBJSContext *);
    MBJSJit *(*JitNew)(MBJSJitBackend, const MBJSJitOptions *);
    void (*JitFree)(MBJSJit *);
    const char *(*JitBackendName)(const MBJSJit *);
    const char *(*JitLastError)(const MBJSJit *);
    MBJSJitFn (*JitCompile)(MBJSJit *, MBJSContext *, const char *, const char *);
    MBJSValue (*JitCall)(MBJSJit *, MBJSContext *, const char *, const char *,
                         MBJSValue, int, MBJSValue *);
    void (*JitCleanupAll)(MBJSJit *, MBJSContext *);
    MBJSJitStats (*JitStats)(const MBJSJit *);
    const char *(*GetVersion)(void);
    MBJSValue (*NewArray)(MBJSContext *);
    MBJSValue (*GetPropertyUint32)(MBJSContext *, MBJSValueConst, uint32_t);
    int (*SetPropertyUint32)(MBJSContext *, MBJSValueConst, uint32_t, MBJSValueConst);
    MBJSValue (*GetPropertyValue)(MBJSContext *, MBJSValueConst, MBJSValueConst);
    int (*SetPropertyValue)(MBJSContext *, MBJSValueConst, MBJSValueConst,
                            MBJSValueConst, int);
    int (*DefinePropertyValueStr)(MBJSContext *, MBJSValueConst, const char *,
                                  MBJSValueConst, int);
    int (*GetLength)(MBJSContext *, MBJSValueConst, int64_t *);
    bool (*IsArray)(MBJSValueConst);
    MBJSValue (*NewArrayBufferCopy)(MBJSContext *, const uint8_t *, size_t);
    uint8_t *(*GetArrayBuffer)(MBJSContext *, size_t *, MBJSValueConst);
    bool (*IsArrayBuffer)(MBJSValueConst);
} MochiApi;

static MochiApi api;

static void *host_handle(void);
static void *host_symbol(const char *name);

static const char *api_resolve(void)
{
    void *p;
#define API_SYM(field, name)                                                   \
    do {                                                                       \
        p = host_symbol(name);                                                 \
        if (!p) return name;                                                   \
        memcpy(&api.field, &p, sizeof(p));                                     \
    } while (0)
/* Optional symbol: a missing one is fine — the JIT API degrades gracefully
 * (engine builds without a JIT backend simply report "JIT unavailable"). */
#define API_SYM_OPT(field, name)                                               \
    do {                                                                       \
        p = host_symbol(name);                                                 \
        if (p) memcpy(&api.field, &p, sizeof(p));                              \
    } while (0)

    API_SYM(NewRuntime, "MBJS_NewRuntime");
    API_SYM(FreeRuntime, "MBJS_FreeRuntime");
    API_SYM(NewContext, "MBJS_NewContext");
    API_SYM(FreeContext, "MBJS_FreeContext");
    API_SYM(SetMaxStackSize, "MBJS_SetMaxStackSize");
    API_SYM(SetMemoryLimit, "MBJS_SetMemoryLimit");
    API_SYM(Eval, "MBJS_Eval");
    API_SYM(DetectModule, "MBJS_DetectModule");
    API_SYM(GetException, "MBJS_GetException");
    API_SYM(FreeValue, "MBJS_FreeValue");
    API_SYM(DupValue, "MBJS_DupValue");
    API_SYM(ToInt32, "MBJS_ToInt32");
    API_SYM(ToInt64, "MBJS_ToInt64");
    API_SYM(ToFloat64, "MBJS_ToFloat64");
    API_SYM(NewObject, "MBJS_NewObject");
    API_SYM(GetPropertyStr, "MBJS_GetPropertyStr");
    API_SYM(SetPropertyStr, "MBJS_SetPropertyStr");
    API_SYM(Call, "MBJS_Call");
    API_SYM(GetGlobalObject, "MBJS_GetGlobalObject");
    API_SYM(ParseJSON, "MBJS_ParseJSON");
    API_SYM(JSONStringify, "MBJS_JSONStringify");
    API_SYM(IsError, "MBJS_IsError");
    API_SYM(IsFunction, "MBJS_IsFunction");
    API_SYM(SetModuleLoaderFunc2, "MBJS_SetModuleLoaderFunc2");
    API_SYM(ModuleLoader, "mbjs_module_loader");
    API_SYM(ModuleCheckAttrs, "mbjs_module_check_attributes");
    API_SYM(InitStd, "mbjs_init_module_std");
    API_SYM(InitOs, "mbjs_init_module_os");
    API_SYM(StdInitHandlers, "mbjs_std_init_handlers");
    API_SYM(StdFreeHandlers, "mbjs_std_free_handlers");
    API_SYM(StdAddHelpers, "mbjs_std_add_helpers");
    API_SYM(StdLoop, "mbjs_std_loop");
    API_SYM(StdLoopOnce, "mbjs_std_loop_once");
    API_SYM(StdAwait, "mbjs_std_await");
    API_SYM(StdDumpError, "mbjs_std_dump_error");
    API_SYM_OPT(JitNew, "mbjs_jit_new");
    API_SYM_OPT(JitFree, "mbjs_jit_free");
    API_SYM_OPT(JitBackendName, "mbjs_jit_backend_name");
    API_SYM_OPT(JitLastError, "mbjs_jit_last_error");
    API_SYM_OPT(JitCompile, "mbjs_jit_compile");
    API_SYM_OPT(JitCall, "mbjs_jit_call");
    API_SYM_OPT(JitCleanupAll, "mbjs_jit_cleanup_all");
    API_SYM_OPT(JitStats, "mbjs_jit_stats");
    API_SYM_OPT(GetVersion, "MBJS_GetVersion");
    API_SYM_OPT(NewArray, "MBJS_NewArray");
    API_SYM_OPT(GetPropertyUint32, "MBJS_GetPropertyUint32");
    API_SYM_OPT(SetPropertyUint32, "MBJS_SetPropertyUint32");
    API_SYM_OPT(GetPropertyValue, "MBJS_GetPropertyValue");
    API_SYM_OPT(SetPropertyValue, "MBJS_SetPropertyValue");
    API_SYM_OPT(DefinePropertyValueStr, "MBJS_DefinePropertyValueStr");
    API_SYM_OPT(GetLength, "MBJS_GetLength");
    API_SYM_OPT(IsArray, "MBJS_IsArray");
    API_SYM_OPT(NewArrayBufferCopy, "MBJS_NewArrayBufferCopy");
    API_SYM_OPT(GetArrayBuffer, "MBJS_GetArrayBuffer");
    API_SYM_OPT(IsArrayBuffer, "MBJS_IsArrayBuffer");

#undef API_SYM
    return NULL;
}

#define MAX_BINDS 64
#define MAX_RTS 16
#define MAX_CTXS 32
#define MAX_VALUES 512
#define MAX_JITS 16
#define MAX_JITFNS 64

typedef struct {
    int used;
    void *stub;
    void *stub_end;
    DynalibAbiSpec abi;
} BindSlot;

typedef struct {
    int used;
    MBJSRuntime *rt;
} RtSlot;

typedef struct {
    int used;
    int rtId;
    MBJSContext *ctx;
    int stdReady;
} CtxSlot;

typedef struct {
    int used;
    int ctxId;
    MBJSValue value;
} ValSlot;

typedef struct {
    int used;
    MBJSJit *jit;
    char cc[256];
    char cc_flags[256];
    char inc_dir[520];
    char lib_dir[520];
    char tmp_dir[520];
    char cache_dir[520];
} JitSlot;

typedef struct {
    int used;
    int jitId;
    int ctxId;
    MBJSJitFn fn;
} JitFnSlot;

static BindSlot g_binds[MAX_BINDS];
static RtSlot g_rts[MAX_RTS];
static CtxSlot g_ctxs[MAX_CTXS];
static ValSlot g_values[MAX_VALUES];
static JitSlot g_jits[MAX_JITS];
static JitFnSlot g_jitfns[MAX_JITFNS];
static char g_error[512];

static void context_release(int id);

static void set_error(const char *msg)
{
    snprintf(g_error, sizeof(g_error), "%s", msg ? msg : "unknown error");
}

static const char *arg_cstr(MBJSContext *ctx, MBJSValueConst v)
{
    return rt.ToCStringLen(ctx, NULL, v);
}

static int arg_u64(MBJSContext *ctx, MBJSValueConst v, uint64_t *out)
{
    double d = 0;
    if (MBJS_IsNumber(v)) {
        if (rt.ToFloat64(ctx, &d, v) == 0) {
            *out = (uint64_t)(int64_t)d;
            return 0;
        }
        return -1;
    }
    if (MBJS_IsBigInt(v) && rt.ToBigUint64(ctx, out, v) == 0)
        return 0;
    return -1;
}

static int arg_int(MBJSContext *ctx, MBJSValueConst v, int *out)
{
    uint64_t raw = 0;
    if (arg_u64(ctx, v, &raw) < 0)
        return -1;
    *out = (int)(int64_t)raw;
    return 0;
}

static MBJSValue cstr_result(MBJSContext *ctx, const char *s)
{
    return rt.NewStringLen(ctx, s ? s : "", s ? strlen(s) : 0);
}

static char *nul_copy(const char *s, size_t *len_out)
{
    size_t n = strlen(s);
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return NULL;
    memcpy(buf, s, n);
    buf[n] = '\0';
    if (len_out)
        *len_out = n;
    return buf;
}

static void *host_handle(void)
{
#ifdef _WIN32
    return (void *)GetModuleHandleA(NULL);
#else
    return dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
#endif
}

static void *host_symbol(const char *name)
{
    void *h = host_handle();
    if (!h || !name)
        return NULL;
#ifdef _WIN32
    return (void *)GetProcAddress((HMODULE)h, name);
#else
    return dlsym(h, name);
#endif
}

#ifdef _WIN32
static IMAGE_EXPORT_DIRECTORY *host_exports(void)
{
    HMODULE h = (HMODULE)host_handle();
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    if (!h)
        return NULL;
    dos = (IMAGE_DOS_HEADER *)h;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return NULL;
    nt = (IMAGE_NT_HEADERS *)((unsigned char *)h + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return NULL;
    return (IMAGE_EXPORT_DIRECTORY *)((unsigned char *)h +
          nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
}
#endif

static int host_export_count(void)
{
#ifdef _WIN32
    IMAGE_EXPORT_DIRECTORY *exp = host_exports();
    return exp ? (int)exp->NumberOfFunctions : 0;
#else
    return 257;
#endif
}

static const char *known_signature(const char *name)
{
    static const struct { const char *name; const char *sig; } tab[] = {
        { "MBJS_GetVersion", "ptr()" },
        { "MBJS_NewRuntime", "ptr()" },
        { "MBJS_FreeRuntime", "void(ptr)" },
        { "MBJS_SetMaxStackSize", "void(ptr,u64)" },
        { "MBJS_SetMemoryLimit", "void(ptr,u64)" },
        { "MBJS_NewContext", "ptr(ptr)" },
        { "MBJS_FreeContext", "void(ptr)" },
        { "MBJS_NewObject", "u64(ptr)" },
        { "MBJS_GetGlobalObject", "u64(ptr)" },
        { "MBJS_GetException", "u64(ptr)" },
        { "MBJS_FreeValue", "void(ptr,u64)" },
        { "MBJS_DupValue", "u64(ptr,u64)" },
        { "MBJS_ToInt32", "i32(ptr,ptr,u64)" },
        { "MBJS_ToInt64", "i32(ptr,ptr,u64)" },
        { "MBJS_ToFloat64", "i32(ptr,ptr,u64)" },
        { "MBJS_GetPropertyStr", "u64(ptr,u64,ptr)" },
        { "MBJS_SetPropertyStr", "i32(ptr,u64,ptr,u64)" },
        { "MBJS_Call", "u64(ptr,u64,u64,i32,ptr)" },
        { "MBJS_ParseJSON", "u64(ptr,ptr,u64,ptr)" },
        { "MBJS_JSONStringify", "u64(ptr,u64,u64,u64)" },
        { "MBJS_DetectModule", "i32(ptr,u64)" },
        { "MBJS_IsError", "i32(u64)" },
        { "MBJS_IsFunction", "i32(ptr,u64)" },
        { "mbjs_std_loop", "i32(ptr)" },
        { "mbjs_std_loop_once", "i32(ptr)" },
        { "mbjs_std_await", "u64(ptr,u64)" },
        { "mbjs_std_dump_error", "void(ptr)" },
        { "MBJS_Eval", "u64(ptr,ptr,u64,ptr,i32)" },
        { "MBJS_EvalFunction", "u64(ptr,u64)" },
        { "mbjs_jit_new", "ptr(i32,ptr)" },
        { "mbjs_jit_compile", "ptr(ptr,ptr,ptr,ptr)" },
        { "mbjs_jit_call", "u64(ptr,ptr,ptr,ptr,u64,i32,ptr)" },
        { "mbjs_jit_free", "void(ptr)" },
        { "mbjs_jit_cleanup_all", "void(ptr,ptr)" },
        { "MBJS_GetVersion", "ptr()" },
        { "MBJS_NewArray", "u64(ptr)" },
        { "MBJS_GetPropertyUint32", "u64(ptr,u64,u32)" },
        { "MBJS_SetPropertyUint32", "i32(ptr,u64,u32,u64)" },
        { "MBJS_GetPropertyValue", "u64(ptr,u64,u64)" },
        { "MBJS_SetPropertyValue", "i32(ptr,u64,u64,u64,i32)" },
        { "MBJS_DefinePropertyValueStr", "i32(ptr,u64,ptr,u64,i32)" },
        { "MBJS_GetLength", "i32(ptr,u64,ptr)" },
        { "MBJS_IsArray", "i32(u64)" },
        { "MBJS_NewArrayBufferCopy", "u64(ptr,ptr,u64)" },
        { "MBJS_GetArrayBuffer", "ptr(ptr,ptr,u64)" },
        { "MBJS_IsArrayBuffer", "i32(u64)" }
    };
    size_t i;
    if (!name)
        return "";
    for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
        if (strcmp(tab[i].name, name) == 0)
            return tab[i].sig;
    return "";
}

static const char *value_type_name(MBJSValueConst v)
{
    switch (MBJS_VALUE_GET_TAG(v)) {
    case MBJS_TAG_BIG_INT: return "bigint";
    case MBJS_TAG_SYMBOL: return "symbol";
    case MBJS_TAG_STRING: return "string";
    case MBJS_TAG_STRING_ROPE: return "string_rope";
    case MBJS_TAG_MODULE: return "module";
    case MBJS_TAG_FUNCTION_BYTECODE: return "bytecode";
    case MBJS_TAG_OBJECT: return "object";
    case MBJS_TAG_INT: return "int";
    case MBJS_TAG_BOOL: return "bool";
    case MBJS_TAG_NULL: return "null";
    case MBJS_TAG_UNDEFINED: return "undefined";
    case MBJS_TAG_UNINITIALIZED: return "uninitialized";
    case MBJS_TAG_EXCEPTION: return "exception";
    case MBJS_TAG_SHORT_BIG_INT: return "bigint";
    case MBJS_TAG_FLOAT64: return "float64";
    default: return "unknown";
    }
}

static int value_store(int ctxId, MBJSValue value)
{
    int i;
    for (i = 0; i < MAX_VALUES; i++) {
        if (!g_values[i].used) {
            g_values[i].used = 1;
            g_values[i].ctxId = ctxId;
            g_values[i].value = value;
            return i;
        }
    }
    api.FreeValue(g_ctxs[ctxId].ctx, value);
    set_error("value table full");
    return -1;
}

static ValSlot *value_slot(MBJSContext *ctx, MBJSValueConst v)
{
    int id = -1;
    if (arg_int(ctx, v, &id) < 0 || id < 0 || id >= MAX_VALUES || !g_values[id].used) {
        set_error("invalid value id");
        return NULL;
    }
    return &g_values[id];
}

static int value_slot_id(MBJSContext *ctx, MBJSValueConst v)
{
    int id = -1;
    if (arg_int(ctx, v, &id) < 0 || id < 0 || id >= MAX_VALUES || !g_values[id].used)
        return -1;
    return id;
}

static CtxSlot *ctx_slot(MBJSContext *ctx, MBJSValueConst v)
{
    int id = -1;
    if (arg_int(ctx, v, &id) < 0 || id < 0 || id >= MAX_CTXS || !g_ctxs[id].used) {
        set_error("invalid context id");
        return NULL;
    }
    return &g_ctxs[id];
}

static RtSlot *rt_slot(MBJSContext *ctx, MBJSValueConst v)
{
    int id = -1;
    if (arg_int(ctx, v, &id) < 0 || id < 0 || id >= MAX_RTS || !g_rts[id].used) {
        set_error("invalid runtime id");
        return NULL;
    }
    return &g_rts[id];
}

static void free_values_for_ctx(int ctxId)
{
    int i;
    for (i = 0; i < MAX_VALUES; i++) {
        if (g_values[i].used && g_values[i].ctxId == ctxId) {
            api.FreeValue(g_ctxs[ctxId].ctx, g_values[i].value);
            memset(&g_values[i], 0, sizeof(g_values[i]));
        }
    }
}

static void context_release(int id)
{
    int i;
    CtxSlot *c;
    if (id < 0 || id >= MAX_CTXS || !g_ctxs[id].used)
        return;
    c = &g_ctxs[id];
    for (i = 0; i < MAX_JITS; i++)
        if (g_jits[i].used)
            api.JitCleanupAll(g_jits[i].jit, c->ctx);
    for (i = 0; i < MAX_JITFNS; i++)
        if (g_jitfns[i].used && g_jitfns[i].ctxId == id)
            memset(&g_jitfns[i], 0, sizeof(g_jitfns[i]));
    free_values_for_ctx(id);
    api.FreeContext(c->ctx);
    memset(c, 0, sizeof(*c));
}

static MBJSValue mn_version(MBJSContext *ctx, MBJSValueConst this_val,
                            int argc, MBJSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return cstr_result(ctx, MODULE_VERSION);
}

static MBJSValue mn_abi(MBJSContext *ctx, MBJSValueConst this_val,
                        int argc, MBJSValueConst *argv)
{
    static char buf[12288];
    int n;
    (void)this_val; (void)argc; (void)argv;
    n = snprintf(buf, sizeof(buf),
        "{\"engine\":\"MochiBerry\",\"abi\":\"ms64\",\"pointer\":8,"
        "\"export_count\":%d,\"runtime\":\"host-image\","
        "\"trampoline\":\"uint64_t stub(const uint64_t args[16])\","
        "\"float_return\":\"bits in uint64_t\","
        "\"deep\":{\"runtime\":true,\"context\":true,\"eval\":true,"
        "\"values\":true,\"properties\":true,\"call\":true,"
        "\"json\":true,\"exceptions\":true,\"jit\":true,"
        "\"export_enumeration\":true},"
        "\"symbols\":{"
        "\"MBJS_GetVersion\":\"ptr()\","
        "\"MBJS_NewRuntime\":\"ptr()\","
        "\"MBJS_FreeRuntime\":\"void(ptr)\","
        "\"MBJS_NewContext\":\"ptr(ptr)\","
        "\"MBJS_FreeContext\":\"void(ptr)\","
        "\"MBJS_Eval\":\"u64(ptr,ptr,u64,ptr,i32)\","
        "\"MBJS_Call\":\"u64(ptr,u64,u64,i32,ptr)\","
        "\"MBJS_GetPropertyStr\":\"u64(ptr,u64,ptr)\","
        "\"MBJS_SetPropertyStr\":\"i32(ptr,u64,ptr,u64)\","
        "\"MBJS_JSONStringify\":\"u64(ptr,u64,u64,u64)\","
        "\"MBJS_ParseJSON\":\"u64(ptr,ptr,u64,ptr)\","
        "\"mbjs_std_await\":\"u64(ptr,u64)\","
        "\"mbjs_std_loop\":\"i32(ptr)\","
        "\"mbjs_jit_call\":\"u64(ptr,ptr,ptr,ptr,u64,i32,ptr)\"}}",
        host_export_count());
    return rt.NewStringLen(ctx, buf, (size_t)n);
}

static MBJSValue mn_host_base(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return rt.NewBigUint64(ctx, (uint64_t)(uintptr_t)host_handle());
}

static MBJSValue mn_host_export(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    const char *name;
    void *p;
    (void)this_val;
    g_error[0] = '\0';
    if (argc < 1)
        return rt.ThrowTypeError(ctx, "hostExport: missing symbol name");
    name = arg_cstr(ctx, argv[0]);
    if (!name)
        return rt.ThrowTypeError(ctx, "hostExport: invalid symbol name");
    p = host_symbol(name);
    rt.FreeCString(ctx, name);
    if (!p) {
        set_error("hostExport: symbol not found");
        return rt.NewBigUint64(ctx, 0);
    }
    return rt.NewBigUint64(ctx, (uint64_t)(uintptr_t)p);
}

static MBJSValue mn_engine_version(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    const char *(*fn)(void);
    void *p = host_symbol("MBJS_GetVersion");
    const char *s;
    (void)this_val; (void)argc; (void)argv;
    if (!p)
        return cstr_result(ctx, "unknown");
    memcpy(&fn, &p, sizeof(fn));
    s = fn();
    return cstr_result(ctx, s);
}

static MBJSValue mn_export_names(MBJSContext *ctx, MBJSValueConst this_val,
                                 int argc, MBJSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
#ifdef _WIN32
    IMAGE_EXPORT_DIRECTORY *exp = host_exports();
    char *buf;
    size_t cap, used = 0;
    DWORD i;
    if (!exp)
        return cstr_result(ctx, "[]");
    cap = 4096 + (size_t)exp->NumberOfNames * 160;
    buf = (char *)malloc(cap);
    if (!buf)
        return cstr_result(ctx, "[]");
    used += (size_t)snprintf(buf + used, cap - used, "[");
    for (i = 0; i < exp->NumberOfNames; i++) {
        DWORD rva = ((DWORD *)((unsigned char *)host_handle() + exp->AddressOfNames))[i];
        const char *name = (const char *)((unsigned char *)host_handle() + rva);
        used += (size_t)snprintf(buf + used, cap - used, "%s\"%s\"",
                                 i ? "," : "", name);
    }
    used += (size_t)snprintf(buf + used, cap - used, "]");
    {
        MBJSValue r = rt.NewStringLen(ctx, buf, used);
        free(buf);
        return r;
    }
#else
    return cstr_result(ctx, "[]");
#endif
}

static MBJSValue mn_export_signature(MBJSContext *ctx, MBJSValueConst this_val,
                                     int argc, MBJSValueConst *argv)
{
    const char *name;
    (void)this_val;
    if (argc < 1)
        return cstr_result(ctx, "");
    name = arg_cstr(ctx, argv[0]);
    if (!name)
        return cstr_result(ctx, "");
    {
        MBJSValue r = cstr_result(ctx, known_signature(name));
        rt.FreeCString(ctx, name);
        return r;
    }
}

static MBJSValue mn_bind(MBJSContext *ctx, MBJSValueConst this_val,
                         int argc, MBJSValueConst *argv)
{
    const char *name;
    const char *spec;
    DynalibAbiSpec abi;
    void *target;
    void *stub;
    void *end = NULL;
    int slot;
    (void)this_val;
    g_error[0] = '\0';
    if (argc < 1)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    name = arg_cstr(ctx, argv[0]);
    if (!name)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    target = host_symbol(name);
    if (!target) {
        set_error("bind: symbol not found");
        rt.FreeCString(ctx, name);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    spec = argc > 1 ? arg_cstr(ctx, argv[1]) : NULL;
    if (!spec || dynalib_abi_parse(spec, &abi) != 0) {
        if (spec) rt.FreeCString(ctx, spec);
        rt.FreeCString(ctx, name);
        set_error("bind: invalid ABI spec");
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    rt.FreeCString(ctx, spec);
    rt.FreeCString(ctx, name);
    stub = dynalib_trampoline_emit(target, &abi, &end);
    if (!stub) {
        set_error("bind: trampoline emit failed");
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    for (slot = 0; slot < MAX_BINDS; slot++) {
        if (!g_binds[slot].used) {
            g_binds[slot].used = 1;
            g_binds[slot].stub = stub;
            g_binds[slot].stub_end = end;
            g_binds[slot].abi = abi;
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)slot);
        }
    }
    dynalib_trampoline_free(stub, end);
    set_error("bind: table full");
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
}

static MBJSValue mn_call(MBJSContext *ctx, MBJSValueConst this_val,
                         int argc, MBJSValueConst *argv)
{
    uint64_t args[16];
    uint64_t result = 0;
    BindSlot *s;
    uint64_t raw_id = 0;
    int id;
    int i;
    (void)this_val;
    g_error[0] = '\0';
    if (argc < 1 || arg_u64(ctx, argv[0], &raw_id) < 0)
        return rt.NewBigUint64(ctx, 0);
    id = (int)(int64_t)raw_id;
    if (id < 0 || id >= MAX_BINDS || !g_binds[id].used) {
        set_error("call: invalid bind id");
        return rt.NewBigUint64(ctx, 0);
    }
    s = &g_binds[id];
    memset(args, 0, sizeof(args));
    for (i = 1; i < argc && i <= 8; i++) {
        if (arg_u64(ctx, argv[i], &args[i - 1]) < 0) {
            set_error("call: invalid argument");
            return rt.NewBigUint64(ctx, 0);
        }
    }
    if (s->abi.ret_type == DYNALIB_T_VOID) {
        void (*stub)(const uint64_t *) = NULL;
        memcpy(&stub, &s->stub, sizeof(stub));
        stub(args);
        return rt.NewBigUint64(ctx, 0);
    }
    if (s->abi.ret_type == DYNALIB_T_F32 || s->abi.ret_type == DYNALIB_T_F64) {
        double (*stub)(const uint64_t *) = NULL;
        double d;
        memcpy(&stub, &s->stub, sizeof(stub));
        d = stub(args);
        memcpy(&result, &d, sizeof(result));
        return rt.NewBigUint64(ctx, result);
    }
    {
        uint64_t (*stub)(const uint64_t *) = NULL;
        memcpy(&stub, &s->stub, sizeof(stub));
        result = stub(args);
    }
    return rt.NewBigUint64(ctx, result);
}

static MBJSValue mn_unbind(MBJSContext *ctx, MBJSValueConst this_val,
                           int argc, MBJSValueConst *argv)
{
    uint64_t raw_id = 0;
    int id;
    (void)this_val;
    if (argc < 1 || arg_u64(ctx, argv[0], &raw_id) < 0)
        return MBJS_UNDEFINED;
    id = (int)(int64_t)raw_id;
    if (id >= 0 && id < MAX_BINDS && g_binds[id].used) {
        dynalib_trampoline_free(g_binds[id].stub, g_binds[id].stub_end);
        memset(&g_binds[id], 0, sizeof(g_binds[id]));
    }
    return MBJS_UNDEFINED;
}

static MBJSValue mn_last_error(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return cstr_result(ctx, g_error);
}

static MBJSValue mn_runtime_new(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    MBJSRuntime *r;
    int slot;
    (void)this_val; (void)argc; (void)argv;
    r = api.NewRuntime();
    if (!r)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    api.StdInitHandlers(r);
    api.SetModuleLoaderFunc2(r, NULL,
        (MBJSModuleLoaderFunc2 *)api.ModuleLoader, api.ModuleCheckAttrs, NULL);
    for (slot = 0; slot < MAX_RTS; slot++) {
        if (!g_rts[slot].used) {
            g_rts[slot].used = 1;
            g_rts[slot].rt = r;
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)slot);
        }
    }
    api.StdFreeHandlers(r);
    api.FreeRuntime(r);
    set_error("runtime table full");
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
}

static MBJSValue mn_runtime_free(MBJSContext *ctx, MBJSValueConst this_val,
                                 int argc, MBJSValueConst *argv)
{
    RtSlot *r;
    int i;
    (void)this_val;
    if (argc < 1 || !(r = rt_slot(ctx, argv[0])))
        return MBJS_UNDEFINED;
    for (i = 0; i < MAX_CTXS; i++)
        if (g_ctxs[i].used && g_ctxs[i].rtId == (int)(int64_t)(uintptr_t)(r - g_rts))
            context_release(i);
    api.StdFreeHandlers(r->rt);
    api.FreeRuntime(r->rt);
    memset(r, 0, sizeof(*r));
    return MBJS_UNDEFINED;
}

static MBJSValue mn_runtime_limits(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    RtSlot *r;
    int stack = 0;
    int memory = 0;
    (void)this_val;
    if (argc < 1 || !(r = rt_slot(ctx, argv[0])))
        return MBJS_UNDEFINED;
    if (argc > 1 && arg_int(ctx, argv[1], &stack) == 0 && stack > 0)
        api.SetMaxStackSize(r->rt, (size_t)stack);
    if (argc > 2 && arg_int(ctx, argv[2], &memory) == 0 && memory > 0)
        api.SetMemoryLimit(r->rt, (size_t)memory);
    return MBJS_UNDEFINED;
}

static void context_prepare_std(CtxSlot *c)
{
    if (c->stdReady)
        return;
    api.InitStd(c->ctx, "mbjs:std");
    api.InitOs(c->ctx, "mbjs:os");
    api.StdAddHelpers(c->ctx, 0, NULL);
    c->stdReady = 1;
}

static MBJSValue mn_context_new(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    RtSlot *r;
    CtxSlot *c;
    MBJSContext *native;
    int withStd = 1;
    int rtId;
    int slot;
    (void)this_val;
    if (argc < 1 || !(r = rt_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (argc > 1 && arg_int(ctx, argv[1], &withStd) < 0)
        withStd = 1;
    native = api.NewContext(r->rt);
    if (!native)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    rtId = (int)(int64_t)(uintptr_t)(r - g_rts);
    for (slot = 0; slot < MAX_CTXS; slot++) {
        if (!g_ctxs[slot].used) {
            g_ctxs[slot].used = 1;
            g_ctxs[slot].rtId = rtId;
            g_ctxs[slot].ctx = native;
            if (withStd)
                context_prepare_std(&g_ctxs[slot]);
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)slot);
        }
    }
    api.FreeContext(native);
    set_error("context table full");
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
}

static MBJSValue mn_context_free(MBJSContext *ctx, MBJSValueConst this_val,
                                 int argc, MBJSValueConst *argv)
{
    int id = -1;
    (void)this_val;
    if (argc < 1 || arg_int(ctx, argv[0], &id) == 0)
        context_release(id);
    return MBJS_UNDEFINED;
}

static MBJSValue mn_context_std(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    context_prepare_std(c);
    return rt.NewBigUint64(ctx, 0);
}

static MBJSValue mn_value_new(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    const char *kind;
    CtxSlot *c;
    MBJSValue v = MBJS_UNDEFINED;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    kind = arg_cstr(ctx, argv[1]);
    if (!kind)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (strcmp(kind, "int") == 0) {
        int n = 0;
        if (argc > 2) arg_int(ctx, argv[2], &n);
        v = MBJS_NewInt32(c->ctx, (int32_t)n);
    } else if (strcmp(kind, "float") == 0) {
        double d = 0;
        if (argc > 2) {
            if (MBJS_IsBigInt(argv[2])) {
                uint64_t bits = 0;
                if (rt.ToBigUint64(ctx, &bits, argv[2]) == 0)
                    memcpy(&d, &bits, sizeof(d));
            } else {
                rt.ToFloat64(ctx, &d, argv[2]);
            }
        }
        v = MBJS_NewFloat64(c->ctx, d);
    } else if (strcmp(kind, "string") == 0) {
        const char *s = argc > 2 ? arg_cstr(ctx, argv[2]) : NULL;
        size_t n = s ? strlen(s) : 0;
        v = rt.NewStringLen(c->ctx, s ? s : "", n);
        if (s) rt.FreeCString(ctx, s);
    } else if (strcmp(kind, "bool") == 0) {
        int b = 0;
        if (argc > 2) arg_int(ctx, argv[2], &b);
        v = MBJS_NewBool(c->ctx, b != 0);
    } else if (strcmp(kind, "null") == 0) {
        v = MBJS_NULL;
    } else if (strcmp(kind, "object") == 0) {
        v = api.NewObject(c->ctx);
    } else if (strcmp(kind, "undefined") == 0) {
        v = MBJS_UNDEFINED;
    }
    rt.FreeCString(ctx, kind);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store((int)(int64_t)(uintptr_t)(c - g_ctxs), v));
}

static MBJSValue mn_value_free(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return MBJS_UNDEFINED;
    api.FreeValue(g_ctxs[v->ctxId].ctx, v->value);
    memset(v, 0, sizeof(*v));
    return MBJS_UNDEFINED;
}

static MBJSValue mn_value_type(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return cstr_result(ctx, "invalid");
    return cstr_result(ctx, value_type_name(v->value));
}

static MBJSValue mn_value_is_exception(MBJSContext *ctx, MBJSValueConst this_val,
                                       int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, 0);
    return rt.NewBigUint64(ctx, MBJS_IsException(v->value) ? 1 : 0);
}

static MBJSValue mn_value_to_number(MBJSContext *ctx, MBJSValueConst this_val,
                                    int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    double d = 0;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, 0);
    if (api.ToFloat64(g_ctxs[v->ctxId].ctx, &d, v->value) < 0) {
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        return rt.NewBigUint64(ctx, bits);
    }
    {
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        return rt.NewBigUint64(ctx, bits);
    }
}

static MBJSValue mn_value_to_int(MBJSContext *ctx, MBJSValueConst this_val,
                                 int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    int64_t n = 0;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, 0);
    if (argc > 1) {
        int wide = 0;
        arg_int(ctx, argv[1], &wide);
        if (wide) {
            if (api.ToInt64(g_ctxs[v->ctxId].ctx, &n, v->value) < 0) n = 0;
        } else {
            int32_t n32 = 0;
            if (api.ToInt32(g_ctxs[v->ctxId].ctx, &n32, v->value) < 0) n32 = 0;
            n = n32;
        }
    } else if (api.ToInt64(g_ctxs[v->ctxId].ctx, &n, v->value) < 0) {
        n = 0;
    }
    return rt.NewBigUint64(ctx, (uint64_t)n);
}

static MBJSValue mn_value_to_string(MBJSContext *ctx, MBJSValueConst this_val,
                                    int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    const char *s;
    MBJSValue r;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return cstr_result(ctx, "");
    s = rt.ToCStringLen(g_ctxs[v->ctxId].ctx, NULL, v->value);
    r = cstr_result(ctx, s);
    if (s) rt.FreeCString(g_ctxs[v->ctxId].ctx, s);
    return r;
}

static MBJSValue mn_value_json(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    MBJSValue r;
    const char *s;
    (void)this_val;
    if (argc < 1 || !(v = value_slot(ctx, argv[0])))
        return cstr_result(ctx, "null");
    r = api.JSONStringify(g_ctxs[v->ctxId].ctx, v->value, MBJS_UNDEFINED, MBJS_UNDEFINED);
    if (MBJS_IsException(r)) {
        api.FreeValue(g_ctxs[v->ctxId].ctx, r);
        return cstr_result(ctx, "null");
    }
    s = rt.ToCStringLen(g_ctxs[v->ctxId].ctx, NULL, r);
    {
        MBJSValue out = cstr_result(ctx, s);
        if (s) rt.FreeCString(g_ctxs[v->ctxId].ctx, s);
        api.FreeValue(g_ctxs[v->ctxId].ctx, r);
        return out;
    }
}

static MBJSValue mn_eval(MBJSContext *ctx, MBJSValueConst this_val,
                         int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    const char *source;
    const char *filename;
    char *buf;
    size_t len;
    MBJSValue result;
    int flags = 0;
    int freeFilename = 0;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    source = arg_cstr(ctx, argv[1]);
    if (!source)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    filename = argc > 2 ? arg_cstr(ctx, argv[2]) : "<eval>";
    freeFilename = argc > 2;
    if (argc > 3) arg_int(ctx, argv[3], &flags);
    buf = nul_copy(source, &len);
    rt.FreeCString(ctx, source);
    if (!buf) {
        if (freeFilename && filename) rt.FreeCString(ctx, filename);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    result = api.Eval(c->ctx, buf, len, filename ? filename : "<eval>", flags);
    free(buf);
    if (freeFilename && filename) rt.FreeCString(ctx, filename);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static MBJSValue mn_eval_module(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    const char *source;
    const char *filename;
    char *buf;
    size_t len;
    MBJSValue result;
    (void)this_val;
    if (argc < 3 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    source = arg_cstr(ctx, argv[1]);
    filename = arg_cstr(ctx, argv[2]);
    if (!source || !filename) {
        if (source) rt.FreeCString(ctx, source);
        if (filename) rt.FreeCString(ctx, filename);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    buf = nul_copy(source, &len);
    rt.FreeCString(ctx, source);
    if (!buf) {
        rt.FreeCString(ctx, filename);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    result = api.Eval(c->ctx, buf, len, filename, MBJS_EVAL_TYPE_MODULE);
    free(buf);
    rt.FreeCString(ctx, filename);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static MBJSValue mn_detect_module(MBJSContext *ctx, MBJSValueConst this_val,
                                  int argc, MBJSValueConst *argv)
{
    const char *source;
    char *buf;
    bool r;
    (void)this_val;
    if (argc < 1 || !(source = arg_cstr(ctx, argv[0])))
        return rt.NewBigUint64(ctx, 0);
    buf = nul_copy(source, NULL);
    rt.FreeCString(ctx, source);
    if (!buf)
        return rt.NewBigUint64(ctx, 0);
    r = api.DetectModule(buf, strlen(buf));
    free(buf);
    return rt.NewBigUint64(ctx, r ? 1 : 0);
}

static MBJSValue mn_get_exception(MBJSContext *ctx, MBJSValueConst this_val,
                                  int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), api.GetException(c->ctx)));
}

static MBJSValue mn_dump_error(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])))
        return MBJS_UNDEFINED;
    api.StdDumpError(c->ctx);
    return MBJS_UNDEFINED;
}

static MBJSValue mn_await(MBJSContext *ctx, MBJSValueConst this_val,
                          int argc, MBJSValueConst *argv)
{
    ValSlot *v;
    MBJSValue result;
    int ctxId;
    (void)this_val;
    if (argc < 2 || !(v = value_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    ctxId = v->ctxId;
    result = v->value;
    memset(v, 0, sizeof(*v));
    result = api.StdAwait(g_ctxs[ctxId].ctx, result);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(ctxId, result));
}

static MBJSValue mn_loop(MBJSContext *ctx, MBJSValueConst this_val,
                         int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    int once = 0;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (argc > 1) arg_int(ctx, argv[1], &once);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)(
        once ? api.StdLoopOnce(c->ctx) : api.StdLoop(c->ctx)));
}

static MBJSValue mn_call_value(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *fn;
    MBJSValue thisValue = MBJS_UNDEFINED;
    MBJSValue values[16];
    MBJSValueConst *args = NULL;
    int callArgc = 0;
    int i;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])) ||
        !(fn = value_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (argc > 2) {
        int thisId = value_slot_id(ctx, argv[2]);
        if (thisId >= 0) thisValue = g_values[thisId].value;
    }
    for (i = 3; i < argc && callArgc < 16; i++) {
        int id = value_slot_id(ctx, argv[i]);
        if (id < 0)
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
        values[callArgc++] = g_values[id].value;
    }
    if (callArgc) args = (MBJSValueConst *)values;
    {
        MBJSValue result = api.Call(c->ctx, fn->value, thisValue, callArgc, args);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
            (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
    }
}

static MBJSValue mn_get_global(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), api.GetGlobalObject(c->ctx)));
}

static MBJSValue mn_get_prop(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    ValSlot *obj;
    const char *key;
    MBJSValue result;
    (void)this_val;
    if (argc < 3 || !(obj = value_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    key = arg_cstr(ctx, argv[2]);
    if (!key)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    result = api.GetPropertyStr(g_ctxs[obj->ctxId].ctx, obj->value, key);
    rt.FreeCString(ctx, key);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(obj->ctxId, result));
}

static MBJSValue mn_set_prop(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    ValSlot *obj;
    ValSlot *val;
    const char *key;
    int r;
    (void)this_val;
    if (argc < 4 || !(obj = value_slot(ctx, argv[1])) ||
        !(val = value_slot(ctx, argv[3])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    key = arg_cstr(ctx, argv[2]);
    if (!key)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    r = api.SetPropertyStr(g_ctxs[obj->ctxId].ctx, obj->value, key, val->value);
    rt.FreeCString(ctx, key);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)r);
}

static MBJSValue mn_value_get_prop(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *obj;
    const char *key;
    MBJSValue result;
    (void)this_val;
    if (argc < 3 || !(c = ctx_slot(ctx, argv[0])) ||
        !(obj = value_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    key = arg_cstr(ctx, argv[2]);
    if (!key)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    result = api.GetPropertyStr(c->ctx, obj->value, key);
    rt.FreeCString(ctx, key);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static MBJSValue mn_value_set_prop(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *obj;
    ValSlot *val;
    const char *key;
    int r;
    (void)this_val;
    if (argc < 4 || !(c = ctx_slot(ctx, argv[0])) ||
        !(obj = value_slot(ctx, argv[1])) || !(val = value_slot(ctx, argv[3])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    key = arg_cstr(ctx, argv[2]);
    if (!key)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    r = api.SetPropertyStr(c->ctx, obj->value, key, val->value);
    rt.FreeCString(ctx, key);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)r);
}

static MBJSValue mn_engine_cap(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    const char *name;
    int present = 0;
    (void)this_val;
    if (argc < 1)
        return rt.NewBigUint64(ctx, 0);
    name = arg_cstr(ctx, argv[0]);
    if (!name)
        return rt.NewBigUint64(ctx, 0);
    if (!strcmp(name, "GetVersion")) present = api.GetVersion != NULL;
    else if (!strcmp(name, "NewArray")) present = api.NewArray != NULL;
    else if (!strcmp(name, "GetPropertyUint32")) present = api.GetPropertyUint32 != NULL;
    else if (!strcmp(name, "SetPropertyUint32")) present = api.SetPropertyUint32 != NULL;
    else if (!strcmp(name, "GetPropertyValue")) present = api.GetPropertyValue != NULL;
    else if (!strcmp(name, "SetPropertyValue")) present = api.SetPropertyValue != NULL;
    else if (!strcmp(name, "DefinePropertyValueStr")) present = api.DefinePropertyValueStr != NULL;
    else if (!strcmp(name, "GetLength")) present = api.GetLength != NULL;
    else if (!strcmp(name, "IsArray")) present = api.IsArray != NULL;
    else if (!strcmp(name, "NewArrayBufferCopy")) present = api.NewArrayBufferCopy != NULL;
    else if (!strcmp(name, "GetArrayBuffer")) present = api.GetArrayBuffer != NULL;
    else if (!strcmp(name, "IsArrayBuffer")) present = api.IsArrayBuffer != NULL;
    rt.FreeCString(ctx, name);
    return rt.NewBigUint64(ctx, present ? 1 : 0);
}

static MBJSValue mn_array_new(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    (void)this_val;
    if (argc < 1 || !(c = ctx_slot(ctx, argv[0])) || !api.NewArray) {
        set_error("MBJS_NewArray unavailable");
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), api.NewArray(c->ctx)));
}

static MBJSValue mn_array_length(MBJSContext *ctx, MBJSValueConst this_val,
                                 int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *v;
    int64_t len = 0;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])) ||
        !(v = value_slot(ctx, argv[1])) || !api.GetLength)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (api.GetLength(c->ctx, v->value, &len) < 0)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    return rt.NewBigUint64(ctx, (uint64_t)len);
}

static MBJSValue mn_array_get(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *v;
    int index = 0;
    (void)this_val;
    if (argc < 3 || !(c = ctx_slot(ctx, argv[0])) ||
        !(v = value_slot(ctx, argv[1])) || !api.GetPropertyUint32)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    arg_int(ctx, argv[2], &index);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs),
        api.GetPropertyUint32(c->ctx, v->value, (uint32_t)index)));
}

static MBJSValue mn_array_set(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *v;
    ValSlot *item;
    int index = 0;
    int r;
    (void)this_val;
    if (argc < 4 || !(c = ctx_slot(ctx, argv[0])) ||
        !(v = value_slot(ctx, argv[1])) || !(item = value_slot(ctx, argv[3])) ||
        !api.SetPropertyUint32)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    arg_int(ctx, argv[2], &index);
    r = api.SetPropertyUint32(c->ctx, v->value, (uint32_t)index,
                              api.DupValue(c->ctx, item->value));
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)r);
}

static MBJSValue mn_get_prop_value(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *obj;
    ValSlot *keyVal;
    MBJSValue result;
    (void)this_val;
    if (argc < 3 || !(c = ctx_slot(ctx, argv[0])) ||
        !(obj = value_slot(ctx, argv[1])) || !(keyVal = value_slot(ctx, argv[2])) ||
        !api.GetPropertyValue)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    result = api.GetPropertyValue(c->ctx, obj->value, keyVal->value);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static MBJSValue mn_set_prop_value(MBJSContext *ctx, MBJSValueConst this_val,
                                   int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *obj;
    ValSlot *keyVal;
    ValSlot *val;
    int flags = 0;
    int r;
    (void)this_val;
    if (argc < 4 || !(c = ctx_slot(ctx, argv[0])) ||
        !(obj = value_slot(ctx, argv[1])) || !(keyVal = value_slot(ctx, argv[2])) ||
        !(val = value_slot(ctx, argv[3])) || !api.SetPropertyValue)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (argc > 4) arg_int(ctx, argv[4], &flags);
    r = api.SetPropertyValue(c->ctx, obj->value, keyVal->value,
                             api.DupValue(c->ctx, val->value), flags);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)r);
}

static MBJSValue mn_define_prop(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *obj;
    ValSlot *val;
    const char *key;
    int flags = 0;
    int r;
    (void)this_val;
    if (argc < 4 || !(c = ctx_slot(ctx, argv[0])) ||
        !(obj = value_slot(ctx, argv[1])) || !(val = value_slot(ctx, argv[3])) ||
        !api.DefinePropertyValueStr)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    key = arg_cstr(ctx, argv[2]);
    if (!key)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    if (argc > 4) arg_int(ctx, argv[4], &flags);
    r = api.DefinePropertyValueStr(c->ctx, obj->value, key,
                                   api.DupValue(c->ctx, val->value), flags);
    rt.FreeCString(ctx, key);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)r);
}

static MBJSValue mn_array_buffer_copy(MBJSContext *ctx, MBJSValueConst this_val,
                                      int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    uint64_t ptr = 0;
    uint64_t size = 0;
    (void)this_val;
    if (argc < 3 || !(c = ctx_slot(ctx, argv[0])) || !api.NewArrayBufferCopy ||
        arg_u64(ctx, argv[1], &ptr) < 0 || arg_u64(ctx, argv[2], &size) < 0 || !ptr)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs),
        api.NewArrayBufferCopy(c->ctx, (const uint8_t *)(uintptr_t)ptr, (size_t)size)));
}

static MBJSValue mn_array_buffer_data(MBJSContext *ctx, MBJSValueConst this_val,
                                      int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *v;
    size_t size = 0;
    uint8_t *data;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])) ||
        !(v = value_slot(ctx, argv[1])) || !api.GetArrayBuffer)
        return rt.NewBigUint64(ctx, 0);
    data = api.GetArrayBuffer(c->ctx, &size, v->value);
    return rt.NewBigUint64(ctx, (uint64_t)(uintptr_t)data);
}

static MBJSValue mn_array_buffer_size(MBJSContext *ctx, MBJSValueConst this_val,
                                      int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    ValSlot *v;
    size_t size = 0;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])) ||
        !(v = value_slot(ctx, argv[1])) || !api.GetArrayBuffer)
        return rt.NewBigUint64(ctx, 0);
    api.GetArrayBuffer(c->ctx, &size, v->value);
    return rt.NewBigUint64(ctx, (uint64_t)size);
}

static MBJSValue mn_json_parse(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    CtxSlot *c;
    const char *text;
    char *buf;
    size_t len;
    MBJSValue result;
    (void)this_val;
    if (argc < 2 || !(c = ctx_slot(ctx, argv[0])) || !(text = arg_cstr(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    buf = nul_copy(text, &len);
    rt.FreeCString(ctx, text);
    if (!buf)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    result = api.ParseJSON(c->ctx, buf, len, "<json>");
    free(buf);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static void opt_str(char *dst, size_t cap, MBJSContext *ctx,
                    MBJSValueConst *argv, int index)
{
    const char *s;
    dst[0] = '\0';
    if (index >= 0 && index < 10000) {
        s = arg_cstr(ctx, argv[index]);
        if (s) {
            snprintf(dst, cap, "%s", s);
            rt.FreeCString(ctx, s);
        }
    }
}

static MBJSValue mn_jit_new(MBJSContext *ctx, MBJSValueConst this_val,
                            int argc, MBJSValueConst *argv)
{
    MBJSJitOptions opts;
    MBJSJit *jit;
    int backend = 0;
    int hot = 0;
    int slot;
    (void)this_val;
    if (!api.JitNew) {
        set_error("JIT backend unavailable in this engine build");
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    if (argc > 0) arg_int(ctx, argv[0], &backend);
    if (argc > 5) arg_int(ctx, argv[5], &hot);
    memset(&opts, 0, sizeof(opts));
    for (slot = 0; slot < MAX_JITS; slot++) {
        if (!g_jits[slot].used)
            break;
    }
    if (slot == MAX_JITS)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    opt_str(g_jits[slot].cc, sizeof(g_jits[slot].cc), ctx, argv, 1);
    opt_str(g_jits[slot].cc_flags, sizeof(g_jits[slot].cc_flags), ctx, argv, 2);
    opt_str(g_jits[slot].inc_dir, sizeof(g_jits[slot].inc_dir), ctx, argv, 3);
    opt_str(g_jits[slot].lib_dir, sizeof(g_jits[slot].lib_dir), ctx, argv, 4);
    opt_str(g_jits[slot].tmp_dir, sizeof(g_jits[slot].tmp_dir), ctx, argv, 6);
    opt_str(g_jits[slot].cache_dir, sizeof(g_jits[slot].cache_dir), ctx, argv, 7);
    opts.cc = g_jits[slot].cc[0] ? g_jits[slot].cc : NULL;
    opts.cc_flags = g_jits[slot].cc_flags[0] ? g_jits[slot].cc_flags : NULL;
    opts.inc_dir = g_jits[slot].inc_dir[0] ? g_jits[slot].inc_dir : NULL;
    opts.lib_dir = g_jits[slot].lib_dir[0] ? g_jits[slot].lib_dir : NULL;
    opts.tmp_dir = g_jits[slot].tmp_dir[0] ? g_jits[slot].tmp_dir : NULL;
    opts.cache_dir = g_jits[slot].cache_dir[0] ? g_jits[slot].cache_dir : NULL;
    opts.hot_threshold = hot > 0 ? (unsigned)hot : 0;
    jit = api.JitNew((MBJSJitBackend)backend, &opts);
    if (!jit)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    g_jits[slot].used = 1;
    g_jits[slot].jit = jit;
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)slot);
}

static JitSlot *jit_slot(MBJSContext *ctx, MBJSValueConst v)
{
    int id = -1;
    if (arg_int(ctx, v, &id) < 0 || id < 0 || id >= MAX_JITS || !g_jits[id].used) {
        set_error("invalid jit id");
        return NULL;
    }
    return &g_jits[id];
}

static MBJSValue mn_jit_info(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    int which;
    (void)this_val;
    if (argc < 2 || !(j = jit_slot(ctx, argv[0])))
        return cstr_result(ctx, "");
    if (arg_int(ctx, argv[1], &which) < 0)
        which = 0;
    if (which == 0)
        return cstr_result(ctx, api.JitBackendName(j->jit));
    return cstr_result(ctx, api.JitLastError(j->jit));
}

static MBJSValue mn_jit_stats(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    MBJSJitStats s;
    char buf[1024];
    int n;
    (void)this_val;
    if (argc < 1 || !(j = jit_slot(ctx, argv[0])))
        return cstr_result(ctx, "{}");
    s = api.JitStats(j->jit);
    n = snprintf(buf, sizeof(buf),
        "{\"compile_attempts\":%lu,\"compile_ok\":%lu,\"compile_failed\":%lu,"
        "\"native_calls\":%lu,\"interp_calls\":%lu,\"hot_tracked\":%lu,"
        "\"hot_compiled\":%lu,\"cache_hits\":%lu,\"cache_disk_hits\":%lu,"
        "\"cache_disk_misses\":%lu,\"cache_disk_writes\":%lu,"
        "\"cache_disk_errors\":%lu}",
        s.compile_attempts, s.compile_ok, s.compile_failed,
        s.native_calls, s.interp_calls, s.hot_tracked,
        s.hot_compiled, s.cache_hits, s.cache_disk_hits,
        s.cache_disk_misses, s.cache_disk_writes, s.cache_disk_errors);
    return rt.NewStringLen(ctx, buf, (size_t)n);
}

static MBJSValue mn_jit_compile(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    CtxSlot *c;
    const char *source;
    const char *name;
    char *buf;
    MBJSJitFn fn;
    int slot;
    int jitId;
    int ctxId;
    (void)this_val;
    if (argc < 4 || !(j = jit_slot(ctx, argv[0])) || !(c = ctx_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    source = arg_cstr(ctx, argv[2]);
    name = arg_cstr(ctx, argv[3]);
    if (!source || !name) {
        if (source) rt.FreeCString(ctx, source);
        if (name) rt.FreeCString(ctx, name);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    buf = nul_copy(source, NULL);
    rt.FreeCString(ctx, source);
    if (!buf) {
        rt.FreeCString(ctx, name);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    fn = api.JitCompile(j->jit, c->ctx, buf, name);
    free(buf);
    rt.FreeCString(ctx, name);
    if (!fn)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    jitId = (int)(int64_t)(uintptr_t)(j - g_jits);
    ctxId = (int)(int64_t)(uintptr_t)(c - g_ctxs);
    for (slot = 0; slot < MAX_JITFNS; slot++) {
        if (!g_jitfns[slot].used) {
            g_jitfns[slot].used = 1;
            g_jitfns[slot].jitId = jitId;
            g_jitfns[slot].ctxId = ctxId;
            g_jitfns[slot].fn = fn;
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)slot);
        }
    }
    set_error("jit function table full");
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
}

static MBJSValue mn_jit_invoke(MBJSContext *ctx, MBJSValueConst this_val,
                               int argc, MBJSValueConst *argv)
{
    JitFnSlot *f;
    MBJSValue thisValue = MBJS_UNDEFINED;
    MBJSValue values[16];
    MBJSValue *args = NULL;
    int fnargc = 0;
    int i;
    int id = -1;
    (void)this_val;
    if (argc < 1 || arg_int(ctx, argv[0], &id) < 0 ||
        id < 0 || id >= MAX_JITFNS || !g_jitfns[id].used)
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    f = &g_jitfns[id];
    if (argc > 1) {
        int thisId = value_slot_id(ctx, argv[1]);
        if (thisId >= 0) thisValue = g_values[thisId].value;
    }
    for (i = 2; i < argc && fnargc < 16; i++) {
        int slotId = value_slot_id(ctx, argv[i]);
        if (slotId < 0)
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
        values[fnargc++] = g_values[slotId].value;
    }
    if (fnargc) args = values;
    {
        MBJSValue result = f->fn(g_ctxs[f->ctxId].ctx, thisValue, fnargc, args);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(f->ctxId, result));
    }
}

static MBJSValue mn_jit_call(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    CtxSlot *c;
    const char *source;
    const char *name;
    char *buf;
    MBJSValue thisValue = MBJS_UNDEFINED;
    MBJSValue values[16];
    MBJSValue *args = NULL;
    int fnargc = 0;
    int i;
    MBJSValue result;
    (void)this_val;
    if (argc < 4 || !(j = jit_slot(ctx, argv[0])) || !(c = ctx_slot(ctx, argv[1])))
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    source = arg_cstr(ctx, argv[2]);
    name = arg_cstr(ctx, argv[3]);
    if (!source || !name) {
        if (source) rt.FreeCString(ctx, source);
        if (name) rt.FreeCString(ctx, name);
        return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
    }
    if (argc > 4) {
        int thisId = value_slot_id(ctx, argv[4]);
        if (thisId >= 0) thisValue = g_values[thisId].value;
    }
    for (i = 5; i < argc && fnargc < 16; i++) {
        int slotId = value_slot_id(ctx, argv[i]);
        if (slotId < 0) {
            rt.FreeCString(ctx, source);
            rt.FreeCString(ctx, name);
            return rt.NewBigUint64(ctx, (uint64_t)(int64_t)-1);
        }
        values[fnargc++] = g_values[slotId].value;
    }
    if (fnargc) args = values;
    buf = nul_copy(source, NULL);
    rt.FreeCString(ctx, source);
    result = api.JitCall(j->jit, c->ctx, buf ? buf : "", name, thisValue, fnargc, args);
    free(buf);
    rt.FreeCString(ctx, name);
    return rt.NewBigUint64(ctx, (uint64_t)(int64_t)value_store(
        (int)(int64_t)(uintptr_t)(c - g_ctxs), result));
}

static MBJSValue mn_jit_cleanup(MBJSContext *ctx, MBJSValueConst this_val,
                                int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    CtxSlot *c;
    int i;
    (void)this_val;
    if (argc < 2 || !(j = jit_slot(ctx, argv[0])) || !(c = ctx_slot(ctx, argv[1])))
        return MBJS_UNDEFINED;
    api.JitCleanupAll(j->jit, c->ctx);
    for (i = 0; i < MAX_JITFNS; i++)
        if (g_jitfns[i].used && g_jitfns[i].jitId == (int)(int64_t)(uintptr_t)(j - g_jits) &&
            g_jitfns[i].ctxId == (int)(int64_t)(uintptr_t)(c - g_ctxs))
            memset(&g_jitfns[i], 0, sizeof(g_jitfns[i]));
    return MBJS_UNDEFINED;
}

static MBJSValue mn_jit_free(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    JitSlot *j;
    int jitId;
    int i;
    (void)this_val;
    if (argc < 1 || !(j = jit_slot(ctx, argv[0])))
        return MBJS_UNDEFINED;
    jitId = (int)(int64_t)(uintptr_t)(j - g_jits);
    for (i = 0; i < MAX_JITFNS; i++) {
        if (g_jitfns[i].used && g_jitfns[i].jitId == jitId) {
            if (g_ctxs[g_jitfns[i].ctxId].used)
                api.JitCleanupAll(j->jit, g_ctxs[g_jitfns[i].ctxId].ctx);
            memset(&g_jitfns[i], 0, sizeof(g_jitfns[i]));
        }
    }
    api.JitFree(j->jit);
    memset(j, 0, sizeof(*j));
    return MBJS_UNDEFINED;
}

#define MN_TAB_LEN 58

static const MBJSCFunctionListEntry mn_tab[MN_TAB_LEN] = {
    MBJS_CFUNC_DEF("version", 0, mn_version),
    MBJS_CFUNC_DEF("abi", 0, mn_abi),
    MBJS_CFUNC_DEF("exportNames", 0, mn_export_names),
    MBJS_CFUNC_DEF("exportSignature", 1, mn_export_signature),
    MBJS_CFUNC_DEF("hostBase", 0, mn_host_base),
    MBJS_CFUNC_DEF("hostExport", 1, mn_host_export),
    MBJS_CFUNC_DEF("engineVersion", 0, mn_engine_version),
    MBJS_CFUNC_DEF("bind", 2, mn_bind),
    MBJS_CFUNC_DEF("call", 9, mn_call),
    MBJS_CFUNC_DEF("unbind", 1, mn_unbind),
    MBJS_CFUNC_DEF("lastError", 0, mn_last_error),
    MBJS_CFUNC_DEF("runtimeNew", 0, mn_runtime_new),
    MBJS_CFUNC_DEF("runtimeFree", 1, mn_runtime_free),
    MBJS_CFUNC_DEF("runtimeLimits", 3, mn_runtime_limits),
    MBJS_CFUNC_DEF("contextNew", 2, mn_context_new),
    MBJS_CFUNC_DEF("contextFree", 1, mn_context_free),
    MBJS_CFUNC_DEF("contextStd", 1, mn_context_std),
    MBJS_CFUNC_DEF("valueNew", 3, mn_value_new),
    MBJS_CFUNC_DEF("valueFree", 1, mn_value_free),
    MBJS_CFUNC_DEF("valueType", 1, mn_value_type),
    MBJS_CFUNC_DEF("valueIsException", 1, mn_value_is_exception),
    MBJS_CFUNC_DEF("valueToNumber", 1, mn_value_to_number),
    MBJS_CFUNC_DEF("valueToInt", 2, mn_value_to_int),
    MBJS_CFUNC_DEF("valueToString", 1, mn_value_to_string),
    MBJS_CFUNC_DEF("valueJson", 1, mn_value_json),
    MBJS_CFUNC_DEF("eval", 4, mn_eval),
    MBJS_CFUNC_DEF("evalModule", 3, mn_eval_module),
    MBJS_CFUNC_DEF("detectModule", 1, mn_detect_module),
    MBJS_CFUNC_DEF("getException", 1, mn_get_exception),
    MBJS_CFUNC_DEF("dumpError", 1, mn_dump_error),
    MBJS_CFUNC_DEF("await", 2, mn_await),
    MBJS_CFUNC_DEF("loop", 2, mn_loop),
    MBJS_CFUNC_DEF("callValue", 18, mn_call_value),
    MBJS_CFUNC_DEF("getGlobal", 1, mn_get_global),
    MBJS_CFUNC_DEF("getProp", 3, mn_get_prop),
    MBJS_CFUNC_DEF("setProp", 4, mn_set_prop),
    MBJS_CFUNC_DEF("jsonParse", 2, mn_json_parse),
    MBJS_CFUNC_DEF("jitNew", 8, mn_jit_new),
    MBJS_CFUNC_DEF("jitInfo", 2, mn_jit_info),
    MBJS_CFUNC_DEF("jitStats", 1, mn_jit_stats),
    MBJS_CFUNC_DEF("jitCompile", 4, mn_jit_compile),
    MBJS_CFUNC_DEF("jitInvoke", 18, mn_jit_invoke),
    MBJS_CFUNC_DEF("jitCall", 21, mn_jit_call),
    MBJS_CFUNC_DEF("jitCleanup", 2, mn_jit_cleanup),
    MBJS_CFUNC_DEF("jitFree", 1, mn_jit_free),
    MBJS_CFUNC_DEF("engineCap", 1, mn_engine_cap),
    MBJS_CFUNC_DEF("arrayNew", 1, mn_array_new),
    MBJS_CFUNC_DEF("arrayLength", 2, mn_array_length),
    MBJS_CFUNC_DEF("arrayGet", 3, mn_array_get),
    MBJS_CFUNC_DEF("arraySet", 4, mn_array_set),
    MBJS_CFUNC_DEF("getPropValue", 3, mn_get_prop_value),
    MBJS_CFUNC_DEF("setPropValue", 5, mn_set_prop_value),
    MBJS_CFUNC_DEF("defineProp", 5, mn_define_prop),
    MBJS_CFUNC_DEF("arrayBufferCopy", 3, mn_array_buffer_copy),
    MBJS_CFUNC_DEF("arrayBufferData", 2, mn_array_buffer_data),
    MBJS_CFUNC_DEF("arrayBufferSize", 2, mn_array_buffer_size),
    MBJS_CFUNC_DEF("valueGetProp", 3, mn_value_get_prop),
    MBJS_CFUNC_DEF("valueSetProp", 4, mn_value_set_prop)
};

static int mn_mod_init(MBJSContext *ctx, MBJSModuleDef *m)
{
    return rt.SetModuleExportList(ctx, m, mn_tab, MN_TAB_LEN);
}

MBJS_MODULE_EXTERN MBJSModuleDef *mbjs_init_module(MBJSContext *ctx,
                                                    const char *name)
{
    const char *missing;
    MBJSModuleDef *m;
    missing = mbjs_runtime_resolve(&rt);
    if (missing)
        return NULL;
    missing = api_resolve();
    if (missing)
        return NULL;
    m = rt.NewCModule(ctx, name ? name : "mochijsnative", mn_mod_init);
    if (!m)
        return NULL;
    if (rt.AddModuleExportList(ctx, m, mn_tab, MN_TAB_LEN) != 0)
        return NULL;
    return m;
}
