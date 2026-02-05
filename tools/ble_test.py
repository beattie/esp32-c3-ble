#!/usr/bin/env python3
"""Connect to ESP32-C3-BLE, write bytes, read them back."""

import asyncio
import sys

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Install bleak: pip install bleak")
    sys.exit(1)

DEVICE_NAME = "ESP32-C3-BLE"
CHR_UUID = "deadbeef-1001-2000-3000-aabbccddeeff"


async def main():
    print(f"Scanning for {DEVICE_NAME}...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)
    if not device:
        print("Device not found. Is it advertising?")
        sys.exit(1)

    print(f"Found: {device.name} [{device.address}]")

    async with BleakClient(device) as client:
        print(f"Connected: {client.is_connected}")

        # Write some bytes
        payload = b"Hello BLE!"
        print(f"Writing: {payload}")
        await client.write_gatt_char(CHR_UUID, payload)

        # Read them back
        data = await client.read_gatt_char(CHR_UUID)
        print(f"Read:    {data}")

        assert data == payload, "Mismatch!"
        print("OK — write/read verified.")


if __name__ == "__main__":
    asyncio.run(main())
