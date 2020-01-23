import serial
import numpy as np


PORT='/dev/ttyACM0'

outfile='out.csv'

f=open(outfile,"w+")

s=serial.Serial(PORT,115200,timeout=1)

BUFFER = 1000
NUM = 1
RES = 10
RATE = 10
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
        chan, index, t_start, V0, V1, t_dur = line.split(':')
        V0 = np.array(map(int,V0.split(',')))
        V1 = np.array(map(int,V1.split(',')))
        X = float(t_dur)*np.arange(len(V0))/len(V0)

        i=0
        while (i<len(V0)):
            #print(X[i],V0[i],V1[i])
            print("%3d, %3d, %3d" %(X[i],V0[i],V1[i])) 
            f.write("%3d, %3d, %3d\n" %(X[i],V0[i],V1[i]))
            i=i+1
            #print(V0,V1)
    
        go=False

#f.close()

#s.write('ADC.STOP\n\r')
