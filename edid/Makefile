
get_edid: get_edid_rev_a3

write_edid: write_edid_rev_a3

# Make sure your display is connected from a normal (Ubuntu) PC. 
# Fist run "python edid.py" to make edid.dat
# You might have to change the port from 0 to something else
get_edid_rev_a3:
	sudo ./edid-rw/edid-rw 0 | parse-edid

write_edid_rev_a3:
	sudo ./edid-rw/edid-rw 0 -w -s 0.1 < edid.dat



update_rev_a1: 
	dtc -O dtb -o BB-EEPROM-01-00A0.dtbo -b 0 -@ BB-EEPROM-01-00A0.dts
	mv BB-EEPROM-01-00A0.dtbo /lib/firmware/
	echo BB-EEPROM-01 > /sys/devices/bone_capemgr.*/slots
	python edid.py
	cat edid.dat >  /sys/bus/i2c/drivers/at24/1-0050/eeprom
