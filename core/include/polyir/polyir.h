/* polyir's C ABI: build a program, compile it to LLVM IR text. Everything is an `int64_t` handle into a context (0 means "no such thing"), so the library
 * can be called from anything that can call C: ninac, Nina (`import cpp "polyir/polyir.h"`), Python's ctypes, Swift, Rust.
 *
 *   int64_t context = polyir_context_new();
 *   int64_t i64 = polyir_type_int(context, 64);
 *   int64_t main = polyir_function_begin(context, "main", i64);
 *   int64_t body = polyir_function_body(context, main);
 *   polyir_ret(context, body, polyir_const_int(context, i64, 42));
 *   if (polyir_compile(context)) puts(polyir_output(context)); else puts(polyir_error(context));
 *   polyir_context_free(context);
 *
 * A call that cannot be done (an unknown name, two types that differ) returns 0 and keeps the reason for polyir_error().
 * The parameters below keep the short name `ctx` on purpose: Nina's `import cpp` binder copies them into its checked-in wrappers.
 */
#ifndef POLYIR_H
#define POLYIR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  define POLYIR_C_API __declspec(dllimport)
#else
#  define POLYIR_C_API __attribute__((visibility("default")))
#endif

  /* binary operators, for polyir_binary */
  enum { POLYIR_ADD, POLYIR_SUB, POLYIR_MUL, POLYIR_DIV, POLYIR_REM, POLYIR_AND, POLYIR_OR, POLYIR_XOR, POLYIR_SHL, POLYIR_SHR, POLYIR_EQ, POLYIR_NE, POLYIR_LT, POLYIR_LE, POLYIR_GT, POLYIR_GE, POLYIR_LOGAND, POLYIR_LOGOR, POLYIR_IDENTICAL, POLYIR_NOT_IDENTICAL };

  /* unary operators, for polyir_unary */
  enum { POLYIR_NEG, POLYIR_LOGNOT, POLYIR_BITNOT };

  POLYIR_C_API int64_t polyir_context_new(void);

  POLYIR_C_API void polyir_context_free(int64_t ctx);

  POLYIR_C_API const char* polyir_version(void);

  POLYIR_C_API const char* polyir_error(int64_t ctx); /* the reason the last call failed, or "" */

  /* types */
  POLYIR_C_API int64_t polyir_type_int(int64_t ctx, int32_t bits);   /* i1 (a flag), i8, i16, i32, i64 */

  POLYIR_C_API int64_t polyir_type_uint(int64_t ctx, int32_t bits);  /* u8 .. u64 */

  POLYIR_C_API int64_t polyir_type_float(int64_t ctx, int32_t bits); /* f32, f64 */

  POLYIR_C_API int64_t polyir_type_string(int64_t ctx);

  POLYIR_C_API int64_t polyir_type_void(int64_t ctx);

  POLYIR_C_API int64_t polyir_type_struct(int64_t ctx, const char* name);

  /* the containers and sum types: a type argument list is made with polyir_args_new and polyir_args_push_type (or polyir_args_push_int for a tensor's dimensions) */
  POLYIR_C_API int64_t polyir_type_pointer(int64_t ctx, int64_t element);

  POLYIR_C_API int64_t polyir_type_func(int64_t ctx, int64_t params, int64_t result);    /* a plain function value */

  POLYIR_C_API int64_t polyir_type_closure(int64_t ctx, int64_t params, int64_t result); /* a function value that carries what it captured */

  POLYIR_C_API int64_t polyir_type_vec(int64_t ctx, int64_t element, int32_t count);     /* a vector of numbers */

  POLYIR_C_API int64_t polyir_type_matrix(int64_t ctx, int64_t element, int32_t size);   /* a square matrix of floats: a GPU kernel type */

  POLYIR_C_API int64_t polyir_type_fixed_array(int64_t ctx, int64_t element, int32_t size);

  POLYIR_C_API int64_t polyir_type_buffer(int64_t ctx, int64_t element, int32_t size);

  POLYIR_C_API int64_t polyir_type_array(int64_t ctx, int64_t element, int32_t is_mutable); /* array<T> / mutarray<T> */

  POLYIR_C_API int64_t polyir_type_sparse(int64_t ctx, int64_t element);

  POLYIR_C_API int64_t polyir_type_map(int64_t ctx, int64_t key, int64_t value, int32_t is_mutable);

  POLYIR_C_API int64_t polyir_type_set(int64_t ctx, int64_t element, int32_t is_mutable);

  POLYIR_C_API int64_t polyir_type_grid(int64_t ctx, int64_t element, int32_t rows, int32_t cols);

  POLYIR_C_API int64_t polyir_type_tensor(int64_t ctx, int64_t element, int64_t dims);

  POLYIR_C_API int64_t polyir_type_variant(int64_t ctx, int64_t cases);

  POLYIR_C_API int64_t polyir_type_either(int64_t ctx, int64_t ok, int64_t err);

  POLYIR_C_API int64_t polyir_type_optional(int64_t ctx, int64_t element);

  POLYIR_C_API int64_t polyir_type_coro(int64_t ctx, int64_t yields);

  /* declarations */
  POLYIR_C_API int64_t polyir_struct_begin(int64_t ctx, const char* name);

  POLYIR_C_API int64_t polyir_struct_field(int64_t ctx, int64_t structure, const char* name, int64_t type, int32_t is_mutable);

  POLYIR_C_API int64_t polyir_global(int64_t ctx, const char* name, int64_t type, int64_t init);

  POLYIR_C_API int64_t polyir_extern_begin(int64_t ctx, const char* name, int64_t result);

  POLYIR_C_API int64_t polyir_extern_param(int64_t ctx, int64_t function, const char* name, int64_t type);

  POLYIR_C_API int64_t polyir_function_begin(int64_t ctx, const char* name, int64_t result);

  POLYIR_C_API int64_t polyir_function_param(int64_t ctx, int64_t function, const char* name, int64_t type);

  POLYIR_C_API int64_t polyir_function_body(int64_t ctx, int64_t function); /* the block of statements; call after the parameters */

  /* GPU kernels. Make the function a kernel of `stage` ("compute", "vertex", "fragment" or "reduce") before its body. Then give the role of
   * each parameter in order with polyir_function_param_role. Roles: 0 element, 1 uniform, 2 buffer, 3 index, 4 writable, 5 workgroup,
   * 6 attribute, 7 varying, 8 output, 9 position (see polyir/Hir.hpp). Writing the kernels as Metal or GLSL is libpolyir-lang-targets's:
   * polyir_emit_metal and polyir_emit_glsl in polyir/polyir_kernels.h. */
  POLYIR_C_API int64_t polyir_function_kernel(int64_t ctx, int64_t function, const char* stage);

  POLYIR_C_API int64_t polyir_function_param_role(int64_t ctx, int64_t function, int32_t role);

  POLYIR_C_API int64_t polyir_coroutine_begin(int64_t ctx, const char* name, int64_t yields); /* a function whose result is coro<yields>; add parameters like any function */

  POLYIR_C_API int64_t polyir_interface_begin(int64_t ctx, const char* name);

  POLYIR_C_API int64_t polyir_interface_method(int64_t ctx, int64_t iface, const char* name, int64_t params, int64_t result); /* in vtable slot order */

  POLYIR_C_API int64_t polyir_implement(int64_t ctx, const char* iface, const char* structure); /* the struct's methods are the functions <Struct>_<method> */

  /* host declarations, for the back ends (the LLVM compiler ignores them): they show in polyir_hir_text. A binding is "target=text", e.g. "java=java.util.ArrayList";
   * add the bindings right after the declaration, before anything else uses it. */
  POLYIR_C_API int64_t polyir_host_type(int64_t ctx, const char* name, const char* base, int32_t is_interface, int64_t type_args); /* extern type / extern interface; type_args may be 0 */

  POLYIR_C_API int64_t polyir_host_type_binding(int64_t ctx, int64_t host_type, const char* binding);

  POLYIR_C_API int64_t polyir_bridge(int64_t ctx, int64_t poly_type, const char* target, const char* host_type, const char* to_host, const char* from_host); /* `$` is the value in each template */

  POLYIR_C_API int64_t polyir_host_member(int64_t ctx, const char* function, const char* owner, const char* member, int32_t is_static, int32_t has_result);

  POLYIR_C_API int64_t polyir_host_member_binding(int64_t ctx, int64_t host_member, const char* binding);

  /* expressions */
  POLYIR_C_API int64_t polyir_const_int(int64_t ctx, int64_t type, int64_t value);

  POLYIR_C_API int64_t polyir_const_float(int64_t ctx, int64_t type, double value);

  POLYIR_C_API int64_t polyir_const_bool(int64_t ctx, int32_t value);

  POLYIR_C_API int64_t polyir_const_string(int64_t ctx, const char* text);

  POLYIR_C_API int64_t polyir_local(int64_t ctx, int64_t block, const char* name);

  POLYIR_C_API int64_t polyir_global_ref(int64_t ctx, const char* name);

  POLYIR_C_API int64_t polyir_binary(int64_t ctx, int32_t op, int64_t left, int64_t right);

  POLYIR_C_API int64_t polyir_unary(int64_t ctx, int32_t op, int64_t operand);

  POLYIR_C_API int64_t polyir_cast(int64_t ctx, int64_t value, int64_t type);

  POLYIR_C_API int64_t polyir_field(int64_t ctx, int64_t object, const char* name);

  POLYIR_C_API int64_t polyir_args_new(int64_t ctx);                       /* a list of arguments (or struct fields) for a call */

  POLYIR_C_API int64_t polyir_args_push(int64_t ctx, int64_t args, int64_t value);

  POLYIR_C_API int64_t polyir_args_field(int64_t ctx, int64_t args, const char* name, int64_t value); /* a named one, for polyir_struct_lit */

  POLYIR_C_API int64_t polyir_call(int64_t ctx, const char* function, int64_t args);

  POLYIR_C_API int64_t polyir_struct_lit(int64_t ctx, const char* structure, int64_t args);

  POLYIR_C_API int64_t polyir_args_push_type(int64_t ctx, int64_t args, int64_t type);

  POLYIR_C_API int64_t polyir_args_push_int(int64_t ctx, int64_t args, int64_t value);

  POLYIR_C_API int64_t polyir_func_ref(int64_t ctx, const char* function);

  POLYIR_C_API int64_t polyir_coerce(int64_t ctx, int64_t value, int64_t type);

  POLYIR_C_API int64_t polyir_call_indirect(int64_t ctx, int64_t callee, int64_t args);

  POLYIR_C_API int64_t polyir_unsupported(int64_t ctx, const char* what, int64_t type);

  POLYIR_C_API int64_t polyir_vec_lit(int64_t ctx, int64_t type, int64_t args);       /* [a, b, c] as the container type */

  POLYIR_C_API int64_t polyir_new_container(int64_t ctx, int64_t type, int64_t args); /* args: the size arguments (may be 0) */

  POLYIR_C_API int64_t polyir_index(int64_t ctx, int64_t base, int64_t args);

  POLYIR_C_API int64_t polyir_len(int64_t ctx, int64_t container);

  POLYIR_C_API int64_t polyir_push(int64_t ctx, int64_t list, int64_t value);

  POLYIR_C_API int64_t polyir_truncate(int64_t ctx, int64_t list, int64_t length); /* shrinks a mutarray to its first `length` elements */

  POLYIR_C_API int64_t polyir_remove(int64_t ctx, int64_t collection, int64_t key); /* takes a key out of a mutmap or mutset, an i1: it was there */

  POLYIR_C_API int64_t polyir_has(int64_t ctx, int64_t collection, int64_t key);

  POLYIR_C_API int64_t polyir_set_add(int64_t ctx, int64_t set, int64_t member);

  POLYIR_C_API int64_t polyir_copy(int64_t ctx, int64_t container);

  POLYIR_C_API int64_t polyir_freeze(int64_t ctx, int64_t container);

  POLYIR_C_API int64_t polyir_thaw(int64_t ctx, int64_t container);

  POLYIR_C_API int64_t polyir_with(int64_t ctx, int64_t collection, int64_t args);

  POLYIR_C_API int64_t polyir_str_eq(int64_t ctx, int64_t a, int64_t b);

  POLYIR_C_API int64_t polyir_concat(int64_t ctx, int64_t a, int64_t b);

  POLYIR_C_API int64_t polyir_variant_lit(int64_t ctx, int64_t type, int32_t case_index, int64_t value);

  POLYIR_C_API int64_t polyir_variant_tag(int64_t ctx, int64_t variant);

  POLYIR_C_API int64_t polyir_variant_is_case(int64_t ctx, int64_t variant, int32_t case_index);

  POLYIR_C_API int64_t polyir_variant_unwrap(int64_t ctx, int64_t variant, int32_t case_index);

  POLYIR_C_API int64_t polyir_ok(int64_t ctx, int64_t either_type, int64_t value);

  POLYIR_C_API int64_t polyir_err(int64_t ctx, int64_t either_type, int64_t value);

  POLYIR_C_API int64_t polyir_is_ok(int64_t ctx, int64_t either);

  POLYIR_C_API int64_t polyir_is_err(int64_t ctx, int64_t either);

  POLYIR_C_API int64_t polyir_unwrap_ok(int64_t ctx, int64_t either);

  POLYIR_C_API int64_t polyir_unwrap_err(int64_t ctx, int64_t either);

  POLYIR_C_API int64_t polyir_some(int64_t ctx, int64_t value);

  POLYIR_C_API int64_t polyir_none(int64_t ctx, int64_t element_type);

  POLYIR_C_API int64_t polyir_is_some(int64_t ctx, int64_t optional);

  POLYIR_C_API int64_t polyir_is_none(int64_t ctx, int64_t optional);

  POLYIR_C_API int64_t polyir_unwrap(int64_t ctx, int64_t optional);

  POLYIR_C_API int64_t polyir_iface_box(int64_t ctx, const char* iface, int64_t struct_value);

  POLYIR_C_API int64_t polyir_iface_call(int64_t ctx, int64_t object, const char* method, int64_t args);

  POLYIR_C_API int64_t polyir_resume(int64_t ctx, int64_t coroutine);

  POLYIR_C_API int64_t polyir_done(int64_t ctx, int64_t coroutine);

  /* a closure: begin it in a block, add its parameters, take its body block and write the statements, then end it to get the expression */
  POLYIR_C_API int64_t polyir_closure_begin(int64_t ctx, int64_t block, int64_t result);

  POLYIR_C_API int64_t polyir_closure_param(int64_t ctx, int64_t closure, const char* name, int64_t type);

  POLYIR_C_API int64_t polyir_closure_body(int64_t ctx, int64_t closure);

  POLYIR_C_API int64_t polyir_closure_end(int64_t ctx, int64_t closure);

  /* statements: each is added to a block; the ones with bodies give the blocks to fill */
  POLYIR_C_API int64_t polyir_let(int64_t ctx, int64_t block, const char* name, int64_t value);

  POLYIR_C_API int64_t polyir_var(int64_t ctx, int64_t block, const char* name, int64_t type, int64_t init); /* init may be 0 */

  POLYIR_C_API int64_t polyir_assign(int64_t ctx, int64_t block, int64_t target, int64_t value);

  POLYIR_C_API int64_t polyir_set(int64_t ctx, int64_t block, const char* name, int64_t value);

  POLYIR_C_API int64_t polyir_ret(int64_t ctx, int64_t block, int64_t value);

  POLYIR_C_API int64_t polyir_ret_void(int64_t ctx, int64_t block);

  POLYIR_C_API int64_t polyir_eval(int64_t ctx, int64_t block, int64_t value);

  POLYIR_C_API int64_t polyir_if(int64_t ctx, int64_t block, int64_t condition);     /* an if handle */

  POLYIR_C_API int64_t polyir_if_then(int64_t ctx, int64_t if_handle);               /* the block of the true branch */

  POLYIR_C_API int64_t polyir_if_else(int64_t ctx, int64_t if_handle);               /* the block of the false branch */

  POLYIR_C_API int64_t polyir_while(int64_t ctx, int64_t block, int64_t condition);  /* the block of the body */

  POLYIR_C_API int64_t polyir_for_range(int64_t ctx, int64_t block, const char* counter, int64_t from, int64_t to); /* the block of the body */

  POLYIR_C_API int64_t polyir_set_index(int64_t ctx, int64_t block, int64_t base, int64_t indices, int64_t value);

  POLYIR_C_API int64_t polyir_try(int64_t ctx, int64_t block, const char* caught); /* a try handle */

  POLYIR_C_API int64_t polyir_try_body(int64_t ctx, int64_t try_handle);          /* the block of the protected statements */

  POLYIR_C_API int64_t polyir_try_catch(int64_t ctx, int64_t try_handle);         /* the block that runs with the thrown value (an i64) in scope */

  POLYIR_C_API int64_t polyir_throw(int64_t ctx, int64_t block, int64_t value);

  POLYIR_C_API int64_t polyir_yield(int64_t ctx, int64_t block, int64_t value);

  POLYIR_C_API int64_t polyir_label(int64_t ctx, int64_t block, const char* name);

  POLYIR_C_API int64_t polyir_label_param(int64_t ctx, int64_t block, const char* label, const char* name, int64_t type); /* call right after polyir_label */

  POLYIR_C_API int64_t polyir_jump(int64_t ctx, int64_t block, const char* label, int64_t args);

  POLYIR_C_API int64_t polyir_branch(int64_t ctx, int64_t block, int64_t condition, const char* then_label, int64_t then_args, const char* else_label, int64_t else_args);

  /* a program written as polyir source text instead of built call by call: 1 when it parses and checks, else 0. On 0, polyir_error() has what the
   * command line prints (`check failed:` or `compile failed:` and the messages indented under it). A checked program replaces what was built: polyir_hir_text,
   * polyir_compile and polyir_build_executable then work on it. polyir_check_file resolves `import "x.poly"` relative to the file. */
  POLYIR_C_API int32_t polyir_check_source(int64_t ctx, const char* text);

  POLYIR_C_API int32_t polyir_check_file(int64_t ctx, const char* path);

  /* the program so far as text, in the notation the Nina implementation prints for --emit-hir; valid until the next call to this function or free */
  POLYIR_C_API const char* polyir_hir_text(int64_t ctx);

  /* compile the program so far to LLVM IR: 1 when it worked, else 0 and polyir_error() says why */
  POLYIR_C_API int32_t polyir_compile(int64_t ctx);

  POLYIR_C_API const char* polyir_output(int64_t ctx); /* the IR text, valid until the next compile or free */

  POLYIR_C_API int64_t polyir_output_length(int64_t ctx);

  /* compile and build a native executable at `path` (`<path>.ll` is written beside it) with the C compiler $NINA_CC or `clang`. It returns 1 when it worked,
   * else 0 and polyir_error() says why. release is -O2, otherwise -O0 -g. polyir_run_executable runs one and returns its exit status. */
  POLYIR_C_API int32_t polyir_build_executable(int64_t ctx, const char* path, int32_t release);

  POLYIR_C_API int32_t polyir_run_executable(const char* path);

#ifdef __cplusplus
}
#endif

#endif
