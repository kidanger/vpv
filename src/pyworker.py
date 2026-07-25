# Worker process driving the PYTHON edit backend of vpv.
#
# This script is embedded into the vpv binary at configure time (see
# cmake/modules/GeneratePyWorker.cmake) and launched with `python3 -c <script>`.
# It speaks a small binary protocol on stdin/stdout, described in
# src/PythonWorker.hpp. Keep both sides in sync.
#
# It can also be run standalone for testing:
#   python3 src/pyworker.py < request.bin > response.bin

import ast
import os
import struct
import sys
import traceback

MAGIC_HELLO = b"VPVH"
MAGIC_REQUEST = b"VPVQ"
MAGIC_RESPONSE = b"VPVR"

STATUS_OK = 0
STATUS_ERROR = 1

TRANSPORT_PIPE = 0

# Input images are bound to these names, in order, to match the convention users
# already have from plambda. Beyond the 26th image only `I` is available.
IMAGE_NAMES = "xyzabcdefghijklmnopqrstuvw"

# Populated by main(). The user's script never sees the real stdout, so a stray
# print() cannot corrupt the protocol stream.
_in = None
_out = None


def read_exact(n):
    """Read exactly n bytes, or return None on clean EOF."""
    buf = bytearray(n)
    view = memoryview(buf)
    got = 0
    while got < n:
        k = _in.readinto(view[got:])
        if not k:
            if got == 0:
                return None
            raise EOFError(
                "unexpected end of input: wanted {} bytes, got {}".format(n, got)
            )
        got += k
    return buf


def write_frame(payload):
    _out.write(payload)
    _out.flush()


def _message(text):
    MAX_MESSAGE_SIZE = 1 << 20
    encoded = text.encode("utf-8", "replace")
    if len(encoded) > MAX_MESSAGE_SIZE:
        end = "[trunc]".encode("utf-8")
        encoded = encoded[: MAX_MESSAGE_SIZE - len(end)] + end
    return struct.pack("<I", len(encoded)) + encoded


def send_error(msg):
    write_frame(MAGIC_RESPONSE + struct.pack("<I", STATUS_ERROR) + _message(msg))


def send_image(array):
    h, w, c = array.shape
    write_frame(
        MAGIC_RESPONSE + struct.pack("<IIII", STATUS_OK, w, h, c) + array.tobytes()
    )


def compile_program(source):
    """Compile source so that a trailing bare expression becomes the result.

    Returns (body_code, tail_code); tail_code is None when the script does not
    end in an expression, in which case the result is taken from `out`.
    """
    tree = ast.parse(source, filename="<vpv>", mode="exec")
    tail = None
    if tree.body and isinstance(tree.body[-1], ast.Expr):
        tail = ast.Expression(tree.body.pop().value)
        ast.fix_missing_locations(tail)
    body = compile(tree, "<vpv>", "exec")
    if tail is not None:
        tail = compile(tail, "<vpv>", "eval")
    return body, tail


def normalize(result, reference_shape):
    """Coerce a script result into a contiguous float32 (h, w, c) array."""
    import numpy as np

    array = np.asarray(result)
    if array.dtype.kind not in "biuf":
        raise TypeError(
            "the script returned {} (dtype {}), which is not numeric".format(
                type(result).__name__, array.dtype
            )
        )

    if array.ndim == 0:
        # A scalar broadcasts over the geometry of the first input image.
        h, w, _ = reference_shape
        array = np.broadcast_to(array, (h, w, 1))
    elif array.ndim == 2:
        array = array[:, :, np.newaxis]
    elif array.ndim != 3:
        raise ValueError(
            "the script returned a {}-dimensional array; expected 2 (h, w) "
            "or 3 (h, w, c)".format(array.ndim)
        )

    return np.ascontiguousarray(array, dtype=np.float32)


def handle_request():
    import numpy as np

    header = read_exact(4)
    if header is None:
        return False
    if bytes(header) != MAGIC_REQUEST:
        raise EOFError("desynchronized stream: bad request magic")

    nimages, proglen = struct.unpack("<II", read_exact(8))

    geometries = [struct.unpack("<III", read_exact(12)) for _ in range(nimages)]
    source = bytes(read_exact(proglen)).decode("utf-8")

    images = []
    for w, h, c in geometries:
        raw = read_exact(w * h * c * 4)
        images.append(np.frombuffer(raw, dtype="<f4").reshape(h, w, c))

    # Everything is read; from here on any failure is a normal error response.
    env = {"np": np, "I": images}
    for i, image in enumerate(images):
        if i < len(IMAGE_NAMES):
            env[IMAGE_NAMES[i]] = image

    try:
        body, tail = compile_program(source)
        exec(body, env, env)
        if tail is not None:
            result = eval(tail, env, env)
        elif "out" in env:
            result = env["out"]
        else:
            raise ValueError(
                "the script produced no result: end it with an expression or "
                "assign to `out`"
            )
        if result is None:
            raise ValueError("the script returned None")
        reference = images[0].shape if images else (0, 0, 0)
        send_image(normalize(result, reference))
    except SyntaxError as e:
        send_error("".join(traceback.format_exception_only(type(e), e)).strip())
    except BaseException:
        # Drop the two innermost frames, which are this function's own.
        etype, value, tb = sys.exc_info()
        send_error(
            "".join(traceback.format_exception(etype, value, tb.tb_next or tb)).strip()
        )

    return True


def main():
    global _in, _out

    _in = os.fdopen(os.dup(0), "rb", 0)
    _out = os.fdopen(os.dup(1), "wb", 0)
    # Redirect fd 1 to stderr so user code printing to stdout is harmless.
    os.dup2(2, 1)
    sys.stdout = sys.stderr

    try:
        import numpy  # noqa: F401
    except ImportError as e:
        write_frame(
            MAGIC_HELLO
            + struct.pack("<I", STATUS_ERROR)
            + _message("numpy is not available in {}: {}".format(sys.executable, e))
        )
        return 1

    write_frame(MAGIC_HELLO + struct.pack("<I", STATUS_OK) + _message(sys.version))

    while True:
        try:
            if not handle_request():
                return 0
        except EOFError as e:
            # Protocol-level failure: the stream is no longer trustworthy.
            sys.stderr.write("vpv python worker: {}\n".format(e))
            return 1


if __name__ == "__main__":
    sys.exit(main())
