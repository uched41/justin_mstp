import os
import subprocess
import json
import time, random
import RPi.GPIO as GPIO

c_binary_location = "./demo/server/bacserv"
update_options = ["Temperature", "Humidity", "Pressure", "Flow", "Analog", "Particles"]
bacnet_proc = None

def start_bacnet():
	GPIO.setwarnings(False)
	GPIO.setmode(GPIO.BOARD)
	GPIO.setup(7, GPIO.OUT, initial=GPIO.HIGH)
	
	global bacnet_proc
	try:
		bacnet_proc = subprocess.Popen(
			c_binary_location, 
			bufsize=0,
			stdin=subprocess.PIPE, 
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT
		)
	except Exception as e:
		print(e)
		return False

	return True


def stop_bacnet():
	global bacnet_proc
	if bacnet_proc is None:
		return False

	bacnet_proc.terminate()
	return True


def update_value(sensor, value):
	global bacnet_proc
	global update_options

	if sensor not in update_options:
		return False

	ind = update_options.index(sensor)
	msg = "{}:{}".format(ind, value)
	bacnet_proc.stdin.write(msg.encode())
	print(msg)
	return True


if __name__ == "__main__":
	print("Starting BACnet ")
	start_bacnet()
	
	while True:
		try:
			update_value("Temperature", round(random.random()*100, 2))
			time.sleep(2)
		except KeyboardInterrupt:
			print("Stopping")
			stop_bacnet()
			raise
	print("End")
