# This helper runs beside the shell, never through the target's emulator.
have_test_compare = add_languages('c', native: true, required: false)
if have_test_compare
  test_compare = executable('cmp', '../compare.c', native: true,
    install: false)
endif
