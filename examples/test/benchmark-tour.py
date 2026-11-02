#!/usr/bin/env python3
"""Stream per-case measurements from the C benchmark's v32sim checkpoints."""

import argparse
import csv
import re
import struct
import subprocess
import sys

CASES = ("Boids", "Boids", "BunnyMark", "BunnyMark", "Alpha Swarm", "Alpha Swarm")
VALUE = re.compile(r"^0x(203|207|208|209)/[dDxX]:\s*(0x[0-9a-fA-F]+|[0-9]+)")


def main():
    """Run the tour once and print each checkpoint as soon as it is reached."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator")
    parser.add_argument("bios")
    parser.add_argument("rom")
    parser.add_argument("profile")
    parser.add_argument("log")
    parser.add_argument("--results")
    parser.add_argument("--label", default="C")
    args = parser.parse_args()

    commands = ["gamepad0 connect", "gamepad0 start", "watch/d GPU_DrawingPointY >= 1001"]
    for _ in CASES:
        commands.extend(("continue", "print/d GPU_DrawingPointX", "print/X GPU_MultiplyColor", "print/X GPU_DrawingScaleX",
                         "print/d GPU_DrawingPointY"))
    commands.extend(("watch GPU_DrawingPointY == 6", "continue", "print/d GPU_DrawingPointX",
                     "print/d GPU_DrawingPointY", "quit"))
    command = [args.simulator, "--batch", "--no-debug", "--errorcheck", "--exit-status", "--fast",
               "--fixed-clock", "--seed=1", "--max-frames=10000", "--biosfile", args.bios,
               "--entry-point", "0x10000004", "--profile-out=" + args.profile, args.rom]
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, bufsize=1)
    process.stdin.write("\n".join(commands) + "\n")
    process.stdin.close()
    values = {}
    completed = 0
    finished = False
    checksum = 0
    print(f"\n{args.label} benchmark tour (v32sim):", flush=True)
    print("  Case         Count      FPS     GPU%      CPU%", flush=True)
    result_file = open(args.results, "w", newline="", encoding="utf-8") if args.results else None
    writer = csv.writer(result_file) if result_file else None
    if writer:
        writer.writerow(("case", "count", "fps", "gpu_percent", "cpu_percent"))
    with open(args.log, "w", encoding="utf-8") as log:
        for line in process.stdout:
            log.write(line)
            match = VALUE.match(line.strip())
            if not match:
                continue
            port, value = match.groups()
            values[port] = int(value, 0)
            if port != "208":
                continue
            if completed == len(CASES):
                if values.get("207") != checksum % 997 or values["208"] != len(CASES):
                    raise RuntimeError("benchmark completion marker did not match")
                finished = True
                values.clear()
                continue
            if completed >= len(CASES) or set(values) != {"203", "207", "208", "209"}:
                raise RuntimeError("unexpected simulator checkpoint")
            name = CASES[completed]
            marker = values["208"] - 1001
            count = values["207"] + (marker // len(CASES)) * 1000
            if marker < 0 or marker % len(CASES) != completed or not 0 <= values["207"] < 1000:
                raise RuntimeError("benchmark case did not match the expected sequence")
            checksum += (completed + 1) * (count + 1)
            packed = values["203"]
            fps = struct.unpack("<f", struct.pack("<I", values["209"]))[0]
            gpu = (packed & 0x3fff) / 100.0
            cpu = (packed >> 14) / 100.0
            print(f"  {name:<12} {count:>5}  {fps:>7.2f}  {gpu:>6.2f}%  {cpu:>7.2f}%", flush=True)
            if writer:
                writer.writerow((name, count, f"{fps:.2f}", f"{gpu:.2f}", f"{cpu:.2f}"))
                result_file.flush()
            completed += 1
            values.clear()
    status = process.wait()
    if result_file:
        result_file.close()
    if status != 0 or completed != len(CASES) or not finished:
        raise RuntimeError(f"v32sim exited with status {status} after {completed} of {len(CASES)} cases; see {args.log}")
    print("  FPS is completed updates per second; CPU/GPU are averages per update.\n", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"benchmark tour: {error}", file=sys.stderr, flush=True)
        sys.exit(1)
