import serial

PORT='/dev/ttyACM0'

s=serial.Serial(PORT,115200,timeout=1)

SIZE = 10
NUM = 1
RES = 12
RATE = 10
SAMP_SPEED = 4
CONV_SPEED = 4

# SET UP THE SAMPLING

s.write('ADC.SET_SIZE '+str(SIZE)+'\n\r') # number of samples to grab in one go
s.write('ADC.SET_NUM '+str(NUM)+'\n\r') # number of times to repeat the sample grab
s.write('ADC.SET_RES '+str(RES)+'\n\r') # resolution
s.write('ADC.SET_CONV_SPEED '+str(CONV_SPEED)+'\n\r')
s.write('ADC.SET_SAMP_SPEED '+str(SAMP_SPEED)+'\n\r')

#s.write('ADC.SET_RATE '+str(RATE)+'\n\r') # resolution

print("SIZE=",SIZE)
print("NUM=",NUM)
print("RES=",RES)
print("RATE=",RATE)
print("SAMP_SPEED=",SAMP_SPEED)
print("CONV_SPEED=",CONV_SPEED)

for i in range(0,10):
    s.write('ADC.START\n\r')
    print(s.readline())
    #s.write('ADC.STOP\n\r')

