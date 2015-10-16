#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <uv.h>

#include "fnav/event/stream.h"
#include "fnav/log.h"

static void close_cb(uv_handle_t *handle);

/// Sets the stream associated with `fd` to "blocking" mode.
///
/// @return `0` on success, or `-errno` on failure.
int stream_set_blocking(int fd, bool blocking)
{
  log_msg("STREAM", "stream_set_blocking");
  // Private loop to avoid conflict with existing watcher(s):
  //    uv__io_stop: Assertion `loop->watchers[w->fd] == w' failed.
  uv_loop_t loop;
  uv_pipe_t stream;
  uv_loop_init(&loop);
  uv_pipe_init(&loop, &stream, 0);
  uv_pipe_open(&stream, fd);
  int retval = uv_stream_set_blocking((uv_stream_t *)&stream, blocking);
  uv_close((uv_handle_t *)&stream, NULL);
  uv_run(&loop, UV_RUN_NOWAIT);  // not necessary, but couldn't hurt.
  uv_loop_close(&loop);
  return retval;
}

void stream_init(Loop *loop, Stream *stream, int fd, uv_stream_t *uvstream,
    void *data)
{
  log_msg("STREAM", "init");
  stream->uvstream = uvstream;

  if (fd >= 0) {
    uv_handle_type type = uv_guess_handle(fd);
    stream->fd = fd;

    if (type == UV_FILE) {
      // Non-blocking file reads are simulated with an idle handle that reads in
      // chunks of the ring buffer size, giving time for other events to be
      // processed between reads.
      uv_idle_init(&loop->uv, &stream->uv.idle);
      stream->uv.idle.data = stream;
    } else {
      assert(type == UV_NAMED_PIPE || type == UV_TTY);
      uv_pipe_init(&loop->uv, &stream->uv.pipe, 0);
      uv_pipe_open(&stream->uv.pipe, fd);
      stream->uvstream = (uv_stream_t *)&stream->uv.pipe;
    }
  }

  if (stream->uvstream) {
    stream->uvstream->data = stream;
    loop = stream->uvstream->loop->data;
  }

  stream->data = data;
  stream->internal_data = NULL;
  stream->fpos = 0;
  stream->curmem = 0;
  stream->maxmem = 0;
  stream->pending_reqs = 0;
  stream->read_cb = NULL;
  stream->write_cb = NULL;
  stream->close_cb = NULL;
  stream->internal_close_cb = NULL;
  stream->closed = false;
  stream->buffer = NULL;
}

void stream_close_handle(Stream *stream)
{
  log_msg("STREAM", "stream_close_handle");
  if (stream->uvstream) {
    uv_close((uv_handle_t *)stream->uvstream, close_cb);
  } else {
    uv_close((uv_handle_t *)&stream->uv.idle, close_cb);
  }
}

void stream_close(Stream *stream, stream_close_cb on_stream_close)
{
  log_msg("STREAM", "close");
  assert(!stream->closed);
  stream->closed = true;
  stream->close_cb = on_stream_close;

  if (!stream->pending_reqs) {
    stream_close_handle(stream);
  }
}

static void close_cb(uv_handle_t *handle)
{
  log_msg("STREAM", "close_cb");
  Stream *stream = handle->data;
  if (stream->buffer) {
    rbuffer_free(stream->buffer);
  }
  if (stream->close_cb) {
    stream->close_cb(stream, stream->data);
  }
  if (stream->internal_close_cb) {
    stream->internal_close_cb(stream, stream->internal_data);
  }
}
