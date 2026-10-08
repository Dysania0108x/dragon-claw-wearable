"""
vibration_controller.py

Bluetooth controller for the Dragon Claw Challenge wearable.


This module connects the Raspberry Pi to the ESP32-S3
and sends vibration commands for different weather events.

Commands:
    W = Wind
    R = Rain
    T = Thunder
    S = Stop

The actual vibration intensity, timing, and patterns
are controlled by the ESP32 Arduino program.
"""

from bleak import BleakScanner, BleakClient


# ==================================================
# BLE Configuration
# ==================================================

DEVICE_NAME = "Museum-ESP32"

CHARACTERISTIC_UUID = "87654321-4321-4321-4321-cba987654321"


# ==================================================
# BLE Commands
# ==================================================

WIND_COMMAND = "W"
RAIN_COMMAND = "R"
THUNDER_COMMAND = "T"
STOP_COMMAND = "S"


# ==================================================
# Vibration Controller
# ==================================================

class VibrationController:

    def __init__(self):
        self.client = None


    # --------------------------------------------------
    # Connect
    # --------------------------------------------------

    async def connect(self):
        """
        Find and connect to the ESP32-S3 via BLE.

        Returns:
            bool:
                True  = connection successful
                False = ESP32 not found
        """

        print("Searching for ESP32...")

        device = await BleakScanner.find_device_by_name(
            DEVICE_NAME,
            timeout=10
        )

        if device is None:
            print("ESP32 not found.")
            return False

        print("ESP32 found. Connecting...")

        self.client = BleakClient(device)

        await self.client.connect()

        print("ESP32 connected.")

        return True


    # --------------------------------------------------
    # Send BLE Command
    # --------------------------------------------------

    async def send_command(self, command):
        """
        Send a BLE command to the ESP32-S3.
        """

        if self.client is None:
            print("ESP32 is not connected.")
            return

        if not self.client.is_connected:
            print("ESP32 connection lost.")
            return

        await self.client.write_gatt_char(
            CHARACTERISTIC_UUID,
            command.encode()
        )


    # --------------------------------------------------
    # Weather Vibration Commands
    # --------------------------------------------------

    async def wind(self):
        """
        Trigger the Wind vibration pattern.
        """
        print("Vibration -> Wind")

        await self.send_command(WIND_COMMAND)


    async def rain(self):
        """
        Trigger the Rain vibration pattern.
        """
        print("Vibration -> Rain")

        await self.send_command(RAIN_COMMAND)


    async def thunder(self):
        """
        Trigger the Thunder vibration pattern.
        """
        print("Vibration -> Thunder")

        await self.send_command(THUNDER_COMMAND)


    # --------------------------------------------------
    # Stop
    # --------------------------------------------------

    async def stop(self):
        """
        Stop the current vibration pattern.
        """
        print("Vibration -> Stop")

        await self.send_command(STOP_COMMAND)


    # --------------------------------------------------
    # Disconnect
    # --------------------------------------------------

    async def disconnect(self):
        """
        Stop vibration and disconnect from the ESP32-S3.
        """

        if self.client is not None and self.client.is_connected:

            await self.stop()

            await self.client.disconnect()

            print("ESP32 disconnected.")

        self.client = None