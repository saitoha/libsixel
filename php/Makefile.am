# PHP bindings: bundled FFI package builder

if HAVE_PHP

PHP_PACKAGE_STAMP = $(abs_top_builddir)/php/dist/.php-package-stamp

PHP_PACKAGE_INPUTS = \
	$(srcdir)/composer.json \
	$(srcdir)/package_builder.php \
	$(srcdir)/README \
	$(srcdir)/README.md \
	$(top_builddir)/src/libsixel.la \
	$(wildcard $(srcdir)/src/*.php) \
	$(wildcard $(srcdir)/src/Libsixel/*.php)

$(PHP_PACKAGE_STAMP): $(PHP_PACKAGE_INPUTS)
	$(AM_V_at) mkdir -p "$(abs_top_builddir)/php/dist"
	$(AM_V_GEN) $(PHP) $(srcdir)/package_builder.php \
		--libdir "$(top_builddir)/src/.libs" \
		--distdir "$(abs_top_builddir)/php/dist" \
		--version "$(PACKAGE_VERSION)"
	$(AM_V_at) touch "$@"

all-local: $(PHP_PACKAGE_STAMP)

install-exec-local:
	@echo "PHP package install is not performed by make install."
	@echo "Use Composer or extract php/dist/libsixel-php-* archive."

endif

clean-local:
	rm -rf dist
	rm -f $(abs_top_builddir)/php/dist/.php-package-stamp
	if test -d "$(builddir)/src/_libs"; then \
		find "$(builddir)/src/_libs" -type f -exec rm -f {} +; \
		rmdir "$(builddir)/src/_libs" 2>/dev/null || true; \
	fi

EXTRA_DIST = \
	composer.json \
	package_builder.php \
	README \
	README.md \
	src/autoload.php \
	src/compat.php \
	src/Libsixel/API.php \
	src/Libsixel/Constants.php \
	src/Libsixel/Decoder.php \
	src/Libsixel/Encoder.php \
	src/Libsixel/Exception.php
