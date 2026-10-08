import asyncio

from vibration_controller import VibrationController


async def main():

    vibration = VibrationController()

    connected = await vibration.connect()

    if not connected:
        return

    while True:

        command = input(
            "\nW = Wind\n"
            "R = Rain\n"
            "T = Thunder\n"
            "S = Stop\n"
            "Q = Quit\n"
            "> "
        ).upper()

        if command == "W":
            await vibration.wind()

        elif command == "R":
            await vibration.rain()

        elif command == "T":
            await vibration.thunder()

        elif command == "S":
            await vibration.stop()

        elif command == "Q":
            await vibration.disconnect()
            break


if __name__ == "__main__":
    asyncio.run(main())