#!/usr/bin/env python3
"""Connect to ESP32-C3-BLE, write bytes, read them back.

Usage:
    python ble_test.py              # default read/write test
    python ble_test.py --set-time   # write host UNIX time to device
    python ble_test.py --get-time   # read device UNIX time
    python ble_test.py --set-tz +5  # set timezone to UTC+5
    python ble_test.py --set-tz -4:30  # set timezone to UTC-4:30
    python ble_test.py --get-tz     # read device timezone
    python ble_test.py --local-time # print device local time (UTC + TZ)
    python ble_test.py --set-local  # set time and timezone from host clock
    python ble_test.py --sensor     # read temperature, pressure, humidity
"""

import argparse
import asyncio
from datetime import datetime, timezone, timedelta
import struct
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Install bleak: pip install bleak")
    sys.exit(1)

DEVICE_NAME = "ESP32-C3-BLE"
CHR_UUID = "deadbeef-1001-2000-3000-aabbccddeeff"
TIME_UUID = "deadbeef-1005-2000-3000-aabbccddeeff"
TZ_UUID = "deadbeef-1006-2000-3000-aabbccddeeff"
PRESS_UUID = "deadbeef-1002-2000-3000-aabbccddeeff"
TEMP_UUID = "deadbeef-1003-2000-3000-aabbccddeeff"
HUM_UUID = "deadbeef-1004-2000-3000-aabbccddeeff"


async def connect():
    """Scan and connect, returning a BleakClient context manager."""
    print(f"Scanning for {DEVICE_NAME}...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)
    if not device:
        print("Device not found. Is it advertising?")
        sys.exit(1)
    print(f"Found: {device.name} [{device.address}]")
    return BleakClient(device)


async def test_readwrite():
    """Write bytes to the data characteristic and read them back."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        payload = b"Hello BLE!"
        print(f"Writing: {payload}")
        await client.write_gatt_char(CHR_UUID, payload)

        data = await client.read_gatt_char(CHR_UUID)
        print(f"Read:    {data}")

        assert data == payload, "Mismatch!"
        print("OK — write/read verified.")


async def set_time():
    """Write the host's current UNIX time to the device."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        now = int(time.time())
        payload = struct.pack("<q", now)
        print(f"Setting device time to {now} ({time.ctime(now)})")
        await client.write_gatt_char(TIME_UUID, payload)

        # Read it back
        data = await client.read_gatt_char(TIME_UUID)
        readback = struct.unpack("<q", data)[0]
        print(f"Device time readback: {readback} ({time.ctime(readback)})")
        print(f"OK — delta {readback - now}s")


async def get_time():
    """Read the device's current UNIX time."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        data = await client.read_gatt_char(TIME_UUID)
        device_time = struct.unpack("<q", data)[0]
        now = int(time.time())
        print(f"Device time: {device_time} ({time.ctime(device_time)})")
        print(f"Host time:   {now} ({time.ctime(now)})")
        print(f"Delta: {device_time - now}s")


def parse_tz(s):
    """Parse a timezone string like '+5', '-4:30', '+5:45' into quarter-hours."""
    s = s.strip()
    if ":" in s:
        hours_s, mins_s = s.split(":", 1)
        hours = int(hours_s)
        mins = int(mins_s)
        if hours < 0:
            mins = -mins
    else:
        hours = int(s)
        mins = 0
    total_minutes = hours * 60 + mins
    if total_minutes % 15 != 0:
        print(f"Warning: {s} is not a multiple of 15 minutes, rounding")
    return total_minutes // 15


def format_tz(quarter_hours):
    """Format quarter-hours as UTC offset string."""
    total_minutes = quarter_hours * 15
    sign = "+" if total_minutes >= 0 else "-"
    total_minutes = abs(total_minutes)
    hours = total_minutes // 60
    mins = total_minutes % 60
    if mins:
        return f"UTC{sign}{hours}:{mins:02d}"
    return f"UTC{sign}{hours}"


async def set_tz(tz_str):
    """Write timezone offset to the device."""
    qh = parse_tz(tz_str)
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        payload = struct.pack("<b", qh)
        print(f"Setting timezone to {format_tz(qh)} ({qh} quarter-hours)")
        await client.write_gatt_char(TZ_UUID, payload)

        data = await client.read_gatt_char(TZ_UUID)
        readback = struct.unpack("<b", data)[0]
        print(f"Readback: {format_tz(readback)}")


async def get_tz():
    """Read timezone offset from the device."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        data = await client.read_gatt_char(TZ_UUID)
        qh = struct.unpack("<b", data)[0]
        print(f"Device timezone: {format_tz(qh)} ({qh} quarter-hours)")


async def local_time():
    """Read device time and timezone, print the local time."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        data = await client.read_gatt_char(TIME_UUID)
        device_time = struct.unpack("<q", data)[0]

        data = await client.read_gatt_char(TZ_UUID)
        qh = struct.unpack("<b", data)[0]

        tz_offset = timedelta(minutes=qh * 15)
        tz_info = timezone(tz_offset)
        dt = datetime.fromtimestamp(device_time, tz=tz_info)
        print(f"Device local time: {dt.strftime('%H:%M:%S %Z')} ({format_tz(qh)})")
        print(f"Date:              {dt.strftime('%a %d %b %Y')}")


async def set_local():
    """Set device time and timezone from the host's local clock."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        # Set UNIX time
        now = int(time.time())
        await client.write_gatt_char(TIME_UUID, struct.pack("<q", now))
        print(f"Time set to {now} ({time.ctime(now)})")

        # Determine host UTC offset in quarter-hours
        local_dt = datetime.now(timezone.utc).astimezone()
        utc_offset_sec = local_dt.utcoffset().total_seconds()
        qh = int(utc_offset_sec // 900)
        await client.write_gatt_char(TZ_UUID, struct.pack("<b", qh))
        print(f"Timezone set to {format_tz(qh)} ({qh} quarter-hours)")

        # Read back local time
        data = await client.read_gatt_char(TIME_UUID)
        device_time = struct.unpack("<q", data)[0]
        data = await client.read_gatt_char(TZ_UUID)
        readback_qh = struct.unpack("<b", data)[0]
        tz_info = timezone(timedelta(minutes=readback_qh * 15))
        dt = datetime.fromtimestamp(device_time, tz=tz_info)
        print(f"Device local time: {dt.strftime('%H:%M:%S %Z')} ({format_tz(readback_qh)})")


async def read_sensor():
    """Read temperature, pressure, and humidity from the device."""
    async with await connect() as client:
        print(f"Connected: {client.is_connected}")

        data = await client.read_gatt_char(TEMP_UUID)
        temp = struct.unpack("<f", data)[0]

        data = await client.read_gatt_char(PRESS_UUID)
        press = struct.unpack("<f", data)[0]

        data = await client.read_gatt_char(HUM_UUID)
        hum = struct.unpack("<f", data)[0]

        print(f"Temperature: {temp:.2f} °C")
        print(f"Pressure:    {press / 100.0:.2f} hPa")
        print(f"Humidity:    {hum:.2f} %")


def main():
    parser = argparse.ArgumentParser(description="ESP32-C3-BLE test tool")
    parser.add_argument("--set-time", action="store_true",
                        help="write host UNIX time to device")
    parser.add_argument("--get-time", action="store_true",
                        help="read device UNIX time")
    parser.add_argument("--set-tz", metavar="OFFSET",
                        help="set timezone, e.g. +5, -4:30, +5:45")
    parser.add_argument("--get-tz", action="store_true",
                        help="read device timezone")
    parser.add_argument("--local-time", action="store_true",
                        help="print device local time (UTC + TZ)")
    parser.add_argument("--set-local", action="store_true",
                        help="set time and timezone from host clock")
    parser.add_argument("--sensor", action="store_true",
                        help="read temperature, pressure, humidity")
    args = parser.parse_args()

    if args.sensor:
        asyncio.run(read_sensor())
    elif args.set_time:
        asyncio.run(set_time())
    elif args.get_time:
        asyncio.run(get_time())
    elif args.set_tz:
        asyncio.run(set_tz(args.set_tz))
    elif args.get_tz:
        asyncio.run(get_tz())
    elif args.local_time:
        asyncio.run(local_time())
    elif args.set_local:
        asyncio.run(set_local())
    else:
        asyncio.run(test_readwrite())


if __name__ == "__main__":
    main()
