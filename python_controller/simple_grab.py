import serial

PORT='/dev/ttyACM1'
OUTFILE='out.csv'
NUM_SAMPLES = 1000

f=open(OUTFILE,"w+")

s=serial.Serial(PORT,9600,timeout=1)

s.write('SAMPLE '+str(NUM_SAMPLES)+'\n\r')

i=0

while (i<NUM_SAMPLES):
    val=s.readline().strip()
    #print(val)
    f.write(val+'\n')
    i=i+1

f.close()

