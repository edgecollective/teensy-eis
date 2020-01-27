import serial
from collections import OrderedDict
import numpy as np

#import IPython;IPython.embed() # USE ME FOR DEBUGGING

OUTFILE_NAME = 'out.csv'
OUTFILE_COLS = ['X_micros','V0','V1']
OUTFILE_FMT  = '%0.3f,%i,%i'

PORT = '/dev/ttyACM0'
BAUD = 115200
BUFFER = 1000
NUM = 1
RES = 10
RATE = 10
CONV_SPEED = 4
SAMP_SPEED = 0

class ADC_Driver:
    def __init__(self, port=PORT):
        self.ser = serial.Serial(PORT,BAUD,timeout=1)
    
    def send(self, cmd, endl='\n'):
        self.ser.write(bytes(cmd+endl,'utf8'))
        
    def config(self,
               buffer_size = BUFFER,
               num_groups  = NUM,
               group_rate  = RATE,
               resolution = RES,
               conv_speed = CONV_SPEED,
               samp_speed = SAMP_SPEED,
              ):
        self.send(f"ADC.SET_BUFFER {buffer_size}")
        self.send(f"ADC.SET_RES {resolution}")
        self.send(f"ADC.SET_CONV_SPEED {conv_speed}")
        self.send(f"ADC.SET_SAMP_SPEED {samp_speed}")
        self.send(f"ADC.SET_NUM  {num_groups}")
        self.send(f"ADC.SET_RATE {group_rate}")
        
    def acquire_groups(self, num=1, **kwargs):
        kwargs['num_groups'] = int(num)
        self.config(**kwargs)
        self.send("ADC.START")
        groups = []
        for g in range(num):
            line = self.ser.readline()
            print(line)
            chan, index, t_start, V0, V1, t_dur = line.split(b':')
            V0 = np.array(list(map(int,V0.split(b','))))
            V1 = np.array(list(map(int,V1.split(b','))))
            X_micros  = float(t_dur)*np.arange(len(V0))/len(V0)
            data = OrderedDict()
            data['chan']     = chan
            data['t_start']  = int(t_start)
            data['X_micros'] = X_micros
            data['V0']       = V0
            data['V1']       = V1
            groups.append(data)
        return groups

if __name__ == "__main__":
    import matplotlib.pyplot as plt
    ADC = ADC_Driver()
    ADC.send("DDS.SET_OUT_PIN 13")
    ADC.send("DDS.START") #this should cause built in LED to pulse at 1Hz
    #the following acquistion should take ~10 seconds
    groups = ADC.acquire_groups(100, group_rate=10,buffer_size=1)
    ts = np.array([g['t_start'] for g in groups])
    ts = (ts - ts[0])/1e6
    V0 = np.array([g['V0'] for g in groups])
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.plot(ts,V0,'.-')
