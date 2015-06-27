/*
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Hayaki Saito <saitoha@me.com>                               |
   +----------------------------------------------------------------------+
*/

/* $ Id: $ */ 

#ifndef PHP_SIXEL_H
#define PHP_SIXEL_H

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>

#ifdef HAVE_SIXEL
#define PHP_SIXEL_VERSION "0.0.1"


#include <php_ini.h>
#include <SAPI.h>
#include <ext/standard/info.h>
#include <Zend/zend_extensions.h>
#ifdef  __cplusplus
} // extern "C" 
#endif
#include <sixel.h>
#ifdef  __cplusplus
extern "C" {
#endif

extern zend_module_entry sixel_module_entry;
#define phpext_sixel_ptr &sixel_module_entry

#ifdef PHP_WIN32
#define PHP_SIXEL_API __declspec(dllexport)
#else
#define PHP_SIXEL_API
#endif

PHP_MINIT_FUNCTION(sixel);
PHP_MSHUTDOWN_FUNCTION(sixel);
PHP_RINIT_FUNCTION(sixel);
PHP_RSHUTDOWN_FUNCTION(sixel);
PHP_MINFO_FUNCTION(sixel);

#ifdef ZTS
#include "TSRM.h"
#endif

#define FREE_RESOURCE(resource) zend_list_delete(Z_LVAL_P(resource))

#define PROP_GET_LONG(name)    Z_LVAL_P(zend_read_property(_this_ce, _this_zval, #name, strlen(#name), 1 TSRMLS_CC))
#define PROP_SET_LONG(name, l) zend_update_property_long(_this_ce, _this_zval, #name, strlen(#name), l TSRMLS_CC)

#define PROP_GET_DOUBLE(name)    Z_DVAL_P(zend_read_property(_this_ce, _this_zval, #name, strlen(#name), 1 TSRMLS_CC))
#define PROP_SET_DOUBLE(name, d) zend_update_property_double(_this_ce, _this_zval, #name, strlen(#name), d TSRMLS_CC)

#define PROP_GET_STRING(name)    Z_STRVAL_P(zend_read_property(_this_ce, _this_zval, #name, strlen(#name), 1 TSRMLS_CC))
#define PROP_GET_STRLEN(name)    Z_STRLEN_P(zend_read_property(_this_ce, _this_zval, #name, strlen(#name), 1 TSRMLS_CC))
#define PROP_SET_STRING(name, s) zend_update_property_string(_this_ce, _this_zval, #name, strlen(#name), s TSRMLS_CC)
#define PROP_SET_STRINGL(name, s, l) zend_update_property_stringl(_this_ce, _this_zval, #name, strlen(#name), s, l TSRMLS_CC)


PHP_METHOD(SixelEncoder, __construct);
#if (PHP_MAJOR_VERSION >= 5)
ZEND_BEGIN_ARG_INFO_EX(SixelEncoder____construct_args, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()
#else /* PHP 4.x */
#define SixelEncoder____construct_args NULL
#endif

PHP_METHOD(SixelEncoder, __destruct);
#if (PHP_MAJOR_VERSION >= 5)
ZEND_BEGIN_ARG_INFO_EX(SixelEncoder____destruct_args, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()
#else /* PHP 4.x */
#define SixelEncoder____destruct_args NULL
#endif

PHP_METHOD(SixelEncoder, setopt);
#if (PHP_MAJOR_VERSION >= 5)
ZEND_BEGIN_ARG_INFO_EX(SixelEncoder__setopt_args, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, opt)
  ZEND_ARG_INFO(0, arg)
ZEND_END_ARG_INFO()
#else /* PHP 4.x */
#define SixelEncoder__setopt_args NULL
#endif

PHP_METHOD(SixelEncoder, encode);
#if (PHP_MAJOR_VERSION >= 5)
ZEND_BEGIN_ARG_INFO_EX(SixelEncoder__encode_args, ZEND_SEND_BY_VAL, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()
#else /* PHP 4.x */
#define SixelEncoder__encode_args NULL
#endif

#ifdef  __cplusplus
} // extern "C" 
#endif

#endif /* PHP_HAVE_SIXEL */

#endif /* PHP_SIXEL_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
