import serial

PORT='/dev/ttyACM0'
OUTFILE='out.csv'

#f=open(OUTFILE,"w+")
#f=open(OUTFILE,"a+")

s=serial.Serial(PORT,115200,timeout=1)

SIZE = 10
NUM = 10
RES = 12
RATE = 5

# SET UP THE SAMPLING

s.write('ADC.SET_SIZE '+str(SIZE)+'\n\r') # number of samples to grab in one go
s.write('ADC.SET_NUM '+str(NUM)+'\n\r') # number of times to repeat the sample grab
s.write('ADC.SET_RES '+str(RES)+'\n\r') # resolution
s.write('ADC.SET_RATE '+str(RATE)+'\n\r') # resolution

print("SIZE=",SIZE)
print("NUM=",NUM)
print("RES=",RES)
print("RATE=",RATE)

for i in range(0,3):
    s.write('ADC.START\n\r')
    print(s.readline())
    s.write('ADC.STOP\n\r')

