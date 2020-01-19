import serial

PORT='/dev/ttyACM0'
OUTFILE='out.csv'
NUM_SAMPLES = 1000

f=open(OUTFILE,"w+")

s=serial.Serial(PORT,115200,timeout=1)


SIZE = 1000
NUM = 1
RES = 12
RATE = 10
SAMP_SPEED = 4
CONV_SPEED = 4

s.write('ADC.SET_SIZE '+str(SIZE)+'\n\r') # number of samples to grab in one go
#s.write('ADC.SET_NUM '+str(NUM)+'\n\r') # number of times to repeat the sample grab
#s.write('ADC.SET_RES '+str(RES)+'\n\r') # resolution
#s.write('ADC.SET_CONV_SPEED '+str(CONV_SPEED)+'\n\r')
#s.write('ADC.SET_SAMP_SPEED '+str(SAMP_SPEED)+'\n\r')
#s.write('ADC.SET_RATE '+str(RATE)+'\n\r') 

s.write('ADC.START\n\r')

i=0
go=True

while go==True:
    line=s.readline()
    vals=line.split(',')
    for v in vals:
        print(v)
    #print(val)
    #f.write(line)
    #i=i+1
    #print("yeah")
    #print(len(vals))
    if len(vals)>1:
        go=False

f.close()

#s.write('ADC.STOP\n\r')
