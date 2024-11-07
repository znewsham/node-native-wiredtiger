#ifndef HELPERS_H
#define HELPERS_H

#define RETURN_IF_ERROR(code) { int __ret_if_error__ = code; if (__ret_if_error__ != 0) { return __ret_if_error__; } }
#define RETURN_AND_FREE_IF_ERROR(code, ptr) { int __ret_if_error__ = code; if (__ret_if_error__ != 0) { free(ptr); return __ret_if_error__; } }
#define ERROR_AND_RESULT_RETURN_IF_ERROR(result, code, errorValue) if (code != 0) { result.error = code; result.value = errorValue; return result; }
#define POINTER_RETURN_IF_ERROR(result, code) if (code != 0) { result.error = code; result.value = NULL; return result; }

#endif
