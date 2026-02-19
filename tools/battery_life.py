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
import subprocess
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
TEMP_UUID = "deadbeef-1003-2000-3000-aabbccddeeff"
PRESS_UUID = "deadbeef-1002-2000-3000-aabbccddeeff"
HUM_UUID = "deadbeef-1004-2000-3000-aabbccddeeff"

BT_SCAN_TIMEOUT = 30
BT_CONNECT_TIMEOUT = 30


def ts_now():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def restart_bluetooth():
    """Restart the BlueZ bluetooth service to clear stale state."""
    print(f"[{ts_now()}] Restarting bluetooth service...")
    try:
        subprocess.run(["sudo", "systemctl", "restart", "bluetooth"],
                       timeout=30, check=True,
                       capture_output=True)
        time.sleep(3)  # give BlueZ time to settle
        print(f"[{ts_now()}] Bluetooth restarted.")
    except Exception as e:
        print(f"[{ts_now()}] Failed to restart bluetooth: {e}")


async def read_sensors():
    """Connect, read battery and sensors, disconnect.
    Returns (mv, temp_c, press_hpa, humidity) or None on failure."""
    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME, timeout=BT_SCAN_TIMEOUT)
    if not device:
        return None
    async with BleakClient(device, timeout=BT_CONNECT_TIMEOUT) as client:
        data = await client.read_gatt_char(BATT_UUID)
        mv = struct.unpack("<I", data)[0] * 2  # voltage divider

        data = await client.read_gatt_char(TEMP_UUID)
        temp = struct.unpack("<f", data)[0]

        data = await client.read_gatt_char(PRESS_UUID)
        press = struct.unpack("<f", data)[0] / 100.0  # Pa to hPa

        data = await client.read_gatt_char(HUM_UUID)
        hum = struct.unpack("<f", data)[0]

        return mv, temp, press, hum


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
        writer.writerow(["timestamp", "elapsed_min", "mv", "volts",
                         "temp_c", "press_hpa", "humidity"])
        f.flush()

    if os.geteuid() != 0:
        print("Warning: not running as root — bluetooth auto-restart "
              "requires sudo")

    start = time.time()
    reading = 0
    consecutive_errors = 0
    last_bt_restart = 0
    print(f"Logging to {args.output} every {args.interval}s"
          + (f", cutoff {args.cutoff} mV" if args.cutoff else ""))

    try:
        while True:
            # Restart bluetooth periodically (every 6 hours) or after
            # consecutive failures to prevent BlueZ from hanging
            now = time.time()
            if consecutive_errors >= 3 or (now - last_bt_restart > 6 * 3600):
                if last_bt_restart > 0 or consecutive_errors >= 3:
                    restart_bluetooth()
                last_bt_restart = now

            try:
                result = asyncio.run(read_sensors())
            except Exception as e:
                consecutive_errors += 1
                print(f"[{ts_now()}] BLE error ({consecutive_errors}): {e}")
                time.sleep(min(args.interval, 60))
                continue

            if result is None:
                consecutive_errors += 1
                print(f"[{ts_now()}] Device not found ({consecutive_errors}), retrying...")
                time.sleep(min(args.interval, 60))
                continue

            mv, temp, press, hum = result
            consecutive_errors = 0
            reading += 1
            elapsed = (time.time() - start) / 60.0
            volts = mv / 1000.0
            ts = ts_now()

            writer.writerow([ts, f"{elapsed:.1f}", mv, f"{volts:.3f}",
                             f"{temp:.2f}", f"{press:.2f}", f"{hum:.1f}"])
            f.flush()

            print(f"[{ts}] #{reading}  {elapsed:6.1f} min  "
                  f"{mv} mV  {volts:.3f} V  "
                  f"{temp:.1f}°C  {press:.1f} hPa  {hum:.0f}%")

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
