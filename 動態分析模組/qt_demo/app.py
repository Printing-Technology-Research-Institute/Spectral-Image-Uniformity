import os
import sys
from pathlib import Path


_runtime_log_handle = None


def _restore_windowed_standard_streams() -> None:
    """Give native GenTL libraries valid stdout/stderr handles under pythonw."""
    global _runtime_log_handle
    if sys.stdout is not None and sys.stderr is not None:
        return
    log_path = Path(__file__).resolve().parent / "runtime_console.log"
    _runtime_log_handle = log_path.open(
        "a", encoding="utf-8", errors="backslashreplace", buffering=1
    )
    for descriptor in (1, 2):
        try:
            os.dup2(_runtime_log_handle.fileno(), descriptor)
        except OSError:
            pass
    try:
        import ctypes
        import msvcrt

        native_handle = msvcrt.get_osfhandle(_runtime_log_handle.fileno())
        ctypes.windll.kernel32.SetStdHandle(-11, native_handle)
        ctypes.windll.kernel32.SetStdHandle(-12, native_handle)
    except (ImportError, OSError, AttributeError):
        pass
    sys.stdout = _runtime_log_handle
    sys.stderr = _runtime_log_handle


_restore_windowed_standard_streams()

from simulation_app import main


if __name__ == "__main__":
    raise SystemExit(main())
