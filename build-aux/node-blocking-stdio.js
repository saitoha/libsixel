// C stdio uses synchronous Node filesystem calls. Node creates POSIX pipe
// handles in nonblocking mode; writeSync can then fail with EAGAIN midway
// through an image. Set the shared descriptors before starting any pthread.
// Worker streams are message proxies and have no native descriptor handle.
if (typeof process === 'object' && process.versions && process.versions.node &&
    require('worker_threads').isMainThread) {
  [process.stdin, process.stdout, process.stderr].forEach(function(stream) {
    if (stream._handle && typeof stream._handle.setBlocking === 'function') {
      stream._handle.setBlocking(true);
    }
  });
}
