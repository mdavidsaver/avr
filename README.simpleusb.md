= Linux Setup

```bash
cat << EOF > /etc/udev/rules/50-simpleusb.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="1234", ATTR{idProduct}=="1234", GROUP="plugdev"
EOF
udevadm control --reload
```

Disconnect and reconnect the device.

If the preceeding doesn't work check that the attriutes are correct for your udev version.
Find the bus and device numbers with 'lsusb'.

```bash
udevadm info -a -p $(udevadm info -q path -n /dev/bus/usb/005/094)
udevadm test $(udevadm info -q path -n /dev/bus/usb/005/094)
```

= Testing

```bash
make load-simpleusb-ukey
```

Then run

```bash
python simpleusb-client.py 0x1257
```
