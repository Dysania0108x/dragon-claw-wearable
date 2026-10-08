"""
gesture_detection.py

Gesture recognition interface for Dragon Claw Challenge.

This module is responsible for:
1. Receiving the camera feed.
2. Detecting hand gestures using MediaPipe.
3. Returning a gesture parameter from 0 to 5.

Gesture Mapping:
    0 = Fist
    1 = Wrist Move
    2 = Open Palm
    3 = Three Fingers
    4 = Four Fingers
    5 = Five Fingers
"""


# Gesture IDs
FIST = 0
WRIST_MOVE = 1
OPEN_PALM = 2
THREE_FINGERS = 3
FOUR_FINGERS = 4
FIVE_FINGERS = 5


def get_gesture():
    """
    Detect the current hand gesture.

    Returns:
        int | None:
            0 = Fist
            1 = Wrist Move
            2 = Open Palm
            3 = Three Fingers
            4 = Four Fingers
            5 = Five Fingers
            None = No valid gesture detected
    """

    # TODO:
    # Add Camera + MediaPipe gesture recognition here.

    return None