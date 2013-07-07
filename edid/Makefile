


all: 
	dtc -O dtb -o BB-EEPROM-01-00A0.dtbo -b 0 -@ BB-EEPROM-01-00A0.dts
	mv BB-EEPROM-01-00A0.dtbo /lib/firmware/
	echo BB-EEPROM-01 > /sys/devices/bone_capemgr.*/slots
	python edid.py
	cat edid.dat >  /sys/bus/i2c/drivers/at24/1-0050/eeprom
