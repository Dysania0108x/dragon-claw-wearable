import asyncio
from bleak import BleakScanner


async def main():
    print("Scanning for BLE devices...")

    devices = await BleakScanner.discover(timeout=10)

    print("\nFound devices:")

    for device in devices:
        print(f"{device.name}  |  {device.address}")


asyncio.run(main())