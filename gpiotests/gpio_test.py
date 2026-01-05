import pigpio

count = 0

def sig(a, b, c):
    global count
    count += 1
    print(count, a, b, c)

pi = pigpio.pi()

pi.callback(11, func=sig)

while True: pass
