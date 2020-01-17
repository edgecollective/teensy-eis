import serial

PORT='/dev/ttyACM0'
OUTFILE='out.csv'

f=open(OUTFILE,"w+")
#f=open(OUTFILE,"a+")

s=serial.Serial(PORT,115200,timeout=1)

SIZE = 10
NUM = 10
RES = 12
RATE = 100

# SET UP THE SAMPLING

s.write('ADC.SET_SIZE '+str(SIZE)+'\n\r') # number of samples to grab in one go
s.write('ADC.SET_NUM '+str(NUM)+'\n\r') # number of times to repeat the sample grab
s.write('ADC.SET_RES '+str(RES)+'\n\r') # resolution
s.write('ADC.SET_RATE '+str(RATE)+'\n\r') # resolution

s.write('ADC.START\n\r')

print("SIZE=",SIZE)
print("NUM=",NUM)
print("RES=",RES)
print("RATE=",RATE)

i=0
go=True

while go==True:
    line=s.readline()
    p=line.split(':')
    if (len(p) > 1):
        #go = False
        index=p[0].strip()
        start=p[1].strip()
        delta=p[3].strip()
        print("micros_start:"+str(start),"micros_delta:"+str(delta))
        print('uS total:'+str(float(delta)/float(SIZE)))
        vals = p[2].split(',')
        print(vals)
        for v in vals:
            f.write(v+'\n')


f.close()
