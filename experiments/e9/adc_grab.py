import serial

PORT='/dev/ttyACM0'

chan0_outfile='out0.csv'
chan1_outfile='out1.csv'

f=open(chan0_outfile,"w+")
g=open(chan1_outfile,"w+")

s=serial.Serial(PORT,115200,timeout=1)

BUFFER = 100
NUM = 1
RES = 10
RATE = 10
SAMP_SPEED = 4
CONV_SPEED = 4

#s.write('ADC.SET_SIZE '+str(SIZE)+'\n\r') # number of samples to grab in one go
#s.write('ADC.SET_NUM '+str(NUM)+'\n\r') # number of times to repeat the sample grab


s.write('ADC.SET_BUFFER ' + str(BUFFER)+'\n\r') #number of samples to grab in the buffer
s.write('ADC.SET_RES '+str(RES)+'\n\r') # resolution
s.write('ADC.SET_CONV_SPEED '+str(CONV_SPEED)+'\n\r')
#s.write('ADC.SET_SAMP_SPEED '+str(SAMP_SPEED)+'\n\r')
#s.write('ADC.SET_RATE '+str(RATE)+'\n\r') 

s.write('ADC.START\n\r')

i=0
go=True

while go==True:
    line=s.readline()
    p=line.split(':')
    if len(p)>1:
        #print(p)
        C0=p[3]
        C1=p[4]
        C0_vals=C0.split(',')
        C1_vals=C1.split(',')
        for v in C0_vals:
            print(v)
            f.write(v)
            f.write('\n')
        for v in C1_vals:
            print(v)
            g.write(v)
            g.write('\n')
        go=False

f.close()

#s.write('ADC.STOP\n\r')
