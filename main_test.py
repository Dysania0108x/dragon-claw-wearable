import asyncio
from xlot_vib_controller import VibrationController
#Using to test xlot vibration modes

async def main():

    vibration = VibrationController()

    print("Connecting to XLOT-Hermes...")

    connected = await vibration.connect()

    if not connected:
        print("Connection failed!")
        return

    print("\nXLOT-Hermes connected successfully!")

    try:
        while True:

            print("\n====================")
            print("Vibration Test")
            print("====================")
            print("W - Wind")
            print("R - Rain")
            print("T - Thunder")
            print("S - Stop")
            print("Q - Quit")

            command = (
                await asyncio.to_thread(input, "\nEnter command: ")
            ).strip().upper()

            if command == "W":
                await vibration.wind()

            elif command == "R":
                await vibration.rain()

            elif command == "T":
                await vibration.thunder()

            elif command == "S":
                await vibration.stop()

            elif command == "Q":
                print("Exiting...")
                break

            else:
                print("Invalid command!")

    finally:
        await vibration.disconnect()


if __name__ == "__main__":
    asyncio.run(main())