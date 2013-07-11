''' This was taken from the Synaptics_3K driver in Linux, 
copyright HTC and lisenced under GPL_v2. 

Also, thanks to Adafruit for the I2C lib : ) '''

from Adafruit_I2C import Adafruit_I2C 
from threading import Thread
import time

ADDR = 0x20 # Address of chip on I2C bus

touch = Adafruit_I2C(0x20, 1, False) # Open device

page_table = [{"addr": 0, "value": 0}]*(0xEE-0xDD) # Table with stuff


def synaptics_init_panel(sensitivity_adjust):

    ret = None
    if sensitivity_adjust:
        ret = touch.write8(page_table[2]["value"] + 0x48, sensitivity_adjust); # Set Sensitivity
        if ret != None:
            print "[TP] TOUCH_ERR: touch.write8 failed for Sensitivity Set: "+str(ret)
            return ret

    # Position Threshold 
    touch.write8(page_table[2]["value"]+2, 3)
    touch.write8(page_table[2]["value"]+3, 3)

    # 2D Gesture Enable 
    touch.write8(page_table[2]["value"]+10, 0)
    touch.write8(page_table[2]["value"]+11, 0)

    # Configured 
    touch.write8(page_table[8]["value"], 0x80)

    return ret


def synaptics_ts_work_func():
    finger_support = 1
    buf_len = ((finger_support * 21 + 11) / 4)
    i = 100
    while i > 0: 
        touch.write8(ADDR, page_table[9]["value"])
        buf = touch.readList(ADDR, buf_len)
        if (buf[0] & 0x0F) != 0: 
            print "Error"
        else:
            print buf                     
        i -= 1
        time.sleep(0.1)

def probe(): 

    for i in range(10):
        r = touch.readU8(0xDD)  	
        if r == 0x96:
            print "[TP] found Synaptic chip"
            break

    j = 0
    for i, addr in enumerate(range(0xDD, 0xEE)):
        page_table[i] = {"addr": addr, "value": touch.readU8(addr)}
        while page_table[i]["value"] != touch.readU8(addr):
            page_table[i]["value"] = touch.readU8(addr)
        j += 1

    panel_version = touch.readU8(page_table[6]["value"] + 3) | (touch.readU8(page_table[6]["value"] + 2) << 8)

    max_x = touch.readU8(page_table[2]["value"]+6) | (touch.readU8(page_table[2]["value"]+7) << 8)
    max_y = touch.readU8(page_table[2]["value"]+8) | (touch.readU8(page_table[2]["value"]+9) << 8)

    abs_x_min = 0;
    abs_x_max = max_x;
    abs_y_min = 0;
    abs_y_max = max_y;

    display_height = 1
    raw_ref = 115 * ((abs_y_max - abs_y_min) + abs_y_min) / display_height
    raw_base = 650 * ((abs_y_max - abs_y_min) + abs_y_min) / display_height

    print "[TP] raw_ref: %d, raw_base: %d" % (raw_ref, raw_base)

    ret = synaptics_init_panel(1);

    timestamp = time.time()

    if ret != None: 
        print "[TP] TOUCH_ERR: synaptics_init_panel failed"
        exit(0)

    syn_wq = Thread(target=synaptics_ts_work_func)
    syn_wq.start()		

    print "[TP] synaptics_ts_probe: max_x %d, max_y %d"%( max_x, max_y ) 
    print "[TP] input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d"%(abs_x_min, abs_x_max, abs_y_min, abs_y_max)




probe()
