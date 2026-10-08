import asyncio
from bleak import BleakScanner, BleakClient

DEVICE_NAME = "Museum-ESP32"

CHARACTERISTIC_UUID = "87654321-4321-4321-4321-cba987654321"


async def main():
    print("Searching for Museum-ESP32...")

    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME,
        timeout=10
    )

    if device is None:
        print("Museum-ESP32 not found.")
        return

    print("Found ESP32!")
    print("Connecting...")

    async with BleakClient(device) as client:
        print("Connected!")

        while True:
            command = input(
                "\n1 = vibration ON\n"
                "0 = vibration OFF\n"
                "2 = short vibration\n"
                "q = quit\n"
                "> "
            )

            if command == "q":
                await client.write_gatt_char(
                    CHARACTERISTIC_UUID,
                    b"0"
                )
                print("Disconnected.")
                break

            elif command in ["0", "1", "2"]:
                await client.write_gatt_char(
                    CHARACTERISTIC_UUID,
                    command.encode()
                )

            else:
                print("Please enter 0, 1, 2, or q.")


asyncio.run(main())