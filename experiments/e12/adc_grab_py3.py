import serial
from collections import OrderedDict
import numpy as np

#import IPython;IPython.embed() # USE ME FOR DEBUGGING

OUTFILE_NAME = 'dummy_out.csv'
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
    ADC = ADC_Driver()
    group = ADC.acquire_groups(num=1)[0]
    D = np.array([group[key] for key in OUTFILE_COLS]).transpose()
    np.savetxt(OUTFILE_NAME,D,fmt=OUTFILE_FMT)

