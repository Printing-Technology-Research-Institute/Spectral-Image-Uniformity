
# -*- coding: utf-8 -*-
from time import sleep
import camera
import cv2

# Create device
cam = camera.Camera()

# List avaliable device
print(cam.get_device_list())

# Connect first
print("Connecting first device...")
cam.connect_first()

# Set exposure time (ms)
cam.set_exposure(5)
print(f"Current exposure time: {cam.get_exposure()} ms")

# Start stream
print("Start stream...")
cam.start()

# Open shutter
cam.shutter_on()

sleep(0.5)

window = cv2.namedWindow("Camera", cv2.WINDOW_NORMAL)

# # Get 5 image
# for i in range(5):
#     # Get raw data as numpy
#     data = cam.get_data()

#     print(data.shape)
#     assert data.shape == cam.get_shape()

while True:
    data = cam.get_data()
    display_img = cv2.normalize(data, None, 0, 65535, cv2.NORM_MINMAX, dtype=cv2.CV_16U)
    print(f"\r{data.max(), data.min()}", end="")
    cv2.imshow("Camera", display_img)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# Close shutter
cam.shutter_off()

# Stop stream
print("Stop stream...")
cam.stop()

# Close shutter and disconnect the device
print("Disconnecting...")
cam.disconnect()
