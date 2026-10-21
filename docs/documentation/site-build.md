# Building the documentation site

This how-to is for contributors who want to read or check the documentation as the searchable HTML site that MkDocs builds from `docs/`. The site's navigation is generated from the [documentation index](../README.md); the [documentation instructions](../AGENTS.md) own the rules for the documents themselves.

Run `tools/build-docs` from any directory to build the complete searchable HTML site under `site/`. The command creates and caches a private virtual environment under `.docs-build/`; MkDocs and Material for MkDocs remain optional and are not required for configuring, building, testing, or installing libsixel itself. The first documentation build requires network access to install the versions pinned in `tools/docs/requirements.txt`; subsequent builds reuse the cached environment.

An Autotools build provides the equivalent `make docs` target, and a Meson build provides `meson compile -C BUILDDIR docs`. Both write the generated site to `site/` under their build directory. Pass `--no-bootstrap` to `tools/build-docs` only when the current Python environment already contains the pinned documentation packages.
