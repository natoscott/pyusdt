import time

def nap():
    print("sleeping");
    time.sleep(1)

while True:
    try:
        nap()
    except KeyboardInterrupt:
        break
