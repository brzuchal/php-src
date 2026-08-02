/*
  +----------------------------------------------------------------------+
  | Copyright © The PHP Group and Contributors.                          |
  +----------------------------------------------------------------------+
  | This source file is subject to the Modified BSD License that is      |
  | bundled with this package in the file LICENSE, and is available      |
  | through the World Wide Web at <https://www.php.net/license/>.        |
  |                                                                      |
  | SPDX-License-Identifier: BSD-3-Clause                                |
  +----------------------------------------------------------------------+
*/

#include "ext/opcache/zend_accelerator_api.h"
#include "zend_API.h"
#include "zend_modules.h"
#include "zend_types.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_test.h"
#include "observer.h"
#include "fiber.h"
#include "iterators.h"
#include "object_handlers.h"
#include "zend_attributes.h"
#include "zend_enum.h"
#include "zend_vec.h"
#include "zend_collection_info.h"
#include "zend_type_info.h"
#include "zend_interfaces.h"
#include "zend_weakrefs.h"
#include "Zend/Optimizer/zend_optimizer.h"
#include "Zend/zend_alloc.h"
#include "test_arginfo.h"
#include "tmp_methods_arginfo.h"
#include "zend_call_stack.h"
#include "zend_exceptions.h"
#include "zend_mm_custom_handlers.h"
#include "ext/uri/php_uri.h"
#include "zend_observer.h"
#include "test_decl.h"

#if defined(HAVE_LIBXML) && !defined(PHP_WIN32)
# include <libxml/globals.h>
# include <libxml/parser.h>
#endif

ZEND_DECLARE_MODULE_GLOBALS(zend_test)

static zend_class_entry *zend_test_interface;
static zend_class_entry *zend_test_class;
static zend_class_entry *zend_test_child_class;
static zend_class_entry *zend_test_gen_stub_flag_compatibility_test;
static zend_class_entry *zend_attribute_test_class;
static zend_class_entry *zend_test_trait;
static zend_class_entry *zend_test_attribute;
static zend_class_entry *zend_test_repeatable_attribute;
static zend_class_entry *zend_test_parameter_attribute;
static zend_class_entry *zend_test_property_attribute;
static zend_class_entry *zend_test_attribute_with_arguments;
static zend_class_entry *zend_test_class_with_method_with_parameter_attribute;
static zend_class_entry *zend_test_child_class_with_method_with_parameter_attribute;
static zend_class_entry *zend_test_class_with_property_attribute;
static zend_class_entry *zend_test_forbid_dynamic_call;
static zend_class_entry *zend_test_ns_foo_class;
static zend_class_entry *zend_test_ns_unlikely_compile_error_class;
static zend_class_entry *zend_test_ns_not_unlikely_compile_error_class;
static zend_class_entry *zend_test_ns_bar_class;
static zend_class_entry *zend_test_ns2_foo_class;
static zend_class_entry *zend_test_ns2_ns_foo_class;
static zend_class_entry *zend_test_unit_enum;
static zend_class_entry *zend_test_string_enum;
static zend_class_entry *zend_test_int_enum;
static zend_class_entry *zend_test_enum_with_interface;
static zend_class_entry *zend_test_magic_call;
static zend_object_handlers zend_test_class_handlers;

static int le_throwing_resource;

static ZEND_FUNCTION(zend_test_func)
{
	RETVAL_STR_COPY(EX(func)->common.function_name);

	/* Cleanup trampoline */
	ZEND_ASSERT(EX(func)->common.fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE);
	zend_string_release(EX(func)->common.function_name);
	zend_free_trampoline(EX(func));
	EX(func) = NULL;
}

static ZEND_FUNCTION(zend_trigger_bailout)
{
	ZEND_PARSE_PARAMETERS_NONE();

	zend_error(E_ERROR, "Bailout");
}

static ZEND_FUNCTION(zend_test_array_return)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_FUNCTION(zend_test_nullable_array_return)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_NULL();
}

static ZEND_FUNCTION(zend_test_void_return)
{
	/* dummy */
	ZEND_PARSE_PARAMETERS_NONE();
}

static void pass1(zend_script *script, void *context)
{
	php_printf("pass1\n");
}

static void pass2(zend_script *script, void *context)
{
	php_printf("pass2\n");
}

static ZEND_FUNCTION(zend_test_deprecated)
{
	zval *arg1;

	zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &arg1);
}

static ZEND_FUNCTION(zend_test_deprecated_attr)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_FUNCTION(zend_test_nodiscard)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(1);
}

static ZEND_FUNCTION(zend_test_deprecated_nodiscard)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(1);
}

/* Create a string without terminating null byte. Must be terminated with
 * zend_terminate_string() before destruction, otherwise a warning is issued
 * in debug builds. */
static ZEND_FUNCTION(zend_create_unterminated_string)
{
	zend_string *str, *res;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &str) == FAILURE) {
		RETURN_THROWS();
	}

	res = zend_string_alloc(ZSTR_LEN(str), 0);
	memcpy(ZSTR_VAL(res), ZSTR_VAL(str), ZSTR_LEN(str));
	/* No trailing null byte */

	RETURN_STR(res);
}

/* Enforce terminate null byte on string. This avoids a warning in debug builds. */
static ZEND_FUNCTION(zend_terminate_string)
{
	zend_string *str;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &str) == FAILURE) {
		RETURN_THROWS();
	}

	ZSTR_VAL(str)[ZSTR_LEN(str)] = '\0';
}

/* Cause an intentional memory leak, for testing/debugging purposes */
static ZEND_FUNCTION(zend_leak_bytes)
{
	zend_long leakbytes = 3;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &leakbytes) == FAILURE) {
		RETURN_THROWS();
	}

	emalloc(leakbytes);
}

/* Leak a refcounted variable */
static ZEND_FUNCTION(zend_leak_variable)
{
	zval *zv;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zv) == FAILURE) {
		RETURN_THROWS();
	}

	if (!Z_REFCOUNTED_P(zv)) {
		zend_error(E_WARNING, "Cannot leak variable that is not refcounted");
		return;
	}

	Z_ADDREF_P(zv);
}

static ZEND_FUNCTION(zend_delref)
{
	zval *zv;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zv) == FAILURE) {
		RETURN_THROWS();
	}

	Z_TRY_DELREF_P(zv);

	RETURN_NULL();
}

/* Tests Z_PARAM_OBJ_OR_STR */
static ZEND_FUNCTION(zend_string_or_object)
{
	zend_string *str;
	zend_object *object;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OR_STR(object, str)
	ZEND_PARSE_PARAMETERS_END();

	if (str) {
		RETURN_STR_COPY(str);
	} else {
		RETURN_OBJ_COPY(object);
	}
}

/* Tests Z_PARAM_OBJ_OR_STR_OR_NULL */
static ZEND_FUNCTION(zend_string_or_object_or_null)
{
	zend_string *str;
	zend_object *object;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OR_STR_OR_NULL(object, str)
	ZEND_PARSE_PARAMETERS_END();

	if (str) {
		RETURN_STR_COPY(str);
	} else if (object) {
		RETURN_OBJ_COPY(object);
	} else {
		RETURN_NULL();
	}
}

/* Tests Z_PARAM_OBJ_OF_CLASS_OR_STR */
static ZEND_FUNCTION(zend_string_or_stdclass)
{
	zend_string *str;
	zend_object *object;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR(object, zend_standard_class_def, str)
	ZEND_PARSE_PARAMETERS_END();

	if (str) {
		RETURN_STR_COPY(str);
	} else {
		RETURN_OBJ_COPY(object);
	}
}

static ZEND_FUNCTION(zend_test_compile_string)
{
	zend_string *source_string = NULL;
	zend_string *filename = NULL;
	zend_long position = ZEND_COMPILE_POSITION_AT_OPEN_TAG;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_STR(source_string)
		Z_PARAM_PATH_STR(filename)
		Z_PARAM_LONG(position)
	ZEND_PARSE_PARAMETERS_END();

	zend_op_array *op_array = NULL;

	op_array = compile_string(source_string, ZSTR_VAL(filename), position);

	if (op_array) {
		zval retval;

		zend_try {
			ZVAL_UNDEF(&retval);
			zend_execute(op_array, &retval);
		} zend_catch {
			destroy_op_array(op_array);
			efree_size(op_array, sizeof(zend_op_array));
			zend_bailout();
		} zend_end_try();

		destroy_op_array(op_array);
		efree_size(op_array, sizeof(zend_op_array));
	}

	return;
}

/* Tests Z_PARAM_OBJ_OF_CLASS_OR_STR_OR_NULL */
static ZEND_FUNCTION(zend_string_or_stdclass_or_null)
{
	zend_string *str;
	zend_object *object;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR_OR_NULL(object, zend_standard_class_def, str)
	ZEND_PARSE_PARAMETERS_END();

	if (str) {
		RETURN_STR_COPY(str);
	} else if (object) {
		RETURN_OBJ_COPY(object);
	} else {
		RETURN_NULL();
	}
}

/* Tests Z_PARAM_NUMBER_OR_STR */
static ZEND_FUNCTION(zend_number_or_string)
{
	zval *input;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_NUMBER_OR_STR(input)
	ZEND_PARSE_PARAMETERS_END();

	switch (Z_TYPE_P(input)) {
		case IS_LONG:
			RETURN_LONG(Z_LVAL_P(input));
		case IS_DOUBLE:
			RETURN_DOUBLE(Z_DVAL_P(input));
		case IS_STRING:
			RETURN_STR_COPY(Z_STR_P(input));
		default: ZEND_UNREACHABLE();
	}
}

/* Tests Z_PARAM_NUMBER_OR_STR_OR_NULL */
static ZEND_FUNCTION(zend_number_or_string_or_null)
{
	zval *input;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_NUMBER_OR_STR_OR_NULL(input)
	ZEND_PARSE_PARAMETERS_END();

	if (!input) {
		RETURN_NULL();
	}

	switch (Z_TYPE_P(input)) {
		case IS_LONG:
			RETURN_LONG(Z_LVAL_P(input));
		case IS_DOUBLE:
			RETURN_DOUBLE(Z_DVAL_P(input));
		case IS_STRING:
			RETURN_STR_COPY(Z_STR_P(input));
		default: ZEND_UNREACHABLE();
	}
}

static ZEND_FUNCTION(zend_weakmap_attach)
{
	zval *value;
	zend_object *obj;

	ZEND_PARSE_PARAMETERS_START(2, 2)
			Z_PARAM_OBJ(obj)
			Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	if (zend_weakrefs_hash_add(ZT_G(global_weakmap), obj, value)) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

static ZEND_FUNCTION(zend_weakmap_remove)
{
	zend_object *obj;

	ZEND_PARSE_PARAMETERS_START(1, 1)
			Z_PARAM_OBJ(obj)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_BOOL(zend_weakrefs_hash_del(ZT_G(global_weakmap), obj) == SUCCESS);
}

static ZEND_FUNCTION(zend_weakmap_dump)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_ARR(zend_array_dup(ZT_G(global_weakmap)));
}

static ZEND_FUNCTION(zend_get_current_func_name)
{
    ZEND_PARSE_PARAMETERS_NONE();

    zend_string *function_name = get_function_or_method_name(EG(current_execute_data)->prev_execute_data->func);

    RETURN_STR(function_name);
}

#if defined(HAVE_LIBXML) && !defined(PHP_WIN32)
static ZEND_FUNCTION(zend_test_override_libxml_global_state)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ZEND_DIAGNOSTIC_IGNORED_START("-Wdeprecated-declarations")
	xmlLoadExtDtdDefaultValue = 1;
	xmlDoValidityCheckingDefaultValue = 1;
	(void) xmlPedanticParserDefault(1);
	(void) xmlSubstituteEntitiesDefault(1);
	(void) xmlLineNumbersDefault(1);
	(void) xmlKeepBlanksDefault(0);
	ZEND_DIAGNOSTIC_IGNORED_END
}
#endif

/* TESTS Z_PARAM_ITERABLE and Z_PARAM_ITERABLE_OR_NULL */
static ZEND_FUNCTION(zend_iterable)
{
	zval *arg1, *arg2;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ITERABLE(arg1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ITERABLE_OR_NULL(arg2)
	ZEND_PARSE_PARAMETERS_END();
}

static ZEND_FUNCTION(zend_iterable_legacy)
{
	zval *arg1, *arg2;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ITERABLE(arg1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ITERABLE_OR_NULL(arg2)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_COPY(arg1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_zend_iterable_legacy, 0, 1, IS_ITERABLE, 0)
	ZEND_ARG_TYPE_INFO(0, arg1, IS_ITERABLE, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, arg2, IS_ITERABLE, 1, "null")
ZEND_END_ARG_INFO()

static const zend_function_entry ext_function_legacy[] = {
	ZEND_FE(zend_iterable_legacy, arginfo_zend_iterable_legacy)
	ZEND_FE_END
};

/* Call a method on a class or object using zend_call_method() */
static ZEND_FUNCTION(zend_call_method)
{
	zend_string *method_name;
	zval *class_or_object, *arg1 = NULL, *arg2 = NULL;
	zend_object *obj = NULL;
	zend_class_entry *ce = NULL;
	int argc = ZEND_NUM_ARGS();

	ZEND_PARSE_PARAMETERS_START(2, 4)
		Z_PARAM_ZVAL(class_or_object)
		Z_PARAM_STR(method_name)
		Z_PARAM_OPTIONAL
		Z_PARAM_ZVAL(arg1)
		Z_PARAM_ZVAL(arg2)
	ZEND_PARSE_PARAMETERS_END();

	if (Z_TYPE_P(class_or_object) == IS_OBJECT) {
		obj = Z_OBJ_P(class_or_object);
		ce = obj->ce;
	} else if (Z_TYPE_P(class_or_object) == IS_STRING) {
		ce = zend_lookup_class(Z_STR_P(class_or_object));
		if (!ce) {
			zend_error_noreturn(E_ERROR, "Unknown class '%s'", Z_STRVAL_P(class_or_object));
		}
	} else {
		zend_argument_type_error(1, "must be of type object|string, %s given", zend_zval_value_name(class_or_object));
		return;
	}

	ZEND_ASSERT((argc >= 2) && (argc <= 4));
	zend_call_method(obj, ce, NULL, ZSTR_VAL(method_name), ZSTR_LEN(method_name), return_value, argc - 2, arg1, arg2);
}

/* Instantiate a class and run the constructor via object_init_with_constructor */
static ZEND_FUNCTION(zend_object_init_with_constructor)
{
	zend_class_entry *ce = NULL;
	zval *args;
	uint32_t num_args;
	HashTable *named_args;

	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_CLASS(ce)
		Z_PARAM_VARIADIC_WITH_NAMED(args, num_args, named_args)
	ZEND_PARSE_PARAMETERS_END();

	zval obj;
	/* We don't use return_value directly to check for memory leaks of the API on failure */
	zend_result status = object_init_with_constructor(&obj, ce, num_args, args, named_args);
	if (status == FAILURE) {
		RETURN_THROWS();
	}
	ZEND_ASSERT(!EG(exception));
	ZVAL_COPY_VALUE(return_value, &obj);
}

static ZEND_FUNCTION(zend_call_method_if_exists)
{
	zend_object *obj = NULL;
	zend_string *method_name;
	uint32_t num_args = 0;
	zval *args = NULL;
	ZEND_PARSE_PARAMETERS_START(2, -1)
		Z_PARAM_OBJ(obj)
		Z_PARAM_STR(method_name)
		Z_PARAM_VARIADIC('*', args, num_args)
	ZEND_PARSE_PARAMETERS_END();

	zend_result status = zend_call_method_if_exists(obj, method_name, return_value, num_args, args);
	if (status == FAILURE) {
		ZEND_ASSERT(Z_ISUNDEF_P(return_value));
		if (EG(exception)) {
			RETURN_THROWS();
		}
		RETURN_NULL();
	}
	if (Z_TYPE_P(return_value) == IS_REFERENCE) {
		zend_unwrap_reference(return_value);
	}
}

static ZEND_FUNCTION(zend_test_call_with_consumed_args)
{
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	zval *args;
	zend_long consumed_args;
	zval retval;
	uint32_t actual_consumed_args = 0;
	uint32_t i;
	zend_result call_result;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_FUNC(fci, fcc)
		Z_PARAM_ARRAY(args)
		Z_PARAM_LONG(consumed_args)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(consumed_args < 0 || consumed_args > UINT32_MAX)) {
		zend_argument_value_error(3, "must be between 0 and 4294967295");
		RETURN_THROWS();
	}

	zend_fcall_info_args(&fci, args);

	ZVAL_UNDEF(&retval);
	fci.retval = &retval;
	fci.consumed_args = (uint32_t) consumed_args;

	call_result = zend_call_function(&fci, &fcc);

	for (i = 0; i < fci.param_count && i < 32; i++) {
		if (Z_ISUNDEF(fci.params[i])) {
			actual_consumed_args |= (1u << i);
		}
	}

	zend_fcall_info_args_clear(&fci, true);

	if (call_result == FAILURE || EG(exception)) {
		if (!Z_ISUNDEF(retval)) {
			zval_ptr_dtor(&retval);
		}
		RETURN_THROWS();
	}

	array_init(return_value);
	add_assoc_long(return_value, "consumed_args", actual_consumed_args);

	if (Z_ISUNDEF(retval)) {
		add_assoc_null(return_value, "retval");
	} else {
		add_assoc_zval(return_value, "retval", &retval);
	}
}

static ZEND_FUNCTION(zend_test_refcount)
{
	zval *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	if (!Z_REFCOUNTED_P(value)) {
		RETURN_LONG(-1);
	}

	RETURN_LONG(Z_REFCOUNT_P(value));
}

/* Destroy a vec through the ordinary refcounted-zval path, so the GC dtor slot
 * (rc_dtor_func -> zend_vec_destroy) is exercised, not just a direct call. */
static void zend_test_vec_release(zend_vec *vec)
{
	zval z;

	/* Must go through ZVAL_VEC: a hand-built type_info that omits
	 * IS_TYPE_COLLECTABLE would make this the one vec zval the collector cannot
	 * see, which is exactly the invariant every other path upholds. */
	ZVAL_VEC(&z, vec);
	zval_ptr_dtor(&z);
}

/* Build a vec through the only exported construction entry point. Reserving
 * storage and installing elements are private to zend_vec.c; the invariants of
 * that two-step path are covered by zend_vec_lifecycle_selftest() instead. */
static zend_vec *zend_test_vec_build(zend_type elem, zval *vals, uint32_t n)
{
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} probe;
	zend_type probe_type = ZEND_TYPE_INIT_NONE(0);
	const zend_collection_info *info;
	HashTable ht;
	zend_vec *vec;

	/* Promotion is the only way in: the descriptor is a stack temporary, and
	 * the value receives a borrowed canonical node instead of a copy of it. */
	probe.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	probe.desc.num_types = 1;
	probe.desc.types[0] = elem;
	ZEND_TYPE_SET_COLLECTION(probe_type, &probe.desc);

	info = zend_collection_info_intern(probe_type);
	if (!info) {
		return NULL;
	}

	zend_hash_init(&ht, n ? n : 1, NULL, ZVAL_PTR_DTOR, 0);
	for (uint32_t i = 0; i < n; i++) {
		zend_hash_next_index_insert_new(&ht, &vals[i]);
	}
	vec = zend_vec_create(&ht, info, NULL);
	zend_hash_destroy(&ht);
	return vec;
}

/* Self-test for the vec runtime representation (commit: vec payload). Exercises
 * allocation, element storage, builtin and named-class element-type metadata,
 * ownership of a class-name zend_string, element destruction and empty vecs,
 * entirely in C. Returns a map of scenario => bool so a .phpt can assert each
 * path actually ran and passed. */
static ZEND_FUNCTION(zend_test_vec_selftest)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* 1. Builtin element type: allocate, verify count and element_type,
	 *    populate and read back, destroy. */
	{
		zend_type int_type = ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
		zval vals[3];
		zend_vec *vec;
		bool ok;

		for (uint32_t i = 0; i < 3; i++) {
			ZVAL_LONG(&vals[i], (zend_long) (i + 10));
		}
		vec = zend_test_vec_build(int_type, vals, 3);
		ok = vec != NULL && ZEND_VEC_COUNT(vec) == 3
			&& (ZEND_TYPE_FULL_MASK(ZEND_VEC_ELEMENT_TYPE(vec)) & _ZEND_TYPE_MAY_BE_MASK)
				== (1u << IS_LONG);
		for (uint32_t i = 0; i < 3; i++) {
			ok = ok && Z_TYPE(vec->elements[i]) == IS_LONG
				&& Z_LVAL(vec->elements[i]) == (zend_long) (i + 10);
		}
		zend_test_vec_release(vec);
		add_assoc_bool(return_value, "builtin", ok);
	}

	/* 2. Named-class element type: balanced ownership. The vec takes its own
	 *    reference to the class-name string; destroying the vec releases
	 *    exactly that reference and no other; the caller's reference remains
	 *    valid afterward.
	 *
	 *    A request-local, non-interned string is used so the addref/release is
	 *    observable (interned strings would no-op). The exact refcount values
	 *    below are an internal white-box check of that balance, not a public or
	 *    architectural contract: the invariant being verified is that ownership
	 *    is balanced (net zero across the vec's lifetime), whatever the caller's
	 *    starting refcount happens to be. */
	{
		zend_string *foo = zend_string_init("Foo", sizeof("Foo") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		zend_type foo_type = ZEND_TYPE_INIT_CLASS(foo, 0, 0);
		bool ok = zend_vec_type_is_supported(foo_type);
		zend_vec *vec = zend_test_vec_build(foo_type, NULL, 0);

		/* Ownership moved: the canonical node holds the class-name reference,
		 * not the value. Promotion takes it once per distinct type per request
		 * (or not at all when the node already existed), so this asserts only
		 * that the caller's reference was not consumed. */
		uint32_t rc_after_build = GC_REFCOUNT(foo);
		ok = ok && rc_after_build >= rc_caller;
		zend_test_vec_release(vec);
		/* INV-5: destroying a value does no type-ownership work at all, so the
		 * node's reference is untouched by the release above. */
		ok = ok && GC_REFCOUNT(foo) == rc_after_build;

		zend_string_release(foo);
		add_assoc_bool(return_value, "named_ownership", ok);
	}

	/* 3. Element destruction: destroying a vec must release a refcounted
	 *    element (a request-local string) exactly once. */
	{
		zend_string *elem = zend_string_init("elem", sizeof("elem") - 1, 0);
		zend_type str_type = ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
		zval tmp;
		zend_vec *vec;
		uint32_t rc_before;

		ZVAL_STR_COPY(&tmp, elem);
		vec = zend_test_vec_build(str_type, &tmp, 1);  /* vec takes its own ref */
		rc_before = GC_REFCOUNT(elem);
		zend_test_vec_release(vec);
		add_assoc_bool(return_value, "element_dtor",
			GC_REFCOUNT(elem) == rc_before - 1);

		zend_string_release(elem);
	}

	/* 4. Empty vec: count == 0 must allocate a valid header and destroy
	 *    cleanly. */
	{
		zend_type int_type = ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
		zend_vec *vec = zend_test_vec_build(int_type, NULL, 0);
		bool ok = vec != NULL && ZEND_VEC_COUNT(vec) == 0;

		zend_test_vec_release(vec);
		add_assoc_bool(return_value, "empty", ok);
	}

	/* 5. Internal construction invariants. alloc/append are private to
	 *    zend_vec.c, so the engine runs these checks itself and reports which
	 *    passed, rather than the API being widened for the tests. */
	{
		uint32_t bits = zend_vec_lifecycle_selftest();

		add_assoc_bool(return_value, "alloc_starts_empty",
			(bits & ZEND_VEC_SELFTEST_ALLOC_EMPTY) != 0);
		add_assoc_bool(return_value, "failed_append_inert",
			(bits & ZEND_VEC_SELFTEST_FAILED_APPEND_INERT) != 0);
		add_assoc_bool(return_value, "destroys_only_installed",
			(bits & ZEND_VEC_SELFTEST_ONLY_INSTALLED) != 0);
		add_assoc_bool(return_value, "element_dtor_exactly_once",
			(bits & ZEND_VEC_SELFTEST_DTOR_EXACTLY_ONCE) != 0);
	}

	/* 6. Validator: reuse zend_vec_type_is_supported (do not duplicate it). */
	{
		zend_type ok_type = ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
		zend_type bad_type = ZEND_TYPE_INIT_CODE(IS_CALLABLE, 0, 0);
		add_assoc_bool(return_value, "validator",
			zend_vec_type_is_supported(ok_type)
			&& !zend_vec_type_is_supported(bad_type));
	}

	/* 7. Hybrid ownership. A hybrid root over a shared flat base --
	 *    built and torn down entirely in C, since no PHP surface constructs one
	 *    yet -- must share the base (never copy it), take the tail's sole ref,
	 *    release each child exactly once, and give branches independent lifetimes. */
	{
		uint32_t bits = zend_hybrid_lifecycle_selftest();

		add_assoc_bool(return_value, "hybrid_tagged",
			(bits & ZEND_HYBRID_SELFTEST_TAGGED_HYBRID) != 0);
		add_assoc_bool(return_value, "hybrid_base_shared",
			(bits & ZEND_HYBRID_SELFTEST_BASE_SHARED) != 0);
		add_assoc_bool(return_value, "hybrid_tail_owned",
			(bits & ZEND_HYBRID_SELFTEST_TAIL_OWNED) != 0);
		add_assoc_bool(return_value, "hybrid_branch_independent",
			(bits & ZEND_HYBRID_SELFTEST_BRANCH_INDEP) != 0);
		add_assoc_bool(return_value, "hybrid_dtor_balanced",
			(bits & ZEND_HYBRID_SELFTEST_DTOR_BALANCED) != 0);
	}

	/* 8. Flatten policy. The append dispatcher must never publish a hybrid
	 *    that violates a policy bound: a retained append to an EMPTY vec stays
	 *    FLAT (tail > R*base is forbidden at base_count == 0), while the same
	 *    append to a non-empty vec yields a HYBRID sharing that base. */
	{
		uint32_t bits = zend_hybrid_policy_selftest();

		add_assoc_bool(return_value, "policy_empty_base_flat",
			(bits & ZEND_HYBRID_POLICY_SELFTEST_EMPTY_BASE_FLAT) != 0);
		add_assoc_bool(return_value, "policy_retained_hybrid",
			(bits & ZEND_HYBRID_POLICY_SELFTEST_RETAINED_HYBRID) != 0);
	}
}

/* Build a collection type wrapping a single element type. Ownership of any
 * refcounted parts of `elem` transfers to the descriptor. vec has arity one. */
static zend_type zend_test_make_collection(uint32_t kind, zend_type elem)
{
	zend_collection_type *desc = zend_type_collection_alloc(kind, 1, /* persistent */ false);
	zend_type t = ZEND_TYPE_INIT_NONE(0);

	desc->types[0] = elem;
	ZEND_TYPE_SET_COLLECTION(t, desc);
	return t;
}

/* Self-test for the collection-type descriptor (commit: internal collection
 * type representation). Exercises the reachable lifecycle in C: construction,
 * recursive release, class-name ownership, arena-backed release, stringification
 * and the discriminator macros. Deep-copy via zend_type_copy_ctor and opcache
 * persistence are NOT exercised here: both require a collection type to appear
 * in a signature, which needs declaration syntax (a later commit). */
static ZEND_FUNCTION(zend_test_collection_type_selftest)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* 1. vec[int] construction and descriptor invariants. */
	{
		zend_type t = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC,
			(zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0));
		zend_collection_type *desc = ZEND_TYPE_COLLECTION(t);
		bool ok = ZEND_TYPE_HAS_LIST(t)                       /* is list-shaped */
			&& !ZEND_TYPE_IS_TYPE_LIST(t)                     /* but not a real list */
			&& ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(t)         /* it is a collection */
			&& desc->kind == ZEND_COLLECTION_TYPE_VEC
			&& desc->num_types == 1
			&& (ZEND_TYPE_FULL_MASK(desc->types[0]) & _ZEND_TYPE_MAY_BE_MASK)
				== (1u << IS_LONG);
		add_assoc_bool(return_value, "construction", ok);
		zend_type_release(t, /* persistent */ false);
	}

	/* 2. vec[Foo] class-name ownership: the descriptor owns one reference to the
	 *    element's class name, and releasing the descriptor releases exactly
	 *    that reference (net-zero for the caller). */
	{
		zend_string *foo = zend_string_init("Foo", sizeof("Foo") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		zend_type elem = ZEND_TYPE_INIT_CLASS(foo, 0, 0);
		zend_string_addref(foo);                 /* the descriptor's own reference */
		zend_type t = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC, elem);

		bool ok = GC_REFCOUNT(foo) == rc_caller + 1;
		zend_type_release(t, /* persistent */ false);   /* releases the descriptor's ref */
		ok = ok && GC_REFCOUNT(foo) == rc_caller;

		zend_string_release(foo);
		add_assoc_bool(return_value, "named_ownership", ok);
	}

	/* 3. Independent destruction: two descriptors over the same class name each
	 *    own a distinct reference; releasing one does not affect the other. */
	{
		zend_string *foo = zend_string_init("Bar", sizeof("Bar") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);

		zend_string_addref(foo);
		zend_type a = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC,
			(zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0));
		zend_string_addref(foo);
		zend_type b = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC,
			(zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0));

		bool ok = ZEND_TYPE_COLLECTION(a) != ZEND_TYPE_COLLECTION(b)   /* distinct */
			&& GC_REFCOUNT(foo) == rc_caller + 2;
		zend_type_release(a, /* persistent */ false);
		ok = ok && GC_REFCOUNT(foo) == rc_caller + 1;   /* b's ref untouched */
		zend_type_release(b, /* persistent */ false);
		ok = ok && GC_REFCOUNT(foo) == rc_caller;

		zend_string_release(foo);
		add_assoc_bool(return_value, "independent_destruction", ok);
	}

	/* 4. Arena-backed descriptor: releasing it must release the element's name
	 *    but must NOT free the descriptor storage (the arena owns it). ASAN in a
	 *    debug build proves no invalid free happens here. */
	{
		zend_string *foo = zend_string_init("Baz", sizeof("Baz") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		size_t size = ZEND_TYPE_COLLECTION_SIZE(1);
		zend_collection_type *desc = zend_arena_alloc(&CG(arena), size);
		zend_type t = ZEND_TYPE_INIT_NONE(0);

		desc->kind = ZEND_COLLECTION_TYPE_VEC;
		desc->num_types = 1;
		zend_string_addref(foo);
		desc->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0);
		ZEND_TYPE_SET_COLLECTION(t, desc);
		ZEND_TYPE_FULL_MASK(t) |= _ZEND_TYPE_ARENA_BIT;

		zend_type_release(t, /* persistent */ false);   /* releases name, keeps arena mem */
		bool ok = GC_REFCOUNT(foo) == rc_caller;        /* name ref balanced */

		zend_string_release(foo);
		add_assoc_bool(return_value, "arena_release", ok);
	}

	/* 5. Stringification: vec[int] and vec[Foo]. */
	{
		zend_type ti = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC,
			(zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0));
		zend_string *si = zend_type_to_string(ti);
		bool ok = zend_string_equals_literal(si, "vec[int]");
		zend_string_release(si);
		zend_type_release(ti, /* persistent */ false);

		zend_string *foo = zend_string_init("Foo", sizeof("Foo") - 1, 0);
		zend_string_addref(foo);
		zend_type tf = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC,
			(zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0));
		zend_string *sf = zend_type_to_string(tf);
		ok = ok && zend_string_equals_literal(sf, "vec[Foo]");
		zend_string_release(sf);
		zend_type_release(tf, /* persistent */ false);
		zend_string_release(foo);

		add_assoc_bool(return_value, "stringify", ok);
	}

	/* 6. A real union is still recognised as a type list, never a collection. */
	{
		zend_type_list *list = emalloc(ZEND_TYPE_LIST_SIZE(2));
		list->num_types = 2;
		list->types[0] = (zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
		list->types[1] = (zend_type) ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
		zend_type u = ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_LIST(u, list);
		ZEND_TYPE_FULL_MASK(u) |= _ZEND_TYPE_UNION_BIT;

		add_assoc_bool(return_value, "union_is_type_list",
			ZEND_TYPE_IS_TYPE_LIST(u) && !ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(u));
		zend_type_release(u, /* persistent */ false);
	}

	/* 7. A real intersection is still recognised as a type list. */
	{
		zend_string *a = zend_string_init("A", 1, 0);
		zend_string *b = zend_string_init("B", 1, 0);
		zend_type_list *list = emalloc(ZEND_TYPE_LIST_SIZE(2));
		list->num_types = 2;
		list->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(a, 0, 0);
		list->types[1] = (zend_type) ZEND_TYPE_INIT_CLASS(b, 0, 0);
		zend_type it = ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_LIST(it, list);
		ZEND_TYPE_FULL_MASK(it) |= _ZEND_TYPE_INTERSECTION_BIT;

		add_assoc_bool(return_value, "intersection_is_type_list",
			ZEND_TYPE_IS_TYPE_LIST(it) && !ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(it));
		zend_type_release(it, /* persistent */ false);   /* releases a and b */
	}
}

/* Test-only factory producing a real vec value, so PHPT coverage can exercise the
 * accepting side of a vec[T] declaration and not only rejection. There is no
 * literal syntax yet; this is scaffolding, not a language feature. */
static ZEND_FUNCTION(zend_test_make_vec)
{
	HashTable *values;
	zend_string *type_name;
	zval *out;
	zend_type element_type;
	bool owns_type = false;
	zend_collection_type *owns_nested = NULL;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ARRAY_HT(values)
		Z_PARAM_STR(type_name)
		Z_PARAM_ZVAL(out)
	ZEND_PARSE_PARAMETERS_END();

	if (zend_string_equals_literal(type_name, "int")) {
		element_type = (zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
	} else if (zend_string_equals_literal(type_name, "float")) {
		element_type = (zend_type) ZEND_TYPE_INIT_CODE(IS_DOUBLE, 0, 0);
	} else if (zend_string_equals_literal(type_name, "string")) {
		element_type = (zend_type) ZEND_TYPE_INIT_CODE(IS_STRING, 0, 0);
	} else if (zend_string_equals_literal(type_name, "array")) {
		element_type = (zend_type) ZEND_TYPE_INIT_CODE(IS_ARRAY, 0, 0);
	} else if (zend_string_starts_with_literal(type_name, "vec:")) {
		/* "vec:Foo" builds the descriptor for a vec[vec[Foo]] element, so tests
		 * can construct nested collection values. */
		zend_collection_type *desc = zend_type_collection_alloc(
			ZEND_COLLECTION_TYPE_VEC, 1, /* persistent */ false);
		const char *inner = ZSTR_VAL(type_name) + strlen("vec:");

		if (!strcmp(inner, "int")) {
			desc->types[0] = (zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
		} else {
			zend_string *iname = zend_string_init(inner, strlen(inner), 0);
			zend_class_entry *ce = zend_lookup_class(iname);

			zend_string_release(iname);
			if (!ce) {
				pefree(desc, 0);
				zend_argument_value_error(2, "names an unknown inner class");
				RETURN_THROWS();
			}
			desc->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(zend_string_copy(ce->name), 0, 0);
		}
		element_type = (zend_type) ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_COLLECTION(element_type, desc);
		owns_nested = desc;
	} else {
		/* Anything else is taken as a class name, so tests can build vec[Foo]
		 * and exercise cycles through object elements. */
		zend_class_entry *ce = zend_lookup_class(type_name);

		if (!ce) {
			zend_argument_value_error(2, "must name a builtin element type or an existing class");
			RETURN_THROWS();
		}
		element_type = (zend_type) ZEND_TYPE_INIT_CLASS(zend_string_copy(ce->name), 0, 0);
		owns_type = true;
	}

	/* Routed through promotion and then the production constructor, so the
	 * tests exercise the real canonicalization, validation and
	 * partial-construction cleanup rather than a parallel path. */
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} probe;
	zend_type probe_type = ZEND_TYPE_INIT_NONE(0);
	const zend_collection_info *info;
	zend_vec *vec;

	probe.desc.kind = ZEND_COLLECTION_TYPE_VEC;
	probe.desc.num_types = 1;
	probe.desc.types[0] = element_type;
	ZEND_TYPE_SET_COLLECTION(probe_type, &probe.desc);

	info = zend_collection_info_intern(probe_type);
	vec = info ? zend_vec_create(values, info, NULL) : NULL;

	if (owns_type) {
		/* The canonical node took its own reference; drop the local one. */
		zend_string_release(ZEND_TYPE_NAME(element_type));
	}
	if (owns_nested) {
		zend_type_release(owns_nested->types[0], /* persistent */ false);
		pefree(owns_nested, 0);
	}
	if (!vec) {
		zend_argument_value_error(1,
			"must contain only values matching the requested element type");
		RETURN_THROWS();
	}

	/* Written through a by-ref out parameter: a collection is not `mixed`, so it
	 * cannot be returned through a declared internal return type. */
	zval vec_zv;
	ZVAL_VEC(&vec_zv, vec);

	ZVAL_DEREF(out);
	zval_ptr_dtor(out);
	ZVAL_COPY_VALUE(out, &vec_zv);
}

/* Read-only inspection of a collection value, for lifecycle tests only. There is
 * no public collection API yet; these exist so PHPTs can observe count, element
 * identity and refcounts without one. */
static ZEND_FUNCTION(zend_test_vec_count)
{
	zval *v;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(v)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DEREF(v);
	if (Z_TYPE_P(v) != IS_COLLECTION) {
		zend_argument_type_error(1, "must be a collection");
		RETURN_THROWS();
	}
	ZEND_ASSERT(GC_TYPE(Z_COUNTED_P(v)) == IS_VEC_GC);
	RETURN_LONG((zend_long) ZEND_VEC_COUNT(Z_VEC_P(v)));
}

static ZEND_FUNCTION(zend_test_vec_get)
{
	zval *v;
	zend_long idx;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_ZVAL(v)
		Z_PARAM_LONG(idx)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DEREF(v);
	if (Z_TYPE_P(v) != IS_COLLECTION) {
		zend_argument_type_error(1, "must be a collection");
		RETURN_THROWS();
	}
	if (idx < 0 || (uint32_t) idx >= ZEND_VEC_COUNT(Z_VEC_P(v))) {
		zend_argument_value_error(2, "is out of range");
		RETURN_THROWS();
	}
	/* Logical-position read via the storage contract: on a HYBRID root the
	 * elements[] overlay holds the two child collection zvals, not logical
	 * elements, so a raw elements[idx] would be type confusion (idx 0..1) or
	 * out of bounds (idx >= 2). */
	RETURN_COPY(zend_stor_get(Z_VEC_P(v), (uint32_t) idx));
}

/* TEMPORARY (benchmark-only, uncommitted): report the storage representation of
 * a vec value so the C1 benchmark can PROVE a value is hybrid instead of
 * inferring it from timing. flat: kind/count/capacity/tail=0; hybrid:
 * kind/count/base/tail. */
static ZEND_FUNCTION(zend_test_vec_repr)
{
	zval *v;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(v)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DEREF(v);
	if (Z_TYPE_P(v) != IS_COLLECTION) {
		zend_argument_type_error(1, "must be a collection");
		RETURN_THROWS();
	}
	zend_vec *vec = Z_VEC_P(v);
	array_init(return_value);
	add_assoc_long(return_value, "count", (zend_long) vec->count);
	if (ZEND_VEC_IS_HYBRID(vec)) {
		add_assoc_string(return_value, "kind", "hybrid");
		add_assoc_long(return_value, "base",
			(zend_long) ZEND_VEC_COUNT(ZEND_VEC_HYBRID_BASE_VEC(vec)));
		add_assoc_long(return_value, "tail",
			(zend_long) ZEND_VEC_COUNT(Z_VEC_P(ZEND_VEC_HYBRID_TAIL(vec))));
	} else {
		add_assoc_string(return_value, "kind", "flat");
		add_assoc_long(return_value, "capacity", (zend_long) (vec->capacity & ZEND_VEC_CAP_MASK));
		add_assoc_long(return_value, "tail", 0);
	}
}

/* Structural-key hooks. These exercise the PROBE side only: a key computed by
 * walking a raw compiler-produced zend_type tree. No interning exists yet. */
static bool test_collection_first_param_type(zend_string *fname, zend_type *out)
{
	zend_string *lc = zend_string_tolower(fname);
	zend_function *fn = zend_hash_find_ptr(EG(function_table), lc);
	zend_string_release(lc);

	if (!fn || fn->common.num_args < 1 || !fn->common.arg_info) {
		return false;
	}
	*out = fn->common.arg_info[0].type;
	return true;
}

static ZEND_FUNCTION(zend_test_collection_key)
{
	zend_string *fname;
	zend_type type;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(fname)
	ZEND_PARSE_PARAMETERS_END();

	if (!test_collection_first_param_type(fname, &type)) {
		zend_argument_value_error(1, "must name a function with at least one parameter");
		RETURN_THROWS();
	}
	if (!zend_collection_key_is_supported(type)) {
		zend_argument_value_error(1, "must name a function whose first parameter is a supported collection type");
		RETURN_THROWS();
	}
	RETURN_LONG((zend_long) zend_collection_key_hash_type(type));
}

static ZEND_FUNCTION(zend_test_collection_key_supported)
{
	zend_string *fname;
	zend_type type;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(fname)
	ZEND_PARSE_PARAMETERS_END();

	if (!test_collection_first_param_type(fname, &type)) {
		zend_argument_value_error(1, "must name a function with at least one parameter");
		RETURN_THROWS();
	}
	RETURN_BOOL(zend_collection_key_is_supported(type));
}

static ZEND_FUNCTION(zend_test_collection_key_positional)
{
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(2)];
	} first, second;
	zend_type ta = ZEND_TYPE_INIT_NONE(0);
	zend_type tb = ZEND_TYPE_INIT_NONE(0);
	zend_type m_int = ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
	zend_type m_str = ZEND_TYPE_INIT_MASK(MAY_BE_STRING);

	ZEND_PARSE_PARAMETERS_NONE();

	/* Stack-built, so this stays allocation-free. Member order is the only
	 * difference between the two descriptors. */
	first.desc.kind = 0;
	first.desc.num_types = 2;
	first.desc.types[0] = m_int;
	first.desc.types[1] = m_str;

	second.desc.kind = 0;
	second.desc.num_types = 2;
	second.desc.types[0] = m_str;
	second.desc.types[1] = m_int;

	ZEND_TYPE_SET_COLLECTION(ta, &first.desc);
	ZEND_TYPE_SET_COLLECTION(tb, &second.desc);

	array_init(return_value);
	add_next_index_long(return_value, (zend_long) zend_collection_key_hash_type(ta));
	add_next_index_long(return_value, (zend_long) zend_collection_key_hash_type(tb));
}

/* Provenance bits must not reach the key. Both hashes below are taken over the
 * *same* descriptor, differing only in _ZEND_TYPE_ARENA_BIT, so an equal pair
 * proves allocation provenance is excluded. */
static ZEND_FUNCTION(zend_test_collection_key_provenance)
{
	zend_string *fname;
	zend_type type;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(fname)
	ZEND_PARSE_PARAMETERS_END();

	if (!test_collection_first_param_type(fname, &type)
	 || !zend_collection_key_is_supported(type)) {
		zend_argument_value_error(1, "must name a function with a supported collection parameter");
		RETURN_THROWS();
	}

	zend_type as_arena = type;
	zend_type as_heap = type;
	ZEND_TYPE_FULL_MASK(as_arena) |= _ZEND_TYPE_ARENA_BIT;
	ZEND_TYPE_FULL_MASK(as_heap) &= ~_ZEND_TYPE_ARENA_BIT;

	array_init(return_value);
	add_next_index_long(return_value, (zend_long) zend_collection_key_hash_type(as_arena));
	add_next_index_long(return_value, (zend_long) zend_collection_key_hash_type(as_heap));
}

/* Forms outside the supported input boundary. The compiler rejects a union
 * inside a collection parameter today, so the only way to present one to the
 * key logic is to build it directly. */
/* Canonicalization probes. Node addresses are exposed only as opaque identity
 * tokens, so tests can assert "same node" / "different node" without any
 * assumption about the value. */
static ZEND_FUNCTION(zend_test_collection_intern)
{
	zend_string *fname;
	zend_type type;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(fname)
	ZEND_PARSE_PARAMETERS_END();

	if (!test_collection_first_param_type(fname, &type)) {
		zend_argument_value_error(1, "must name a function with at least one parameter");
		RETURN_THROWS();
	}

	const zend_collection_info *info = zend_collection_info_intern(type);
	if (!info) {
		RETURN_NULL();
	}

	array_init(return_value);
	add_assoc_long(return_value, "id", (zend_long) (uintptr_t) info);
	add_assoc_str(return_value, "name", zend_collection_info_to_string(info));

	/* A nested member must be a *canonical child node*, not a copy of the
	 * compiler's inner descriptor. */
	if (info->num_types == 1 && ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(info->types[0])) {
		add_assoc_long(return_value, "child_id",
			(zend_long) (uintptr_t) ZEND_TYPE_COLLECTION(info->types[0]));
	} else {
		add_assoc_null(return_value, "child_id");
	}

	/* Arena escape checks: the node must not be the compiler descriptor, and no
	 * member may carry provenance bits. */
	add_assoc_bool(return_value, "aliases_descriptor",
		(const void *) info == (const void *) ZEND_TYPE_COLLECTION(type));

	bool arena_free = true;
	for (uint32_t i = 0; i < info->num_types; i++) {
		if ((ZEND_TYPE_FULL_MASK(info->types[i]) & _ZEND_TYPE_ARENA_BIT) != 0) {
			arena_free = false;
		}
	}
	add_assoc_bool(return_value, "members_arena_free", arena_free);
	add_assoc_bool(return_value, "declared_type_uses_arena", ZEND_TYPE_USES_ARENA(type));
}

static ZEND_FUNCTION(zend_test_vec_type_id)
{
	zval *v;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(v)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DEREF(v);
	if (Z_TYPE_P(v) != IS_COLLECTION) {
		zend_argument_type_error(1, "must be a collection");
		RETURN_THROWS();
	}
	RETURN_LONG((zend_long) (uintptr_t) Z_VEC_P(v)->type);
}

/* Cached classification of a canonical node. Every field here is written once,
 * during promotion, and read-only afterwards. */
/* Cache-behaviour counters. NULL in release builds, where they do not exist. */
static ZEND_FUNCTION(zend_test_collection_stats)
{
	ZEND_PARSE_PARAMETERS_NONE();
#if ZEND_DEBUG
	array_init(return_value);
	add_assoc_long(return_value, "descents",
		(zend_long) zend_collection_info_descent_count());
	add_assoc_long(return_value, "promotions",
		(zend_long) zend_collection_info_promotion_count());
	add_assoc_long(return_value, "nodes",
		(zend_long) zend_collection_info_node_count());
#else
	RETURN_NULL();
#endif
}

static ZEND_FUNCTION(zend_test_collection_classify)
{
	zend_string *fname;
	zend_type type;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(fname)
	ZEND_PARSE_PARAMETERS_END();

	if (!test_collection_first_param_type(fname, &type)) {
		zend_argument_value_error(1, "must name a function with at least one parameter");
		RETURN_THROWS();
	}

	const zend_collection_info *info = zend_collection_info_intern(type);
	if (!info) {
		RETURN_NULL();
	}

	array_init(return_value);
	add_assoc_long(return_value, "num_types", (zend_long) info->num_types);
	add_assoc_long(return_value, "fast_mask", (zend_long) info->fast_mask);
	add_assoc_bool(return_value, "all_mask_members",
		ZEND_COLLECTION_INFO_HAS_FLAG(info, ZEND_COLLECTION_INFO_ALL_MASK_MEMBERS));
	add_assoc_bool(return_value, "value_constructible",
		ZEND_COLLECTION_INFO_IS_VALUE_CONSTRUCTIBLE(info));
}

/* Number of member comparisons that had to descend into a nested node. NULL in
 * release builds, where the counter does not exist. */
static ZEND_FUNCTION(zend_test_collection_descents)
{
	ZEND_PARSE_PARAMETERS_NONE();
#if ZEND_DEBUG
	RETURN_LONG((zend_long) zend_collection_info_descent_count());
#else
	RETURN_NULL();
#endif
}

static ZEND_FUNCTION(zend_test_collection_collision_selftest)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_BOOL(zend_collection_info_collision_selftest());
}

static ZEND_FUNCTION(zend_test_collection_key_unsupported)
{
	union {
		zend_collection_type desc;
		char buf[ZEND_TYPE_COLLECTION_SIZE(1)];
	} outer, empty;
	union {
		zend_type_list list;
		char buf[ZEND_TYPE_LIST_SIZE(2)];
	} members;
	zend_type union_member = ZEND_TYPE_INIT_NONE(0);
	zend_type with_union = ZEND_TYPE_INIT_NONE(0);
	zend_type zero_arity = ZEND_TYPE_INIT_NONE(0);
	zend_type plain_int = ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
	zend_type m_long = ZEND_TYPE_INIT_MASK(MAY_BE_LONG);
	zend_type m_string = ZEND_TYPE_INIT_MASK(MAY_BE_STRING);

	ZEND_PARSE_PARAMETERS_NONE();

	members.list.num_types = 2;
	members.list.types[0] = m_long;
	members.list.types[1] = m_string;
	ZEND_TYPE_SET_LIST(union_member, &members.list);
	ZEND_TYPE_FULL_MASK(union_member) |= _ZEND_TYPE_UNION_BIT;

	outer.desc.kind = 0;
	outer.desc.num_types = 1;
	outer.desc.types[0] = union_member;
	ZEND_TYPE_SET_COLLECTION(with_union, &outer.desc);

	empty.desc.kind = 0;
	empty.desc.num_types = 0;
	ZEND_TYPE_SET_COLLECTION(zero_arity, &empty.desc);

	array_init(return_value);
	add_assoc_bool(return_value, "union_member", zend_collection_key_is_supported(with_union));
	add_assoc_bool(return_value, "zero_arity", zend_collection_key_is_supported(zero_arity));
	add_assoc_bool(return_value, "non_collection_root", zend_collection_key_is_supported(plain_int));
}

/* Regression guard for runtime type tags that sit above _ZEND_TYPE_MAY_BE_MASK.
 * IS_COLLECTION is 21 and _ZEND_TYPE_ITERABLE_BIT is 1u << 21, so an unmasked
 * "does this type contain that code" test reads the iterable flag instead, and an
 * iterable declaration silently accepts a collection without verifying it. */
static ZEND_FUNCTION(zend_test_type_code_alias_selftest)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* The shape the compiler produces for `iterable`: array plus the iterable
	 * flag. It must not report that it contains IS_COLLECTION, while still
	 * reporting the array member it really does contain. */
	{
		zend_type t = (zend_type) ZEND_TYPE_INIT_MASK(
			MAY_BE_ARRAY | _ZEND_TYPE_ITERABLE_BIT);
		add_assoc_bool(return_value, "iterable_excludes_collection",
			!ZEND_TYPE_CONTAINS_CODE(t, IS_COLLECTION)
			&& ZEND_TYPE_CONTAINS_CODE(t, IS_ARRAY));
	}

	/* Every other structural flag is equally out of reach of a code test. */
	{
		zend_type t = (zend_type) ZEND_TYPE_INIT_MASK(
			MAY_BE_LONG | _ZEND_TYPE_UNION_BIT | _ZEND_TYPE_ARENA_BIT);
		add_assoc_bool(return_value, "flags_excluded",
			ZEND_TYPE_CONTAINS_CODE(t, IS_LONG)
			&& !ZEND_TYPE_CONTAINS_CODE(t, IS_COLLECTION));
	}

	/* Ordinary builtin codes keep answering exactly as before. */
	{
		zend_type mixed = (zend_type) ZEND_TYPE_INIT_CODE(IS_MIXED, 0, 0);
		zend_type lng = (zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0);
		add_assoc_bool(return_value, "builtins_unchanged",
			ZEND_TYPE_CONTAINS_CODE(mixed, IS_LONG)
			&& ZEND_TYPE_CONTAINS_CODE(mixed, IS_OBJECT)
			&& ZEND_TYPE_CONTAINS_CODE(lng, IS_LONG)
			&& !ZEND_TYPE_CONTAINS_CODE(lng, IS_STRING));
	}
}

/* Build vec[vec[<innermost>]] on the heap: two nested collection descriptors,
 * the inner one held as the sole parameter of the outer. Proves the descriptor
 * layout represents arbitrary nesting with no arity-one assumption in the
 * generic path (only the vec kind fixes num_types == 1 per level). */
static zend_type zend_test_make_nested_vec(zend_type innermost)
{
	zend_type inner = zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC, innermost);
	return zend_test_make_collection(ZEND_COLLECTION_TYPE_VEC, inner);
}

/* Test-only reference copier that builds an independent copy of a nested
 * collection-descriptor tree.
 *
 * What this is, and what it is NOT:
 *   - It is NOT an alternative or public implementation of any engine routine.
 *     Nothing outside this test may use it, and it must never be promoted to the
 *     engine.
 *   - It exists ONLY because the engine's deep-copy routine, the static
 *     zend_type_copy_ctor() in Zend/zend_inheritance.c, is intentionally kept
 *     private: we do not widen the engine's public surface merely to test it. A
 *     test extension therefore cannot call it, so this helper constructs a
 *     structurally identical, independently-owned tree instead.
 *   - Its SOLE purpose is to produce that independent tree so the *production*
 *     recursive infrastructure can be exercised on it (recursive release,
 *     class-name ownership, recursive stringification; arena handling is covered
 *     separately in scenario 5). The helper's own output is never the assertion
 *     target -- see the note on scenario 4 below.
 *
 * It is composed purely from public primitives (zend_type_collection_alloc,
 * ZEND_TYPE_SET_COLLECTION, zend_string_addref and the ZEND_TYPE_* discriminators);
 * it does not touch any engine internal. The only thing it "mirrors" is the
 * irreducible control flow of a deep copy -- allocate a fresh descriptor per
 * level, recurse into every parameter, take exactly one reference on each leaf
 * class name -- which is why it cannot be shrunk further without either baking in
 * a vec-only arity-one assumption or losing the arity-agnostic recursion.
 *
 * It is expected to remain structurally equivalent to the heap branch
 * (use_arena == false, persistent == false) of zend_type_copy_ctor(), and MUST be
 * reviewed whenever that routine's ownership or descriptor-duplication contract
 * changes. Divergence is low-risk: scenarios 1-3 and 5 exercise the production
 * recursive lifecycle WITHOUT this helper, so a bug here cannot masquerade as
 * production coverage -- at worst it fails its own scenario (4). */
static zend_type zend_test_deep_copy_type(zend_type t)
{
	if (ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(t)) {
		zend_collection_type *src = ZEND_TYPE_COLLECTION(t);
		zend_collection_type *dst =
			zend_type_collection_alloc(src->kind, src->num_types, /* persistent */ false);
		for (uint32_t i = 0; i < src->num_types; i++) {
			dst->types[i] = zend_test_deep_copy_type(src->types[i]);   /* recurse */
		}
		zend_type out = ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_COLLECTION(out, dst);
		return out;
	}
	if (ZEND_TYPE_HAS_NAME(t)) {
		zend_string_addref(ZEND_TYPE_NAME(t));   /* the copy owns its own name reference */
		return t;
	}
	return t;   /* builtin mask: plain value copy */
}

/* Regression self-test for recursively nested collection descriptors, using the
 * internal constructors only (no parser syntax). Exercises the reachable generic
 * lifecycle two descriptor levels deep: construction, recursive stringification,
 * recursive release + ownership, recursive deep copy (via the test-local mirror
 * zend_test_deep_copy_type) and recursive arena-backed release. Persistence
 * (zend_persist_type) stays uncovered here: it is static and needs an
 * accelerator/SHM context. */
static ZEND_FUNCTION(zend_test_nested_collection_type_selftest)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* 1. vec[vec[int]] construction: two descriptor levels, inner is itself a
	 *    collection descriptor (not a name/builtin), innermost is the int mask. */
	{
		zend_type t = zend_test_make_nested_vec(
			(zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0));
		zend_collection_type *outer = ZEND_TYPE_COLLECTION(t);
		bool ok = ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(t)
			&& outer->kind == ZEND_COLLECTION_TYPE_VEC
			&& outer->num_types == 1
			&& ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(outer->types[0]);   /* nested! */
		if (ok) {
			zend_collection_type *inner = ZEND_TYPE_COLLECTION(outer->types[0]);
			ok = inner->kind == ZEND_COLLECTION_TYPE_VEC
				&& inner->num_types == 1
				&& !ZEND_TYPE_HAS_COLLECTION_DESCRIPTOR(inner->types[0])
				&& (ZEND_TYPE_FULL_MASK(inner->types[0]) & _ZEND_TYPE_MAY_BE_MASK)
					== (1u << IS_LONG);
		}
		add_assoc_bool(return_value, "nested_construction", ok);
		zend_type_release(t, /* persistent */ false);   /* recursive release */
	}

	/* 2. Recursive stringification: exactly "vec[vec[int]]". */
	{
		zend_type t = zend_test_make_nested_vec(
			(zend_type) ZEND_TYPE_INIT_CODE(IS_LONG, 0, 0));
		zend_string *s = zend_type_to_string(t);
		add_assoc_bool(return_value, "nested_stringify",
			zend_string_equals_literal(s, "vec[vec[int]]"));
		zend_string_release(s);
		zend_type_release(t, /* persistent */ false);
	}

	/* 3. Recursive release + ownership through two descriptor levels: the single
	 *    innermost class-name reference is released by recursing outer -> inner. */
	{
		zend_string *foo = zend_string_init("Foo", sizeof("Foo") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		zend_string_addref(foo);                         /* the innermost descriptor's ref */
		zend_type t = zend_test_make_nested_vec(
			(zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0));

		bool ok = GC_REFCOUNT(foo) == rc_caller + 1;
		zend_type_release(t, /* persistent */ false);    /* recurses two levels down */
		ok = ok && GC_REFCOUNT(foo) == rc_caller;

		zend_string_release(foo);
		add_assoc_bool(return_value, "nested_release_ownership", ok);
	}

	/* 4. Recursive deep copy. The independent tree is built by the test-local
	 *    reference copier (zend_test_deep_copy_type, above); everything that is
	 *    actually ASSERTED then runs through PRODUCTION code:
	 *      - zend_type_to_string() -> zend_type_to_string_resolved(): recursive
	 *        stringification of both descriptor levels ("vec[vec[Foo]]");
	 *      - zend_type_release() x2: recursive release of both descriptor levels
	 *        plus the leaf class-name reference -- the GC_REFCOUNT() checks
	 *        (caller+2 -> caller+1 -> caller) validate its recursive ownership
	 *        accounting and prove the two trees destroy independently.
	 *    The pointer-inequality checks additionally require the copy to own a
	 *    distinct descriptor at BOTH levels (deep, not shallow). */
	{
		zend_string *foo = zend_string_init("Foo", sizeof("Foo") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		zend_string_addref(foo);
		zend_type orig = zend_test_make_nested_vec(
			(zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0));

		zend_type copy = zend_test_deep_copy_type(orig);

		zend_collection_type *o_out = ZEND_TYPE_COLLECTION(orig);
		zend_collection_type *c_out = ZEND_TYPE_COLLECTION(copy);
		zend_collection_type *o_in = ZEND_TYPE_COLLECTION(o_out->types[0]);
		zend_collection_type *c_in = ZEND_TYPE_COLLECTION(c_out->types[0]);
		bool ok = c_out != o_out          /* outer descriptor duplicated */
			&& c_in != o_in               /* inner descriptor duplicated (deep) */
			&& GC_REFCOUNT(foo) == rc_caller + 2;   /* copy owns its own name ref */

		zend_string *s = zend_type_to_string(copy);
		ok = ok && zend_string_equals_literal(s, "vec[vec[Foo]]");
		zend_string_release(s);

		zend_type_release(copy, /* persistent */ false);
		ok = ok && GC_REFCOUNT(foo) == rc_caller + 1;   /* orig untouched */
		zend_type_release(orig, /* persistent */ false);
		ok = ok && GC_REFCOUNT(foo) == rc_caller;

		zend_string_release(foo);
		add_assoc_bool(return_value, "nested_deep_copy", ok);
	}

	/* 5. Recursive arena-backed release: both descriptor levels live in the arena.
	 *    Release must recurse and drop the class-name reference but free no arena
	 *    storage (ASAN in a debug build proves there is no invalid free). */
	{
		zend_string *foo = zend_string_init("Baz", sizeof("Baz") - 1, 0);
		uint32_t rc_caller = GC_REFCOUNT(foo);
		size_t size = ZEND_TYPE_COLLECTION_SIZE(1);

		zend_collection_type *inner = zend_arena_alloc(&CG(arena), size);
		inner->kind = ZEND_COLLECTION_TYPE_VEC;
		inner->num_types = 1;
		zend_string_addref(foo);
		inner->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(foo, 0, 0);
		zend_type inner_t = ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_COLLECTION(inner_t, inner);
		ZEND_TYPE_FULL_MASK(inner_t) |= _ZEND_TYPE_ARENA_BIT;

		zend_collection_type *outer = zend_arena_alloc(&CG(arena), size);
		outer->kind = ZEND_COLLECTION_TYPE_VEC;
		outer->num_types = 1;
		outer->types[0] = inner_t;              /* carries the arena bit */
		zend_type outer_t = ZEND_TYPE_INIT_NONE(0);
		ZEND_TYPE_SET_COLLECTION(outer_t, outer);
		ZEND_TYPE_FULL_MASK(outer_t) |= _ZEND_TYPE_ARENA_BIT;

		zend_type_release(outer_t, /* persistent */ false);
		bool ok = GC_REFCOUNT(foo) == rc_caller;   /* name released, arena kept */

		zend_string_release(foo);
		add_assoc_bool(return_value, "nested_arena_release", ok);
	}
}

static ZEND_FUNCTION(zend_get_unit_enum)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_OBJ_COPY(zend_enum_get_case_by_id(zend_test_unit_enum, ZEND_ENUM_ZendTestUnitEnum_Foo));
}

static ZEND_FUNCTION(zend_test_zend_ini_parse_quantity)
{
	zend_string *str;
	zend_string *errstr;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	RETVAL_LONG(zend_ini_parse_quantity(str, &errstr));

	if (errstr) {
		zend_error(E_WARNING, "%s", ZSTR_VAL(errstr));
		zend_string_release(errstr);
	}
}

static ZEND_FUNCTION(zend_test_zend_ini_parse_uquantity)
{
	zend_string *str;
	zend_string *errstr;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	RETVAL_LONG((zend_long)zend_ini_parse_uquantity(str, &errstr));

	if (errstr) {
		zend_error(E_WARNING, "%s", ZSTR_VAL(errstr));
		zend_string_release(errstr);
	}
}

static ZEND_FUNCTION(zend_test_zend_ini_str)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_STR(ZT_G(str_test));
}

static ZEND_FUNCTION(zend_test_zstr_init_literal)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_STR(ZSTR_INIT_LITERAL("foo\0bar", false));
}

static ZEND_FUNCTION(zend_test_is_string_marked_as_valid_utf8)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_BOOL(ZSTR_IS_VALID_UTF8(str));
}

static ZEND_FUNCTION(ZendTestNS2_namespaced_func)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_TRUE;
}

static ZEND_FUNCTION(ZendTestNS2_namespaced_deprecated_func)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_FUNCTION(ZendTestNS2_ZendSubNS_namespaced_func)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_TRUE;
}

static ZEND_FUNCTION(ZendTestNS2_ZendSubNS_namespaced_deprecated_func)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_FUNCTION(zend_test_parameter_with_attribute)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(1);
}

static ZEND_FUNCTION(zend_test_attribute_with_named_argument)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

#ifdef ZEND_CHECK_STACK_LIMIT
static ZEND_FUNCTION(zend_test_zend_call_stack_get)
{
	zend_call_stack stack;

	ZEND_PARSE_PARAMETERS_NONE();

	if (zend_call_stack_get(&stack)) {
		zend_string *str;

		array_init(return_value);

		str = strpprintf(0, "%p", stack.base);
		add_assoc_str(return_value, "base", str);

		str = strpprintf(0, "0x%zx", stack.max_size);
		add_assoc_str(return_value, "max_size", str);

		str = strpprintf(0, "%p", zend_call_stack_position());
		add_assoc_str(return_value, "position", str);

		str = strpprintf(0, "%p", EG(stack_limit));
		add_assoc_str(return_value, "EG(stack_limit)", str);

		return;
	}

	RETURN_NULL();
}

zend_long (*volatile zend_call_stack_use_all_fun)(void *limit);

static zend_long zend_call_stack_use_all(void *limit)
{
	if (zend_call_stack_overflowed(limit)) {
		return 1;
	}

	return 1 + zend_call_stack_use_all_fun(limit);
}

static ZEND_FUNCTION(zend_test_zend_call_stack_use_all)
{
	zend_call_stack stack;

	ZEND_PARSE_PARAMETERS_NONE();

	if (!zend_call_stack_get(&stack)) {
		return;
	}

	zend_call_stack_use_all_fun = zend_call_stack_use_all;

	void *limit = zend_call_stack_limit(stack.base, stack.max_size, 4096);

	RETURN_LONG(zend_call_stack_use_all(limit));
}
#endif /* ZEND_CHECK_STACK_LIMIT */

static ZEND_FUNCTION(zend_get_map_ptr_last)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_LONG(CG(map_ptr_last));
}

static ZEND_FUNCTION(zend_test_crash)
{
	zend_string *message = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STR_OR_NULL(message)
	ZEND_PARSE_PARAMETERS_END();

	if (message) {
		php_printf("%s", ZSTR_VAL(message));
	}

	char *invalid = (char *) 1;
	php_printf("%s", invalid);
}

static ZEND_FUNCTION(zend_test_uri_parser)
{
	zend_string *uri_string;
	zend_string *parser_name;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(uri_string)
		Z_PARAM_STR(parser_name)
	ZEND_PARSE_PARAMETERS_END();

	const php_uri_parser *parser = php_uri_get_parser(parser_name);
	if (parser == NULL) {
		zend_argument_value_error(1, "Unknown parser");
		RETURN_THROWS();
	}

	php_uri_internal *uri = php_uri_parse(parser, ZSTR_VAL(uri_string), ZSTR_LEN(uri_string), false);
	if (uri == NULL) {
		RETURN_THROWS();
	}

	php_uri *uri_struct = php_uri_parse_to_struct(parser, ZSTR_VAL(uri_string), ZSTR_LEN(uri_string), PHP_URI_COMPONENT_READ_MODE_RAW, false);
	if (uri_struct == NULL) {
		RETURN_THROWS();
	}

	zval value;

	array_init(return_value);
	zval normalized;
	array_init(&normalized);
	php_uri_get_scheme(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_SCHEME), &value);
	php_uri_get_username(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_USERNAME), &value);
	php_uri_get_password(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_PASSWORD), &value);
	php_uri_get_host(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_HOST), &value);
	php_uri_get_port(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_PORT), &value);
	php_uri_get_path(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_PATH), &value);
	php_uri_get_query(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_QUERY), &value);
	php_uri_get_fragment(uri, PHP_URI_COMPONENT_READ_MODE_NORMALIZED_ASCII, &value);
	zend_hash_add(Z_ARR(normalized), ZSTR_KNOWN(ZEND_STR_FRAGMENT), &value);
	zend_hash_str_add(Z_ARR_P(return_value), "normalized", strlen("normalized"), &normalized);
	zval raw;
	array_init(&raw);
	php_uri_get_scheme(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_SCHEME), &value);
	php_uri_get_username(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_USERNAME), &value);
	php_uri_get_password(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_PASSWORD), &value);
	php_uri_get_host(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_HOST), &value);
	php_uri_get_port(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_PORT), &value);
	php_uri_get_path(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_PATH), &value);
	php_uri_get_query(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_QUERY), &value);
	php_uri_get_fragment(uri, PHP_URI_COMPONENT_READ_MODE_RAW, &value);
	zend_hash_add(Z_ARR(raw), ZSTR_KNOWN(ZEND_STR_FRAGMENT), &value);
	zend_hash_str_add(Z_ARR_P(return_value), "raw", strlen("raw"), &raw);
	zval from_struct;
	zval dummy;
	array_init(&from_struct);
	if (uri_struct->scheme) {
		ZVAL_STR_COPY(&dummy, uri_struct->scheme);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_SCHEME), &dummy);
	if (uri_struct->user) {
		ZVAL_STR_COPY(&dummy, uri_struct->user);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_USERNAME), &dummy);
	if (uri_struct->password) {
		ZVAL_STR_COPY(&dummy, uri_struct->password);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_PASSWORD), &dummy);
	if (uri_struct->host) {
		ZVAL_STR_COPY(&dummy, uri_struct->host);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_HOST), &dummy);
	ZVAL_LONG(&dummy, uri_struct->port);
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_PORT), &dummy);
	if (uri_struct->path) {
		ZVAL_STR_COPY(&dummy, uri_struct->path);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_PATH), &dummy);
	if (uri_struct->query) {
		ZVAL_STR_COPY(&dummy, uri_struct->query);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_QUERY), &dummy);
	if (uri_struct->fragment) {
		ZVAL_STR_COPY(&dummy, uri_struct->fragment);
	} else {
		ZVAL_NULL(&dummy);
	}
	zend_hash_add(Z_ARR(from_struct), ZSTR_KNOWN(ZEND_STR_FRAGMENT), &dummy);
	zend_hash_str_add(Z_ARR_P(return_value), "struct", strlen("struct"), &from_struct);

	php_uri_struct_free(uri_struct);
	php_uri_free(uri);
}

static bool has_opline(zend_execute_data *execute_data)
{
	return execute_data
		&& execute_data->func
		&& ZEND_USER_CODE(execute_data->func->type)
		&& execute_data->opline
	;
}

void * zend_test_custom_malloc(size_t len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	if (has_opline(EG(current_execute_data))) {
		assert(EG(current_execute_data)->opline->lineno != (uint32_t)-1);
	}
	return _zend_mm_alloc(ZT_G(zend_orig_heap), len ZEND_FILE_LINE_EMPTY_CC ZEND_FILE_LINE_EMPTY_CC);
}

void zend_test_custom_free(void *ptr ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	if (has_opline(EG(current_execute_data))) {
		assert(EG(current_execute_data)->opline->lineno != (uint32_t)-1);
	}
	_zend_mm_free(ZT_G(zend_orig_heap), ptr ZEND_FILE_LINE_EMPTY_CC ZEND_FILE_LINE_EMPTY_CC);
}

void * zend_test_custom_realloc(void * ptr, size_t len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	if (has_opline(EG(current_execute_data))) {
		assert(EG(current_execute_data)->opline->lineno != (uint32_t)-1);
	}
	return _zend_mm_realloc(ZT_G(zend_orig_heap), ptr, len ZEND_FILE_LINE_EMPTY_CC ZEND_FILE_LINE_EMPTY_CC);
}

static void zend_test_reset_heap(zend_zend_test_globals *zend_test_globals)
{
	if (zend_test_globals->zend_test_heap) {
		free(zend_test_globals->zend_test_heap);
		zend_test_globals->zend_test_heap = NULL;
		zend_mm_set_heap(zend_test_globals->zend_orig_heap);
	}
}

static PHP_INI_MH(OnUpdateZendTestObserveOplineInZendMM)
{
	if (new_value == NULL) {
		return FAILURE;
	}

	int int_value = zend_ini_parse_bool(new_value);

	if (int_value == 1) {
		// `zend_mm_heap` is a private struct, so we have not way to find the
		// actual size, but 4096 bytes should be enough
		ZT_G(zend_test_heap) = malloc(4096);
		memset(ZT_G(zend_test_heap), 0, 4096);
		zend_mm_set_custom_handlers(
			ZT_G(zend_test_heap),
			zend_test_custom_malloc,
			zend_test_custom_free,
			zend_test_custom_realloc
		);
		ZT_G(zend_orig_heap) = zend_mm_get_heap();
		zend_mm_set_heap(ZT_G(zend_test_heap));
	} else {
		zend_test_reset_heap(ZEND_MODULE_GLOBALS_BULK(zend_test));
	}
	return OnUpdateBool(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}

static ZEND_FUNCTION(zend_test_fill_packed_array)
{
	HashTable *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT_EX(parameter, 0, 1)
	ZEND_PARSE_PARAMETERS_END();

	if (!HT_IS_PACKED(parameter)) {
		zend_argument_value_error(1, "must be a packed array");
		RETURN_THROWS();
	}

	zend_hash_extend(parameter, parameter->nNumUsed + 10, true);
	ZEND_HASH_FILL_PACKED(parameter) {
		for (int i = 0; i < 10; i++) {
			zval value;
			ZVAL_LONG(&value, i);
			ZEND_HASH_FILL_ADD(&value);
		}
	} ZEND_HASH_FILL_END();
}

static ZEND_FUNCTION(get_open_basedir)
{
	ZEND_PARSE_PARAMETERS_NONE();
	if (PG(open_basedir)) {
		RETURN_STRING(PG(open_basedir));
	} else {
		RETURN_NULL();
	}
}

static ZEND_FUNCTION(zend_test_is_pcre_bundled)
{
	ZEND_PARSE_PARAMETERS_NONE();
#ifdef HAVE_BUNDLED_PCRE
	RETURN_TRUE;
#else
	RETURN_FALSE;
#endif
}

#ifdef PHP_WIN32
static ZEND_FUNCTION(zend_test_set_fmode)
{
	bool binary;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_BOOL(binary)
	ZEND_PARSE_PARAMETERS_END();

	_fmode = binary ? _O_BINARY : _O_TEXT;
}
#endif

static ZEND_FUNCTION(zend_test_cast_fread)
{
	zval *stream_zv;
	php_stream *stream;
	FILE *fp;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(stream_zv);
	ZEND_PARSE_PARAMETERS_END();

	php_stream_from_zval(stream, stream_zv);

	if (php_stream_cast(stream, PHP_STREAM_AS_STDIO, (void *) &fp, REPORT_ERRORS) == FAILURE) {
		return;
	}

	size_t size = 10240; /* Must be large enough to trigger the issue */
	char *buf = malloc(size);
	bool bail = false;
	zend_try {
		(void) !fread(buf, 1, size, fp);
	} zend_catch {
		bail = true;
	} zend_end_try();

	free(buf);

	if (bail) {
		zend_bailout();
	}
}

static ZEND_FUNCTION(zend_test_is_zend_ptr)
{
	zend_long addr;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(addr);
	ZEND_PARSE_PARAMETERS_END();

	RETURN_BOOL(is_zend_ptr((void*)addr));
}

static ZEND_FUNCTION(zend_test_log_err_debug)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str);
	ZEND_PARSE_PARAMETERS_END();

	php_log_err_with_severity(ZSTR_VAL(str), LOG_DEBUG);
}

typedef struct _zend_test_object {
	zend_internal_function *tmp_method;
	zend_object std;
} zend_test_object;

static zend_object *zend_test_class_new(zend_class_entry *class_type)
{
	zend_test_object *intern = zend_object_alloc(sizeof(zend_test_object), class_type);
	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);
	return &intern->std;
}

static void zend_test_class_free_obj(zend_object *object)
{
	zend_test_object *intern = ZEND_CONTAINER_OF(object, zend_test_object, std);

	if (intern->tmp_method) {
		zend_internal_function *func = intern->tmp_method;
		intern->tmp_method = NULL;
		zend_string_release_ex(func->function_name, 0);
		zend_free_internal_arg_info(func, false);
		efree(func);
	}

	zend_object_std_dtor(object);
}

static zend_function *zend_test_class_method_get(zend_object **object, zend_string *name, const zval *key)
{
	zend_test_object *intern = ZEND_CONTAINER_OF(*object, zend_test_object, std);

	if (zend_string_equals_literal_ci(name, "test")) {
		zend_internal_function *fptr;

		if (EXPECTED(EG(trampoline).common.function_name == NULL)) {
			fptr = (zend_internal_function *) &EG(trampoline);
		} else {
			fptr = emalloc(sizeof(zend_internal_function));
	    }
		memset(fptr, 0, sizeof(zend_internal_function));
		fptr->type = ZEND_INTERNAL_FUNCTION;
		fptr->num_args = 0;
		fptr->scope = (*object)->ce;
		fptr->fn_flags = ZEND_ACC_CALL_VIA_HANDLER;
		fptr->fn_flags2 = 0;
		fptr->function_name = zend_string_copy(name);
		fptr->handler = ZEND_FN(zend_test_func);
		fptr->doc_comment = NULL;

		return (zend_function*)fptr;
	} else if (zend_string_equals_literal_ci(name, "testTmpMethodWithArgInfo")) {
		if (intern->tmp_method) {
			return (zend_function*)intern->tmp_method;
		}

		const zend_function_entry *entry = &class_ZendTestTmpMethods_methods[0];
		zend_internal_function *fptr = emalloc(sizeof(zend_internal_function));
		memset(fptr, 0, sizeof(zend_internal_function));
		fptr->type = ZEND_INTERNAL_FUNCTION;
		fptr->handler = entry->handler;
		fptr->function_name = zend_string_init(entry->fname, strlen(entry->fname), false);
		fptr->scope = intern->std.ce;
		fptr->prototype = NULL;
		fptr->T = ZEND_OBSERVER_ENABLED;
		fptr->fn_flags = ZEND_ACC_PUBLIC | ZEND_ACC_NEVER_CACHE;

		zend_internal_function_info *info = (zend_internal_function_info*)entry->arg_info;

		uint32_t num_arg_info = 1 + entry->num_args;
		zend_arg_info *arg_info = safe_emalloc(num_arg_info, sizeof(zend_arg_info), 0);
		for (uint32_t i = 0; i < num_arg_info; i++) {
			zend_convert_internal_arg_info(&arg_info[i], &entry->arg_info[i], i == 0, false);
		}

		fptr->arg_info = arg_info + 1;
		fptr->num_args = entry->num_args;
		if (info->required_num_args == (uint32_t)-1) {
			fptr->required_num_args = entry->num_args;
		} else {
			fptr->required_num_args = info->required_num_args;
		}

		intern->tmp_method = fptr;

		return (zend_function*)fptr;
	}
	return zend_std_get_method(object, name, key);
}

static zend_function *zend_test_class_static_method_get(zend_class_entry *ce, zend_string *name)
{
	if (zend_string_equals_literal_ci(name, "test")) {
		zend_internal_function *fptr;

		if (EXPECTED(EG(trampoline).common.function_name == NULL)) {
			fptr = (zend_internal_function *) &EG(trampoline);
		} else {
			fptr = emalloc(sizeof(zend_internal_function));
		}
		memset(fptr, 0, sizeof(zend_internal_function));
		fptr->type = ZEND_INTERNAL_FUNCTION;
		fptr->num_args = 0;
		fptr->scope = ce;
		fptr->fn_flags = ZEND_ACC_CALL_VIA_HANDLER|ZEND_ACC_STATIC;
		fptr->fn_flags2 = 0;
		fptr->function_name = zend_string_copy(name);
		fptr->handler = ZEND_FN(zend_test_func);
		fptr->doc_comment = NULL;

		return (zend_function*)fptr;
	}
	return zend_std_get_static_method(ce, name, NULL);
}

zend_string *zend_attribute_validate_zendtestattribute(zend_attribute *attr, uint32_t target, zend_class_entry *scope)
{
	if (target != ZEND_ATTRIBUTE_TARGET_CLASS) {
		return ZSTR_INIT_LITERAL("Only classes can be marked with #[ZendTestAttribute]", 0);
	}
	return NULL;
}

static ZEND_METHOD(_ZendTestClass, __toString)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_EMPTY_STRING();
}

/* Internal function returns bool, we return int. */
static ZEND_METHOD(_ZendTestClass, is_object)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_LONG(42);
}

static ZEND_METHOD(_ZendTestClass, returnsStatic) {
	ZEND_PARSE_PARAMETERS_NONE();
	object_init_ex(return_value, zend_get_called_scope(execute_data));
}

static ZEND_METHOD(_ZendTestClass, returnsThrowable)
{
	ZEND_PARSE_PARAMETERS_NONE();
	zend_throw_error(NULL, "Dummy");
}

static ZEND_METHOD(_ZendTestClass, variadicTest) {
	int      argc, i;
	zval    *args = NULL;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', args, argc)
	ZEND_PARSE_PARAMETERS_END();

	for (i = 0; i < argc; i++) {
		zval *arg = args + i;

		if (Z_TYPE_P(arg) == IS_STRING) {
			continue;
		}
		if (Z_TYPE_P(arg) == IS_OBJECT && instanceof_function(Z_OBJ_P(arg)->ce, zend_ce_iterator)) {
			continue;
		}

		zend_argument_type_error(i + 1, "must be of class Iterator or a string, %s given", zend_zval_type_name(arg));
		RETURN_THROWS();
	}

	object_init_ex(return_value, zend_get_called_scope(execute_data));
}

ZEND_METHOD(ZendTestTmpMethods, testTmpMethodWithArgInfo)
{
	zend_object *obj;
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(0, 2);
		Z_PARAM_OPTIONAL;
		Z_PARAM_OBJ_OR_NULL(obj);
		Z_PARAM_STR(str);
	ZEND_PARSE_PARAMETERS_END();
}

static ZEND_METHOD(_ZendTestChildClass, returnsThrowable)
{
	ZEND_PARSE_PARAMETERS_NONE();
	zend_throw_error(NULL, "Dummy");
}

static ZEND_METHOD(ZendAttributeTest, testMethod)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_TRUE;
}

static ZEND_METHOD(_ZendTestTrait, testMethod)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_TRUE;
}

static ZEND_METHOD(ZendTestNS_Foo, method)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(0);
}

static ZEND_METHOD(ZendTestNS_UnlikelyCompileError, method)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_NULL();
}

static ZEND_METHOD(ZendTestNS_NotUnlikelyCompileError, method)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_NULL();
}

static ZEND_METHOD(ZendTestNS2_Foo, method)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_METHOD(ZendTestNS2_ZendSubNS_Foo, method)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static ZEND_METHOD(ZendTestParameterAttribute, __construct)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_STR_COPY(OBJ_PROP_NUM(Z_OBJ_P(ZEND_THIS), 0), parameter);
}

static ZEND_METHOD(ZendTestPropertyAttribute, __construct)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_STR_COPY(OBJ_PROP_NUM(Z_OBJ_P(ZEND_THIS), 0), parameter);
}

static ZEND_METHOD(ZendTestAttributeWithArguments, __construct)
{
	zval *arg;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(arg)
	ZEND_PARSE_PARAMETERS_END();

	zend_string *property_name = zend_string_init("arg", strlen("arg"), 0);
	zend_update_property_ex(zend_test_attribute_with_arguments, Z_OBJ_P(ZEND_THIS), property_name, arg);
	zend_string_release(property_name);
}

static ZEND_METHOD(ZendTestClassWithMethodWithParameterAttribute, no_override)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(2);
}

static ZEND_METHOD(ZendTestClassWithMethodWithParameterAttribute, override)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(3);
}

static ZEND_METHOD(ZendTestChildClassWithMethodWithParameterAttribute, override)
{
	zend_string *parameter;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(parameter)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_LONG(4);
}

static ZEND_METHOD(ZendTestForbidDynamicCall, call)
{
	ZEND_PARSE_PARAMETERS_NONE();

	zend_forbid_dynamic_call();
}

static ZEND_METHOD(ZendTestForbidDynamicCall, callStatic)
{
	ZEND_PARSE_PARAMETERS_NONE();

	zend_forbid_dynamic_call();
}

static ZEND_METHOD(_ZendTestMagicCall, __call)
{
	zend_string *name;
	zval *arguments;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(name)
		Z_PARAM_ARRAY(arguments)
	ZEND_PARSE_PARAMETERS_END();

	zval name_zv;
	ZVAL_STR(&name_zv, name);

	zend_string_addref(name);
	Z_TRY_ADDREF_P(arguments);
	RETURN_ARR(zend_new_pair(&name_zv, arguments));
}

static ZEND_METHOD(_ZendTestMagicCallForward, __call)
{
	zend_string *name;
	zval *arguments;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(name)
		Z_PARAM_ARRAY(arguments)
	ZEND_PARSE_PARAMETERS_END();

	ZEND_IGNORE_VALUE(arguments);

	zval func, rv;
	ZVAL_STR(&func, name);
	call_user_function(NULL, NULL, &func, &rv, 0, NULL);

	ZVAL_COPY_DEREF(return_value, &rv);
	zval_ptr_dtor(&rv);
}

PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("zend_test.replace_zend_execute_ex", "0", PHP_INI_SYSTEM, OnUpdateBool, replace_zend_execute_ex, zend_zend_test_globals, zend_test_globals)
	STD_PHP_INI_BOOLEAN("zend_test.register_passes", "0", PHP_INI_SYSTEM, OnUpdateBool, register_passes, zend_zend_test_globals, zend_test_globals)
	STD_PHP_INI_BOOLEAN("zend_test.print_stderr_mshutdown", "0", PHP_INI_SYSTEM, OnUpdateBool, print_stderr_mshutdown, zend_zend_test_globals, zend_test_globals)
#ifdef HAVE_COPY_FILE_RANGE
	STD_PHP_INI_ENTRY("zend_test.limit_copy_file_range", "-1", PHP_INI_ALL, OnUpdateLong, limit_copy_file_range, zend_zend_test_globals, zend_test_globals)
#endif
	STD_PHP_INI_ENTRY("zend_test.quantity_value", "0", PHP_INI_ALL, OnUpdateLong, quantity_value, zend_zend_test_globals, zend_test_globals)
	STD_PHP_INI_ENTRY("zend_test.str_test", "", PHP_INI_ALL, OnUpdateStr, str_test, zend_zend_test_globals, zend_test_globals)
	STD_PHP_INI_ENTRY("zend_test.not_empty_str_test", "val", PHP_INI_ALL, OnUpdateStrNotEmpty, not_empty_str_test, zend_zend_test_globals, zend_test_globals)
	STD_PHP_INI_BOOLEAN("zend_test.observe_opline_in_zendmm", "0", PHP_INI_ALL, OnUpdateZendTestObserveOplineInZendMM, observe_opline_in_zendmm, zend_zend_test_globals, zend_test_globals)
PHP_INI_END()

void (*old_zend_execute_ex)(zend_execute_data *execute_data);
static void custom_zend_execute_ex(zend_execute_data *execute_data)
{
	old_zend_execute_ex(execute_data);
}

static void le_throwing_resource_dtor(zend_resource *rsrc)
{
	zend_throw_exception(NULL, "Throwing resource destructor called", 0);
}

static ZEND_METHOD(_ZendTestClass, takesUnionType)
{
	zend_object *obj;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_OBJ(obj)
	ZEND_PARSE_PARAMETERS_END();
	// we have to perform type-checking to avoid arginfo/zpp mismatch error
	bool type_matches = (
		instanceof_function(obj->ce, zend_standard_class_def)
		||
		instanceof_function(obj->ce, zend_ce_iterator)
	);
	if (!type_matches) {
		zend_string *ty = zend_type_to_string(execute_data->func->internal_function.arg_info->type);
		zend_argument_type_error(1, "must be of type %s, %s given", ty->val, obj->ce->name->val);
		zend_string_release(ty);
		RETURN_THROWS();
	}

	RETURN_NULL();
}

// Returns a newly allocated DNF type `Iterator|(Traversable&Countable)`.
//
// We need to generate it "manually" because gen_stubs.php does not support codegen for DNF types ATM.
static zend_type create_test_dnf_type(void) {
	zend_string *class_Iterator = zend_string_init_interned("Iterator", sizeof("Iterator") - 1, true);
	zend_alloc_ce_cache(class_Iterator);
	zend_string *class_Traversable = ZSTR_KNOWN(ZEND_STR_TRAVERSABLE);
	zend_string *class_Countable = zend_string_init_interned("Countable", sizeof("Countable") - 1, true);
	zend_alloc_ce_cache(class_Countable);
	//
	zend_type_list *intersection_list = malloc(ZEND_TYPE_LIST_SIZE(2));
	intersection_list->num_types = 2;
	intersection_list->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(class_Traversable, 0, 0);
	intersection_list->types[1] = (zend_type) ZEND_TYPE_INIT_CLASS(class_Countable, 0, 0);
	zend_type_list *union_list = malloc(ZEND_TYPE_LIST_SIZE(2));
	union_list->num_types = 2;
	union_list->types[0] = (zend_type) ZEND_TYPE_INIT_CLASS(class_Iterator, 0, 0);
	union_list->types[1] = (zend_type) ZEND_TYPE_INIT_INTERSECTION(intersection_list, 0);
	return (zend_type) ZEND_TYPE_INIT_UNION(union_list, 0);
}

static void register_ZendTestClass_dnf_property(zend_class_entry *ce) {
	zend_string *prop_name = zend_string_init_interned("dnfProperty", sizeof("dnfProperty") - 1, true);
	zval default_value;
	ZVAL_UNDEF(&default_value);
	zend_type type = create_test_dnf_type();
	zend_declare_typed_property(ce, prop_name, &default_value, ZEND_ACC_PUBLIC, NULL, type);
}

// arg_info for `zend_test_internal_dnf_arguments`
// The types are upgraded to DNF types in `register_dynamic_function_entries()`
static zend_internal_arg_info arginfo_zend_test_internal_dnf_arguments[] = {
	// first entry is a zend_internal_function_info (see zend_compile.h): {argument_count, return_type, unused}
	{(const char*)(uintptr_t)(1), {0}, NULL},
	{"arg", {0}, NULL}
};

static ZEND_NAMED_FUNCTION(zend_test_internal_dnf_arguments)
{
	zend_object *obj;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_OBJ(obj)
	ZEND_PARSE_PARAMETERS_END();
	// we have to perform type-checking to avoid arginfo/zpp mismatch error
	bool type_matches = (
		instanceof_function(obj->ce, zend_ce_iterator)
		|| (
			instanceof_function(obj->ce, zend_ce_traversable)
			&& instanceof_function(obj->ce, zend_ce_countable)
		)
	);
	if (!type_matches) {
		zend_string *ty = zend_type_to_string(arginfo_zend_test_internal_dnf_arguments[1].type);
		zend_argument_type_error(1, "must be of type %s, %s given", ty->val, obj->ce->name->val);
		zend_string_release(ty);
		RETURN_THROWS();
	}

	RETURN_OBJ_COPY(obj);
}

static const zend_function_entry dynamic_function_entries[] = {
	{
		.fname = "zend_test_internal_dnf_arguments",
		.handler = zend_test_internal_dnf_arguments,
		.arg_info = arginfo_zend_test_internal_dnf_arguments,
		.num_args = 1,
		.flags = 0,
	},
	ZEND_FE_END,
};

static void register_dynamic_function_entries(int module_type) {
	// return-type is at index 0
	arginfo_zend_test_internal_dnf_arguments[0].type = create_test_dnf_type();
	arginfo_zend_test_internal_dnf_arguments[1].type = create_test_dnf_type();
	//
	zend_register_functions(NULL, dynamic_function_entries, NULL, module_type);
}

PHP_MINIT_FUNCTION(zend_test)
{
	register_dynamic_function_entries(type);

	zend_test_interface = register_class__ZendTestInterface();

	zend_test_class = register_class__ZendTestClass(zend_test_interface);
	register_ZendTestClass_dnf_property(zend_test_class);
	zend_test_class->create_object = zend_test_class_new;
	zend_test_class->get_static_method = zend_test_class_static_method_get;
	zend_test_class->default_object_handlers = &zend_test_class_handlers;

	zend_test_child_class = register_class__ZendTestChildClass(zend_test_class);

	memcpy(&zend_test_class_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	zend_test_class_handlers.get_method = zend_test_class_method_get;
	zend_test_class_handlers.clone_obj = NULL;
	zend_test_class_handlers.free_obj = zend_test_class_free_obj;
	zend_test_class_handlers.offset = offsetof(zend_test_object, std);

	zend_test_gen_stub_flag_compatibility_test = register_class_ZendTestGenStubFlagCompatibilityTest();

	zend_attribute_test_class = register_class_ZendAttributeTest();

	zend_test_trait = register_class__ZendTestTrait();

	register_test_symbols(module_number);

	zend_test_attribute = register_class_ZendTestAttribute();
	{
		zend_internal_attribute *attr = zend_mark_internal_attribute(zend_test_attribute);
		attr->validator = zend_attribute_validate_zendtestattribute;
	}

	zend_test_repeatable_attribute = register_class_ZendTestRepeatableAttribute();
	zend_mark_internal_attribute(zend_test_repeatable_attribute);

	zend_test_parameter_attribute = register_class_ZendTestParameterAttribute();
	zend_mark_internal_attribute(zend_test_parameter_attribute);

	zend_test_property_attribute = register_class_ZendTestPropertyAttribute();
	zend_mark_internal_attribute(zend_test_property_attribute);

	zend_test_attribute_with_arguments = register_class_ZendTestAttributeWithArguments();
	zend_mark_internal_attribute(zend_test_attribute_with_arguments);

	zend_test_class_with_method_with_parameter_attribute = register_class_ZendTestClassWithMethodWithParameterAttribute();
	zend_test_child_class_with_method_with_parameter_attribute = register_class_ZendTestChildClassWithMethodWithParameterAttribute(zend_test_class_with_method_with_parameter_attribute);

	zend_test_class_with_property_attribute = register_class_ZendTestClassWithPropertyAttribute();
	{
		zend_property_info *prop_info = zend_hash_str_find_ptr(&zend_test_class_with_property_attribute->properties_info, "attributed", sizeof("attributed") - 1);
		zend_add_property_attribute(zend_test_class_with_property_attribute, prop_info, zend_test_attribute->name, 0);
	}

	zend_test_forbid_dynamic_call = register_class_ZendTestForbidDynamicCall();

	zend_test_ns_foo_class = register_class_ZendTestNS_Foo();
	zend_test_ns_unlikely_compile_error_class = register_class_ZendTestNS_UnlikelyCompileError();
	zend_test_ns_not_unlikely_compile_error_class = register_class_ZendTestNS_NotUnlikelyCompileError();
	zend_test_ns_bar_class = register_class_ZendTestNS_Bar();
	zend_test_ns2_foo_class = register_class_ZendTestNS2_Foo();
	zend_test_ns2_ns_foo_class = register_class_ZendTestNS2_ZendSubNS_Foo();

	zend_test_unit_enum = register_class_ZendTestUnitEnum();
	zend_test_string_enum = register_class_ZendTestStringEnum();
	zend_test_int_enum = register_class_ZendTestIntEnum();
	zend_test_enum_with_interface = register_class_ZendTestEnumWithInterface(zend_test_interface);

	zend_test_magic_call = register_class__ZendTestMagicCall();

	register_class__ZendTestMagicCallForward();

	zend_register_functions(NULL, ext_function_legacy, NULL, EG(current_module)->type);

	// Loading via dl() not supported with the observer API
	if (type != MODULE_TEMPORARY) {
		REGISTER_INI_ENTRIES();
	} else {
		(void)ini_entries;
	}

	if (ZT_G(replace_zend_execute_ex)) {
		old_zend_execute_ex = zend_execute_ex;
		zend_execute_ex = custom_zend_execute_ex;
	}

	if (ZT_G(register_passes)) {
		zend_optimizer_register_pass(pass1);
		zend_optimizer_register_pass(pass2);
	}

	zend_test_observer_init(INIT_FUNC_ARGS_PASSTHRU);
	zend_test_mm_custom_handlers_minit(INIT_FUNC_ARGS_PASSTHRU);
	zend_test_fiber_init();
	zend_test_iterators_init();
	zend_test_object_handlers_init();

	le_throwing_resource = zend_register_list_destructors_ex(le_throwing_resource_dtor, NULL, "throwing resource", module_number);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(zend_test)
{
	if (type != MODULE_TEMPORARY) {
		UNREGISTER_INI_ENTRIES();
	}

	zend_test_observer_shutdown(SHUTDOWN_FUNC_ARGS_PASSTHRU);

	if (ZT_G(print_stderr_mshutdown)) {
		fprintf(stderr, "[zend_test] MSHUTDOWN\n");
	}

	return SUCCESS;
}

PHP_RINIT_FUNCTION(zend_test)
{
	ALLOC_HASHTABLE(ZT_G(global_weakmap));
	zend_hash_init(ZT_G(global_weakmap), 8, NULL, ZVAL_PTR_DTOR, 0);
	ZT_G(observer_nesting_depth) = 0;
	zend_test_mm_custom_handlers_rinit();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(zend_test)
{
	zend_weakrefs_hash_destroy(ZT_G(global_weakmap));
	FREE_HASHTABLE(ZT_G(global_weakmap));

	if (ZT_G(zend_test_heap))  {
		free(ZT_G(zend_test_heap));
		ZT_G(zend_test_heap) = NULL;
		zend_mm_set_heap(ZT_G(zend_orig_heap));
	}

	zend_test_mm_custom_handlers_rshutdown();
	return SUCCESS;
}

static PHP_GINIT_FUNCTION(zend_test)
{
#if defined(COMPILE_DL_ZEND_TEST) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	memset(zend_test_globals, 0, sizeof(*zend_test_globals));

	zend_test_observer_ginit(zend_test_globals);
}

static PHP_GSHUTDOWN_FUNCTION(zend_test)
{
	zend_test_observer_gshutdown(zend_test_globals);
	zend_test_reset_heap(zend_test_globals);
}

PHP_MINFO_FUNCTION(zend_test)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "zend_test extension", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

zend_module_entry zend_test_module_entry = {
	STANDARD_MODULE_HEADER,
	"zend_test",
	ext_functions,
	PHP_MINIT(zend_test),
	PHP_MSHUTDOWN(zend_test),
	PHP_RINIT(zend_test),
	PHP_RSHUTDOWN(zend_test),
	PHP_MINFO(zend_test),
	PHP_ZEND_TEST_VERSION,
	PHP_MODULE_GLOBALS(zend_test),
	PHP_GINIT(zend_test),
	PHP_GSHUTDOWN(zend_test),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_ZEND_TEST
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(zend_test)
#endif

/* The important part here is the ZEND_FASTCALL. */
PHP_ZEND_TEST_API int ZEND_FASTCALL bug78270(const char *str, size_t str_len)
{
	char * copy = zend_strndup(str, str_len);
	int r = (int) ZEND_ATOL(copy);
	free(copy);
	return r;
}

PHP_ZEND_TEST_API struct bug79096 bug79096(void)
{
	struct bug79096 b;

	b.a = 1;
	b.b = 1;
	return b;
}

PHP_ZEND_TEST_API void bug79532(off_t *array, size_t elems)
{
	for (size_t i = 0; i < elems; i++) {
		array[i] = i;
	}
}

PHP_ZEND_TEST_API int *(*bug79177_cb)(void);
void bug79177(void)
{
	bug79177_cb();
}

typedef struct bug80847_01 {
	uint64_t b;
	double c;
} bug80847_01;
typedef struct bug80847_02 {
	bug80847_01 a;
} bug80847_02;

PHP_ZEND_TEST_API bug80847_02 ffi_bug80847(bug80847_02 s) {
	s.a.b += 10;
	s.a.c -= 10.0;
	return s;
}

PHP_ZEND_TEST_API void (*bug_gh9090_void_none_ptr)(void) = NULL;
PHP_ZEND_TEST_API void (*bug_gh9090_void_int_char_ptr)(int, char *) = NULL;
PHP_ZEND_TEST_API void (*bug_gh9090_void_int_char_var_ptr)(int, char *, ...) = NULL;
PHP_ZEND_TEST_API void (*bug_gh9090_void_char_int_ptr)(char *, int) = NULL;
PHP_ZEND_TEST_API int (*bug_gh9090_int_int_char_ptr)(int, char *) = NULL;

PHP_ZEND_TEST_API void bug_gh9090_void_none(void) {
    php_printf("bug_gh9090_none\n");
}

PHP_ZEND_TEST_API void bug_gh9090_void_int_char(int i, char *s) {
    php_printf("bug_gh9090_int_char %d %s\n", i, s);
}

PHP_ZEND_TEST_API void bug_gh9090_void_int_char_var(int i, char *fmt, ...) {
    va_list args;
    char *buffer;

    va_start(args, fmt);

    zend_vspprintf(&buffer, 0, fmt, args);
    php_printf("bug_gh9090_void_int_char_var %s\n", buffer);
    efree(buffer);

    va_end(args);
}

PHP_ZEND_TEST_API int gh11934b_ffi_var_test_cdata;

enum bug_gh16013_enum {
	BUG_GH16013_A = 1,
	BUG_GH16013_B = 2,
};

struct bug_gh16013_int_struct {
	int field;
};

PHP_ZEND_TEST_API char bug_gh16013_return_char(void) {
	return 'A';
}

PHP_ZEND_TEST_API bool bug_gh16013_return_bool(void) {
	return true;
}

PHP_ZEND_TEST_API short bug_gh16013_return_short(void) {
	return 12345;
}

PHP_ZEND_TEST_API int bug_gh16013_return_int(void) {
	return 123456789;
}

PHP_ZEND_TEST_API enum bug_gh16013_enum bug_gh16013_return_enum(void) {
	return BUG_GH16013_B;
}

PHP_ZEND_TEST_API struct bug_gh16013_int_struct bug_gh16013_return_struct(void) {
	struct bug_gh16013_int_struct ret;
	ret.field = 123456789;
	return ret;
}

#ifdef HAVE_COPY_FILE_RANGE
/**
 * This function allows us to simulate early return of copy_file_range by setting the limit_copy_file_range ini setting.
 */
#ifdef __MUSL__
typedef off_t off64_t;
#endif
PHP_ZEND_TEST_API ssize_t copy_file_range(int fd_in, off64_t *off_in, int fd_out, off64_t *off_out, size_t len, unsigned int flags)
{
	ssize_t (*original_copy_file_range)(int, off64_t *, int, off64_t *, size_t, unsigned int) = dlsym(RTLD_NEXT, "copy_file_range");
	if (ZT_G(limit_copy_file_range) >= Z_L(0)) {
		len = ZT_G(limit_copy_file_range);
	}
	return original_copy_file_range(fd_in, off_in, fd_out, off_out, len, flags);
}
#endif


static PHP_FUNCTION(zend_test_create_throwing_resource)
{
	ZEND_PARSE_PARAMETERS_NONE();
	zend_resource *res = zend_register_resource(NULL, le_throwing_resource);
	ZVAL_RES(return_value, res);
}

static PHP_FUNCTION(zend_test_compile_to_ast)
{
	zend_string *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	zend_arena *ast_arena;
	zend_ast *ast = zend_compile_string_to_ast(str, &ast_arena, ZSTR_EMPTY_ALLOC());

	zend_string *result = zend_ast_export("", ast, "");

	zend_ast_destroy(ast);
	zend_arena_destroy(ast_arena);

	RETVAL_STR(result);
}

static PHP_FUNCTION(zend_test_gh18756)
{
	ZEND_PARSE_PARAMETERS_NONE();

	zend_mm_heap *heap = zend_mm_startup();
	zend_mm_gc(heap);
	zend_mm_gc(heap);
	zend_mm_shutdown(heap, true, false);
}

static PHP_FUNCTION(zend_test_opcache_preloading)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(opcache_preloading());
}

static PHP_FUNCTION(zend_test_gh19792)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETVAL_STRING("this is a non-interned string");
	zend_error(E_WARNING, "a warning");
	zend_throw_error(NULL, "an exception");
}
