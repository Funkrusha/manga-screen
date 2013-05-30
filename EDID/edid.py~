#!/usr/bin/python

# Extended display identification data

import struct

# 0-19
man_id      = "IAG" # Manufacturer ID, three letters
prod_code   = 1     # Manufacturer product code. 16-bit number, little-endian.
serial_nr   = 1     # Serial number. 32 bits, little endian.
week_of_man = 25    # Week of manufacture, 8 bits. 
year_of_man = 2013  # Year of manufacture
edid_ver    = 1     # Version
edid_rev    = 3     # Revision

# 20-24
dig_input   = 1     # Digital input, if set the following apply
vesa_compat = 1     # VESA DFP 1.x compatible 
# dig_input = 0     # Analog input. If clear, the following bit definitions apply:
video_white = 0     # Video white and sync levels, relative to blank:
blank_black = 0     # Blank-to-black setup (pedestal) expected
sep_sync    = 0     # Separate sync supported
comp_sync   = 0     # Composite sync (on HSync) supported
sync_on_grn = 0     # Sync on green supported
vsync_serr  = 0     # VSync pulse must be serrated when composite or sync-on-green is used.

max_hor_img = 10    # Maximum horizontal image size, in centimetres (max 292 cm/115 in at 16:9 aspect ratio)
max_vert_img= 6     # Maximum vertical image size, in centimetres. If either byte is 0, undefined (e.g. projector)
disp_gamma  = 128   # Display gamma, datavalue  
DPMS_stnd   = 1     # DPMS standby supported
DPMS_susp   = 1     # DPMS suspend supported
DPMS_off    = 1     # DPMS active-off supported
disp_type   = 0     # Display type (digital): 00 = RGB 4:4:4; 01 = RGB 4:4:4 + YCrCb 4:4:4; 
                    # 10 = RGB 4:4:4 + YCrCb 4:2:2; 11 = RGB 4:4:4 + YCrCb 4:4:4 + YCrCb 4:2:2
disp_typ_an = 0     # Display type (analog): 00 = Monochrome or Grayscale; 01 = RGB color; 10 = Non-RGB color; 11 = Undefined
std_RGB     = 1     # Standard sRGB colour space.
pref_timing = 1     # Preferred timing mode specified in descriptor block 1.
GTF_sup     = 1     # GTF supported with default parameter values.

# 25-34 - Chromaticity coordinates.
red_x_lsb   = 0     # Red x value least-significant 2 bits
red_y_lsb   = 0     # Red y value least-significant 2 bits
grn_x_lsb   = 0     # Green x value least-significant 2 bits
grn_y_lsb   = 0     # Green y value least-significant 2 bits
blu_x_lsb   = 0     # Blue and white least-significant 2 bits
blu_y_lsb   = 0
wht_x_lsb   = 0
wht_y_lsb   = 0
red_x_msb   = 0     # Red x value most significant 8 bits.
red_y_msb   = 0     # Red y value most significant 8 bits
grn_x_msb   = 0     # Green x value most significant 8 bits
grn_y_msb   = 0     # Green x value most significant 8 bits
blu_x_msb   = 0     # Blue x value most significant 8 bits
blu_y_msb   = 0     # Blue y value most significant 8 bits
wht_x_msb   = 0     # Default white point x value most significant 8 bits
wht_y_msb   = 0     # Default white point y value most significant 8 bits

# 35-37 - Established timing bitmap. Supported bitmap for very common timing modes.
byte_35     = 0
byte_36     = 0
byte_37     = 0
# 38-53
x_res       = 100
pix_rat     = 3
v_freq      = 10

# Descriptor 1 
pix_clk     = 100
hor_act     = 10
hor_blank   = 10


def make_edid():
    # Bytes 0-19
    edid = struct.pack("BBBBBBBB", 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00) # Header
    edid += struct.pack("H", int("0b0"+"".join([bin(ord(l)-65)[2:].rjust(5, '0') for l in man_id]), 2))
    edid += struct.pack("<H", prod_code) 
    edid += struct.pack("<I", serial_nr) 
    edid += struct.pack("B", week_of_man) 
    edid += struct.pack("B", year_of_man-1990) 
    edid += struct.pack("B", edid_ver) 
    edid += struct.pack("B", edid_rev) 
    #Bytes 20-24 
    if dig_input == 1:
        edid += struct.pack("B", vesa_compat+128)
    else:
        s = "0b"+bin(video_white)[2:].rjust(3, '0')+str(blank_black)+str(sep_sync)+str(comp_sync)+str(sync_on_grn)+str(vsync_serr)
        edid += struct.pack("B", int(s))
    edid += struct.pack("B", max_hor_img) 
    edid += struct.pack("B", max_vert_img) 
    edid += struct.pack("B", disp_gamma) 
    s = "0b"+str(DPMS_stnd)+str(DPMS_susp)+str(DPMS_off)+bin(disp_type)[2:]+str(std_RGB)+str(pref_timing)+str(GTF_sup)
    edid += struct.pack("B", int(s, 2))
    # Bytes 25-34
    s = "0b"+bin(red_x_lsb)[2:]+bin(red_y_lsb)[2:]+bin(grn_x_lsb)[2:]+bin(grn_y_lsb)[2:]
    edid += struct.pack("B", int(s, 2))
    s = "0b"+bin(blu_x_lsb)[2:]+bin(blu_y_lsb)[2:]+bin(wht_x_lsb)[2:]+bin(wht_y_lsb)[2:]
    edid += struct.pack("B", int(s, 2))
    edid += struct.pack("B", red_x_msb)    
    edid += struct.pack("B", red_y_msb)    
    edid += struct.pack("B", grn_x_msb)    
    edid += struct.pack("B", grn_y_msb)    
    edid += struct.pack("B", blu_x_msb)    
    edid += struct.pack("B", blu_y_msb)    
    edid += struct.pack("B", wht_x_msb)    
    edid += struct.pack("B", wht_y_msb)    
    # Bytes 35-37 Established timing maps 
    edid += struct.pack("B", byte_35)    
    edid += struct.pack("B", byte_36)    
    edid += struct.pack("B", byte_37)    

    # Bytes 38-39 Standard timing
    edid += struct.pack("B", x_res)
    s = "0b"+bin(pix_rat)[2:].rjust(2, "0")+bin(v_freq)[2:].rjust(6, "0")
    edid += struct.pack("B", int(s, 2))

    # Descriptor 1
    edid += struct.pack("H", pix_clk)
    edid += struct.pack("B", hor_act)
    edid += struct.pack("B", hor_blank)

    return edid
    

print "Making edid"
f = open("edid.dat", "r+b")
edid = make_edid()
for i in range(126-39-4):
    edid += struct.pack("B", 0)
edid += struct.pack("B", 1)

checksum = 0
for i in range(127):
    checksum += struct.unpack("B", edid[i])[0]
print "checksum is "+str(checksum)
edid += struct.pack("B", 256-(checksum%256))

f.write(edid)

print "File done"
