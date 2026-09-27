"""Capture one deferred-test configuration and close only its owned X11 window."""
from argparse import ArgumentParser
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import re
import sys
import time

class ClientData(ctypes.Union):
    _fields_ = [('bytes', ctypes.c_char * 20), ('shorts', ctypes.c_short * 10), ('longs', ctypes.c_long * 5)]

class ClientMessage(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong), ('send_event', ctypes.c_int),
                ('display', ctypes.c_void_p), ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong),
                ('format', ctypes.c_int), ('data', ClientData)]

class Event(ctypes.Union):
    _fields_ = [('client', ClientMessage), ('padding', ctypes.c_long * 24)]

def close_window(p_window: int) -> None:
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError('Cannot open test display')
    event = Event()
    event.client.type = 33
    event.client.send_event = 1
    event.client.display = display
    event.client.window = p_window
    event.client.message_type = x11.XInternAtom(display, b'WM_PROTOCOLS', 0)
    event.client.format = 32
    event.client.data.longs[0] = x11.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
    x11.XSendEvent(display, p_window, 0, 0, ctypes.byref(event))
    x11.XFlush(display)
    x11.XCloseDisplay(display)

def key(p_window: int, p_key: str) -> None:
    subprocess.run(['xdotool', 'keydown', '--window', str(p_window), p_key], check=True)
    time.sleep(0.2)
    subprocess.run(['xdotool', 'keyup', '--window', str(p_window), p_key], check=True)

def main() -> int:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--target', default='debug')
    parser.add_argument('--octree', choices=('1', '2'), default='2')
    parser.add_argument('--mode', choices=('capture', 'stability'), default='capture')
    parser.add_argument('--duration', type=float, default=25.0, help='Capture-free stability duration in seconds')
    parser.add_argument('--startup-timeout', type=float, default=90.0)
    parser.add_argument('--shutdown-timeout', type=float, default=25.0)
    parser.add_argument('--cpus', type=int, default=4, help='Maximum CPUs selected from the allowed affinity mask')
    parser.add_argument('--output-dir', type=Path, help='Parent directory for a fresh run directory')
    args = parser.parse_args()
    if args.cpus < 1 or min(args.duration, args.startup_timeout, args.shutdown_timeout) <= 0:
        parser.error('CPU count and durations must be positive')
    if not sys.platform.startswith('linux') or not os.environ.get('DISPLAY'):
        parser.error('This helper requires Linux with an X11 DISPLAY')
    for tool in ('xdotool', 'taskset', 'stdbuf'):
        if shutil.which(tool) is None:
            parser.error(f'Required command is unavailable: {tool}')
    root = args.repo.resolve()
    binary = root / 'build' / args.target / 'bin/wbe_render_test_vulkan_draw_sponza_deferred_test'
    if not binary.is_file():
        parser.error(f'Build the executable first: {binary}')
    parent = (args.output_dir or root / 'build/render_checks').resolve()
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=f'octree{args.octree}_{args.mode}_', dir=parent))
    capture = output / 'mipmap.png'
    log_path = output / 'application.log'
    env = dict(os.environ, WBE_WAIT_PROCESS_INJECT='0', WBE_OCT_TREE_IMPLEMENTATION=args.octree)
    for name in ('WBE_CAPTURE_PNG', 'WBE_CAPTURE_FRAME'):
        env.pop(name, None)
    if args.mode == 'capture':
        env.update(WBE_CAPTURE_PNG=str(capture), WBE_CAPTURE_FRAME='15')
    cpus = sorted(os.sched_getaffinity(0))[:args.cpus]
    cpu_list = ','.join(str(cpu) for cpu in cpus)
    result: dict[str, object] = {'octree': args.octree, 'mode': args.mode, 'cpus': cpus,
                               'clean_exit': False, 'captures': []}
    started = time.monotonic()
    with log_path.open('w') as log:
        process = subprocess.Popen(['taskset', '-c', cpu_list, 'stdbuf', '-oL', '-eL', str(binary)],
                                   cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
        print(f'RenderCheck: PID {process.pid}; artifacts: {output}', flush=True)
        window = 0

        def require_running() -> None:
            if process.poll() is not None:
                raise RuntimeError(f'Application exited before close: {process.returncode}; inspect {log_path}')

        def pause(p_seconds: float) -> None:
            deadline = time.monotonic() + p_seconds
            while time.monotonic() < deadline:
                require_running()
                time.sleep(min(0.1, max(0.0, deadline - time.monotonic())))

        def captured() -> list[tuple[str, str]]:
            return re.findall(r'Captured frame (\d+) to (.+)', log_path.read_text(errors='replace'))

        def request_capture(p_label: str) -> None:
            before = len(captured())
            key(window, 'F1')
            deadline = time.monotonic() + args.startup_timeout
            while len(captured()) <= before:
                if time.monotonic() >= deadline:
                    raise RuntimeError(f'Timed out waiting for {p_label} capture')
                pause(0.1)
            frame, source = captured()[-1]
            destination = output / f'{p_label}.png'
            shutil.copy2(source, destination)
            captures = result['captures']
            assert isinstance(captures, list)
            captures.append({'frame': int(frame), 'path': str(destination)})

        try:
            deadline = time.monotonic() + args.startup_timeout
            while time.monotonic() < deadline:
                require_running()
                found = subprocess.run(['xdotool', 'search', '--pid', str(process.pid)], capture_output=True, text=True)
                if found.returncode == 0:
                    window = int(found.stdout.splitlines()[0])
                ready = 'Octree construction:' in log_path.read_text(errors='replace')
                if window and ready and (args.mode == 'stability' or captured()):
                    break
                pause(0.2)
            else:
                raise RuntimeError('Startup or initial frame capture timed out')
            if args.mode == 'stability':
                pause(args.duration)
            else:
                result['captures'] = [{'frame': int(captured()[0][0]), 'path': str(capture)}]
                key(window, 'r')
                pause(1)
                if 'Deferred shadow filtering: summed-area table.' not in log_path.read_text(errors='replace'):
                    raise RuntimeError('Lighting graph did not switch to summed-area table')
                request_capture('summed_area_table')
                pause(3)
                request_capture('summed_area_table_later')
                key(window, 'v')
                pause(1)
                request_capture('voxels')
                pause(4)
                request_capture('voxels_later')
            close_window(window)
            code = process.wait(timeout=args.shutdown_timeout)
            result['exit_code'] = code
            if code != 0:
                raise RuntimeError(f'Application exited with {code}')
            result['clean_exit'] = True
        except (RuntimeError, subprocess.SubprocessError, OSError, KeyboardInterrupt) as error:
            result['error'] = str(error) or type(error).__name__
            print(f'RenderCheck: FAILED: {result["error"]}', flush=True)
        finally:
            if process.poll() is None:
                # Cleanup is not a successful shutdown. Never leave an owned GPU process behind.
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            result['exit_code'] = process.returncode
            result['elapsed_seconds'] = round(time.monotonic() - started, 3)
            (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'RenderCheck: clean_exit={result["clean_exit"]}; result: {output / "result.json"}', flush=True)
    return 0 if result['clean_exit'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
