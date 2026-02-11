#!/usr/bin/env python3
"""Monitor battery voltage over BLE and log to CSV.

Usage:
    python battery_life.py                  # log every 60s to battery_log.csv
    python battery_life.py -i 300           # log every 5 minutes
    python battery_life.py -o my_test.csv   # custom output file
    python battery_life.py --cutoff 3000    # stop at 3000 mV
"""

import argparse
import asyncio
import csv
import os
import sys
import time
from datetime import datetime

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Install bleak: pip install bleak")
    sys.exit(1)

import struct

DEVICE_NAME = "ESP32-C3-BLE"
BATT_UUID = "deadbeef-1007-2000-3000-aabbccddeeff"


async def read_battery_mv():
    """Connect, read battery voltage, disconnect."""
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)
    if not device:
        return None
    async with BleakClient(device) as client:
        data = await client.read_gatt_char(BATT_UUID)
        return struct.unpack("<I", data)[0] * 2  # voltage divider


def main():
    parser = argparse.ArgumentParser(description="Battery life logger")
    parser.add_argument("-i", "--interval", type=int, default=60,
                        help="seconds between readings (default: 60)")
    parser.add_argument("-o", "--output", default="battery_log.csv",
                        help="output CSV file (default: battery_log.csv)")
    parser.add_argument("--cutoff", type=int, default=0,
                        help="stop when voltage drops below this mV (0 = never)")
    args = parser.parse_args()

    file_exists = os.path.exists(args.output)
    f = open(args.output, "a", newline="")
    writer = csv.writer(f)
    if not file_exists:
        writer.writerow(["timestamp", "elapsed_min", "mv", "volts"])
        f.flush()

    start = time.time()
    reading = 0
    print(f"Logging to {args.output} every {args.interval}s"
          + (f", cutoff {args.cutoff} mV" if args.cutoff else ""))

    try:
        while True:
            try:
                mv = asyncio.run(read_battery_mv())
            except Exception as e:
                print(f"  BLE error: {e}")
                time.sleep(args.interval)
                continue

            if mv is None:
                print(f"  Device not found, retrying in {args.interval}s...")
                time.sleep(args.interval)
                continue

            reading += 1
            elapsed = (time.time() - start) / 60.0
            volts = mv / 1000.0
            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

            writer.writerow([ts, f"{elapsed:.1f}", mv, f"{volts:.3f}"])
            f.flush()

            print(f"[{ts}] #{reading}  {elapsed:6.1f} min  {mv} mV  {volts:.3f} V")

            if args.cutoff and mv < args.cutoff:
                print(f"Voltage {mv} mV below cutoff {args.cutoff} mV — stopping.")
                break

            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        f.close()
        elapsed = (time.time() - start) / 60.0
        print(f"Total: {reading} readings over {elapsed:.1f} minutes")
        print(f"Log saved to {args.output}")


if __name__ == "__main__":
    main()
