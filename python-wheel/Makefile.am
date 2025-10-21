PYPI_REPO = @PYPI_REPO@
PYPI_USERNAME = @PYPI_USERNAME@
PYPI_PASSWORD_ENV = @PYPI_PASSWORD_ENV@
PYTHON_FOR_SHARED = @PYTHON_FOR_SHARED@
PYTHON_FALLBACK = python3
PYTHON_CMD = $(if $(strip $(PYTHON_FOR_SHARED)),$(PYTHON_FOR_SHARED),$(PYTHON_FALLBACK))

SHARED_SYNC_SCRIPT = $(top_srcdir)/python-shared/sync_shared_sources.py
STAGE_DIR = $(abs_builddir)/.stage
DIST_DIR = $(builddir)/dist
WHEELHOUSE_DIR = $(builddir)/wheelhouse

.PHONY: all-local shared-sync wheel-local upload-local clean-local install uninstall

shared-sync:
	@$(PYTHON_CMD) $(SHARED_SYNC_SCRIPT) $(top_srcdir)

all-local: wheel-local

wheel-local: shared-sync
	@rm -rf $(STAGE_DIR)
	@mkdir -p $(STAGE_DIR)
	@$(MAKE) -C $(top_builddir) install DESTDIR=$(STAGE_DIR)
	@bash $(srcdir)/scripts/copy_libsixel_artifacts.sh $(STAGE_DIR)
	@rm -rf $(DIST_DIR) $(WHEELHOUSE_DIR)
	@mkdir -p $(DIST_DIR) $(WHEELHOUSE_DIR)
	@$(PYTHON_CMD) -m build --wheel --outdir $(DIST_DIR) $(srcdir)
	@wheel_path=`ls $(DIST_DIR)/libsixel_wheel-*.whl 2>/dev/null | head -n1`; \
	if [ -z "$$wheel_path" ]; then \
		echo "wheel build failed" >&2; exit 1; \
	fi; \
	uname_s=`uname`; \
	if [ "$$uname_s" = "Linux" ]; then \
		if command -v auditwheel >/dev/null 2>&1; then \
			auditwheel -v repair -w $(WHEELHOUSE_DIR) "$$wheel_path"; \
		else \
			echo "auditwheel not found, copying wheel without repair" >&2; \
			cp "$$wheel_path" $(WHEELHOUSE_DIR)/; \
		fi; \
	elif [ "$$uname_s" = "Darwin" ]; then \
		if command -v delocate-wheel >/dev/null 2>&1; then \
			delocate-wheel -v -w $(WHEELHOUSE_DIR) "$$wheel_path"; \
		else \
			echo "delocate-wheel not found, copying wheel without repair" >&2; \
			cp "$$wheel_path" $(WHEELHOUSE_DIR)/; \
		fi; \
	else \
		cp "$$wheel_path" $(WHEELHOUSE_DIR)/; \
	fi

upload-local: wheel-local
	@PYPI_REPO=$(PYPI_REPO) PYPI_USERNAME=$(PYPI_USERNAME) PYPI_PASSWORD_ENV=$(PYPI_PASSWORD_ENV) \
		bash $(srcdir)/scripts/meson_upload_wheel.sh

clean-local:
	@rm -rf $(STAGE_DIR) $(DIST_DIR) $(WHEELHOUSE_DIR)

install:
	@:

uninstall:
	@:
