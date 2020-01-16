import serial

PORT='/dev/ttyACM1'
OUTFILE='out.csv'
NUM_SAMPLES = 1000

f=open(OUTFILE,"w+")

s=serial.Serial(PORT,115200,timeout=1)

#s.write('SAMPLE '+str(NUM_SAMPLES)+'\n\r')
s.write('ADC.START\n\r')

i=0
#while (i<NUM_SAMPLES):
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

