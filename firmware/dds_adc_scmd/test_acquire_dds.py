import serial
from collections import OrderedDict
import numpy as np

#import IPython;IPython.embed() # USE ME FOR DEBUGGING

################################################################################
PORT = '/dev/ttyACM0'
BAUD = 115200
DEBUG = True

class ScmdComm:
    def __init__(self, port=PORT):
        self.ser = serial.Serial(PORT,BAUD,timeout=1)
    
    def send(self, cmd, endl='\n'):
        if DEBUG:
            print(f"-> {cmd}")
        self.ser.write(bytes(cmd+endl,'utf8'))

################################################################################
OUT_PIN  = 13
PWM_FREQ = 10000
PWM_RES  = 8

class DDS_Driver(ScmdComm):

    def write_table(self, values):
        table_len = len(values)
        self.send(f"DDS.FORMAT_TABLE {table_len}")
        for addr,v in enumerate(values):
            self.send(f"DDS.WRITE_TABLE {addr} {v:d}")
        
    def config(self,
               out_pin = OUT_PIN,
               pwm_freq = PWM_FREQ,
               pwm_res  = PWM_RES,
              ):
        self.send(f"DDS.SET_OUT_PIN {out_pin:d}")
        self.send(f"DDS.SET_PWM_FREQ {pwm_freq:f}")
        self.send(f"DDS.SET_PWM_RES {pwm_res:d}")
        
    def start(self, freq = None):
        if freq is not None:
            self.send(f"DDS.SET_FREQ {freq:f}")
        self.send(f"DDS.START")
        
    def stop(self):
        self.send(f"DDS.STOP")
    

################################################################################
BUFFER = 1000
NUM = 1
RES = 10
RATE = 10
CONV_SPEED = 4
SAMP_SPEED = 0

class ADC_Driver(ScmdComm):
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
################################################################################

if __name__ == "__main__":
    import matplotlib.pyplot as plt
    OUTFILE_NAME = 'out.csv'
    OUTFILE_COLS = ['X_micros','V0','V1']
    OUTFILE_FMT  = '%0.3f,%i,%i'
    
    ADC = ADC_Driver()
    DDS = DDS_Driver()

#    #---------------------------------------------------------------------------
#    DDS.config() #default configuration
#    DDS.start(freq=1) #LED should pulse at 1Hz
#    #the following acquistion should take ~10 seconds
#    groups = ADC.acquire_groups(1000, group_rate=100,buffer_size=1)
#    ts = np.array([g['t_start'] for g in groups])
#    ts = (ts - ts[0])/1e6
#    V0 = np.array([g['V0'] for g in groups])
#    fig = plt.figure()
#    ax = fig.add_subplot(111)
#    ax.plot(ts,V0,'.-', label='1 Hz sine')
#    #---------------------------------------------------------------------------
#    #double the frequency (output doesn't need to be stopped)
#    DDS.config() #default configuration
#    DDS.start(freq=2) #LED should pulse at double the rate
#    #the following acquistion should take ~5 seconds
#    groups = ADC.acquire_groups(1000, group_rate=200,buffer_size=1)
#    DDS.stop() #LED should stop pulsing
#    ts = np.array([g['t_start'] for g in groups])
#    ts = (ts - ts[0])/1e6
#    V0 = np.array([g['V0'] for g in groups])
#    ax.plot(ts,V0,'.-', label='2 Hz sine')
#    #---------------------------------------------------------------------------
    #construct a triangle wave, write the table, and sample
    DDS.config() #default configuration
    triangle_wave = np.concatenate([np.arange(127,255,1),np.arange(255,127,-1)])
    DDS.write_table(triangle_wave)
    DDS.start(freq=1)
    #the following acquistion should take ~10 seconds
    groups = ADC.acquire_groups(1000, group_rate=1000,buffer_size=1)
    DDS.stop()
    ts = np.array([g['t_start'] for g in groups])
    ts = (ts - ts[0])/1e6
    V0 = np.array([g['V0'] for g in groups])
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.plot(ts,V0,'.-', label='1 Hz triangle')
    ax.set_xlabel("Time [s]")
    ax.set_title("DDS to ADC test - RC filter")
    ax.legend()
    plt.show()
    
