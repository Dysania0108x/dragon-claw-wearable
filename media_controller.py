from pathlib import Path
import subprocess


# --------------------------------------------------
# Media folder
# --------------------------------------------------

MEDIA_FOLDER = Path(__file__).parent / "media"


# --------------------------------------------------
# Media mapping
# --------------------------------------------------

MEDIA_MAP = {
    "wind": MEDIA_FOLDER / "wind.mp4",
    "rain": MEDIA_FOLDER / "rain.mp4",
    "thunder": MEDIA_FOLDER / "thunder.mp4",
    "claw_3": MEDIA_FOLDER / "claw_3.mp4",
    "claw_4": MEDIA_FOLDER / "claw_4.mp4",
    "claw_5": MEDIA_FOLDER / "claw_5.mp4"
}


# --------------------------------------------------
# Current media process
# --------------------------------------------------

current_process = None


# --------------------------------------------------
# Stop current media
# --------------------------------------------------

def stop_media():
    global current_process

    if current_process is not None:
        current_process.terminate()
        current_process = None


# --------------------------------------------------
# Play media
# --------------------------------------------------

def play_media(media_name):
    global current_process

    # Check whether the requested media exists in the mapping
    if media_name not in MEDIA_MAP:
        print(f"Unknown media: {media_name}")
        return

    media_file = MEDIA_MAP[media_name]

    # Check whether the MP4 file actually exists
    if not media_file.exists():
        print(f"Media file not found: {media_file}")
        return

    # Stop the previous animation
    stop_media()

    print(f"Playing: {media_file}")

    # Play MP4 fullscreen on the HDMI display
    current_process = subprocess.Popen([
        "vlc",
        "--fullscreen",
        "--play-and-exit",
        str(media_file)
    ])