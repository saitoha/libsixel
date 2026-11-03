"""Launch native test tools with the POSIX shell's ENOEXEC fallback."""
import errno
import os
import subprocess


def run(command, **kwargs):
    """Preserve direct execution, including every error except ENOEXEC."""
    try:
        return subprocess.run(command, **kwargs)
    except OSError as error:
        if os.name != 'posix' or error.errno != errno.ENOEXEC:
            raise
    # APE binaries and executable shell text can lack a kernel-recognized
    # header. Pass arguments as distinct words; never interpolate shell code.
    return subprocess.run(['/bin/sh', *command], **kwargs)


def popen(command, **kwargs):
    """Keep streaming callers' descriptors and child ownership unchanged."""
    try:
        return subprocess.Popen(command, **kwargs)
    except OSError as error:
        if os.name != 'posix' or error.errno != errno.ENOEXEC:
            raise
    return subprocess.Popen(['/bin/sh', *command], **kwargs)
