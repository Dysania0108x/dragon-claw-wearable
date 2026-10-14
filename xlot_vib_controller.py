import asyncio
import urllib.request


class VibrationController:

    def __init__(self, device_ip="192.168.4.1"):

        self.base_url = f"http://{device_ip}"

        self.connected = False

        self.heartbeat_task = None

    # ==========================================
    # HTTP Request
    # ==========================================

    def _request(self, endpoint):

        url = self.base_url + endpoint

        try:

            with urllib.request.urlopen(
                url,
                timeout=3
            ) as response:

                return response.read().decode()

        except Exception as e:

            print(f"XLOT communication error: {e}")

            return None

    # ==========================================
    # Connect
    # ==========================================

    async def connect(self):

        print("Connecting to XLOT-Hermes...")

        result = await asyncio.to_thread(
            self._request,
            "/status"
        )

        if result is None:

            print("XLOT-Hermes not found.")

            self.connected = False

            return False

        self.connected = True

        print("XLOT-Hermes connected.")

        return True

    # ==========================================
    # Heartbeat
    # ==========================================

    async def _heartbeat_loop(self):

        try:

            while self.connected:

                result = await asyncio.to_thread(
                    self._request,
                    "/heartbeat"
                )

                if result is None:
                    print("Heartbeat failed.")
                    self.connected = False
                    break

                await asyncio.sleep(2)

        except asyncio.CancelledError:
            raise

    async def _start_heartbeat(self):

        if (
            self.heartbeat_task is None
            or self.heartbeat_task.done()
        ):

            self.heartbeat_task = asyncio.create_task(
                self._heartbeat_loop()
            )

    async def _stop_heartbeat(self):

        if self.heartbeat_task is not None:

            self.heartbeat_task.cancel()

            try:
                await self.heartbeat_task
            except asyncio.CancelledError:
                pass

            self.heartbeat_task = None

    # ==========================================
    # Send Command
    # ==========================================

    async def send_command(self, command):

        if not self.connected:

            print("XLOT-Hermes not connected.")

            return False

        result = await asyncio.to_thread(
            self._request,
            f"/command?mode={command}"
        )

        if result is None:

            return False

        print(result)

        if command != "S":
            await self._start_heartbeat()
        else:
            await self._stop_heartbeat()

        return True

    # ==========================================
    # Weather Modes
    # ==========================================

    async def wind(self):

        return await self.send_command("W")

    async def rain(self):

        return await self.send_command("R")

    async def thunder(self):

        return await self.send_command("T")

    # ==========================================
    # Stop
    # ==========================================

    async def stop(self):

        return await self.send_command("S")

    # ==========================================
    # Disconnect
    # ==========================================

    async def disconnect(self):

        if self.connected:
            await self.stop()

        await self._stop_heartbeat()

        self.connected = False

        print("XLOT-Hermes controller closed.")