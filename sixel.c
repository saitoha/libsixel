/*
 * Copyright (c) 2015 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* $ Id: $ */ 

#include "php_sixel.h"

#if HAVE_SIXEL

/* {{{ Class definitions */

/* {{{ Class SixelEncoder */

static zend_class_entry * SixelEncoder_ce_ptr = NULL;

/* {{{ Methods */


/* {{{ proto object __construct()
   */
PHP_METHOD(SixelEncoder, __construct)
{
	zend_class_entry * _this_ce;
	zval * _this_zval;



	if (ZEND_NUM_ARGS()>0)  {
		WRONG_PARAM_COUNT;
	}


	do {
		SIXELSTATUS status;
				sixel_encoder_t *encoder;
				zval *value;
				status = sixel_encoder_new(&encoder, NULL);
				if (SIXEL_FAILED(status)) {
#if 0
					zend_throw_exception_ex(zend_exception_get_default(), 1,
											"sixel_encoder_new() failed: %s (%s:%d)",
											sixel_helper_format_error(status),
											__FILE__, __LINE__);
#endif
				} else {
					value = emalloc(sizeof(zval));
					ZVAL_RESOURCE(value, (long)encoder);
					zend_update_property(_this_ce, getThis(),
										 "encoder", sizeof("encoder") - 1, value);
				}
	} while (0);
}
/* }}} __construct */



/* {{{ proto object __destruct()
   */
PHP_METHOD(SixelEncoder, __destruct)
{
	zend_class_entry * _this_ce;

	zval * _this_zval = NULL;



	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &_this_zval, SixelEncoder_ce_ptr) == FAILURE) {
		return;
	}

	_this_ce = Z_OBJCE_P(_this_zval);


	do {
		zval *encoder;
				encoder = zend_read_property(_this_ce, getThis(),
											 "encoder", sizeof("encoder") - 1, 1);
				sixel_encoder_unref((sixel_encoder_t *)Z_RESVAL_P(encoder));
				efree(encoder);
	} while (0);
}
/* }}} __destruct */



/* {{{ proto void setopt(string opt[, string arg])
   */
PHP_METHOD(SixelEncoder, setopt)
{
	zend_class_entry * _this_ce;

	zval * _this_zval = NULL;
	const char * opt = NULL;
	int opt_len = 0;
	const char * arg = NULL;
	int arg_len = 0;



	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|s", &_this_zval, SixelEncoder_ce_ptr, &opt, &opt_len, &arg, &arg_len) == FAILURE) {
		return;
	}

	_this_ce = Z_OBJCE_P(_this_zval);


	do {
		SIXELSTATUS status;
				zval *encoder;
				encoder = zend_read_property(_this_ce, getThis(),
											 "encoder", sizeof("encoder") - 1, 1);
				status = sixel_encoder_setopt((sixel_encoder_t *)Z_RESVAL_P(encoder), *opt, arg);
#if 0
				if (SIXEL_FAILED(status) {
					zend_throw_exception_ex(zend_exception_get_default(), 1,
											"sixel_encoder_encode() failed: %s (%s:%d)",
											sixel_helper_format_error(status),
											__FILE__, __LINE__);
				}
#endif
	} while (0);
}
/* }}} setopt */



/* {{{ proto void encode(string filename)
   */
PHP_METHOD(SixelEncoder, encode)
{
	zend_class_entry * _this_ce;

	zval * _this_zval = NULL;
	const char * filename = NULL;
	int filename_len = 0;



	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &_this_zval, SixelEncoder_ce_ptr, &filename, &filename_len) == FAILURE) {
		return;
	}

	_this_ce = Z_OBJCE_P(_this_zval);


	do {
		SIXELSTATUS status;
				zval *encoder;
				encoder = zend_read_property(_this_ce, getThis(),
											 "encoder", sizeof("encoder") - 1, 1);
				status = sixel_encoder_encode((sixel_encoder_t *)Z_RESVAL_P(encoder), filename);
#if 0
				if (SIXEL_FAILED(status) {
					zend_throw_exception_ex(zend_exception_get_default(), 1,
											"sixel_encoder_encode() failed: %s (%s:%d)",
											sixel_helper_format_error(status),
											__FILE__, __LINE__);
				}
#endif
	} while (0);
}
/* }}} encode */


static zend_function_entry SixelEncoder_methods[] = {
	PHP_ME(SixelEncoder, __construct, NULL, /**/ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(SixelEncoder, __destruct, NULL, /**/ZEND_ACC_PUBLIC)
	PHP_ME(SixelEncoder, setopt, SixelEncoder__setopt_args, /**/ZEND_ACC_PUBLIC)
	PHP_ME(SixelEncoder, encode, SixelEncoder__encode_args, /**/ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

/* }}} Methods */

static void class_init_SixelEncoder(void)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "SixelEncoder", SixelEncoder_methods);
	SixelEncoder_ce_ptr = zend_register_internal_class(&ce);
}

/* }}} Class SixelEncoder */

/* }}} Class definitions*/

/* {{{ sixel_functions[] */
zend_function_entry sixel_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */


/* {{{ sixel_module_entry
 */
zend_module_entry sixel_module_entry = {
	STANDARD_MODULE_HEADER,
	"sixel",
	sixel_functions,
	PHP_MINIT(sixel),     /* Replace with NULL if there is nothing to do at php startup   */ 
	PHP_MSHUTDOWN(sixel), /* Replace with NULL if there is nothing to do at php shutdown  */
	PHP_RINIT(sixel),     /* Replace with NULL if there is nothing to do at request start */
	PHP_RSHUTDOWN(sixel), /* Replace with NULL if there is nothing to do at request end   */
	PHP_MINFO(sixel),
	PHP_SIXEL_VERSION, 
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SIXEL
ZEND_GET_MODULE(sixel)
#endif


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(sixel)
{
	class_init_SixelEncoder();

	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(sixel)
{

	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(sixel)
{
	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(sixel)
{
	/* add your stuff here */

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(sixel)
{
	php_printf("PHP interface to libsixel\n");
	php_info_print_table_start();
	php_info_print_table_row(2, "Version",PHP_SIXEL_VERSION " (alpha)");
	php_info_print_table_row(2, "Released", "2015-06-23");
	php_info_print_table_row(2, "CVS Revision", "$Id: $");
	php_info_print_table_row(2, "Authors", "Hayaki Saito 'saitoha@me.com' (developer)\n");
	php_info_print_table_end();
	/* add your stuff here */

}
/* }}} */

#endif /* HAVE_SIXEL */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
