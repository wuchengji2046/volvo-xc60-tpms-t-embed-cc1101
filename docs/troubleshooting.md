# Troubleshooting

## No display

Check:

- TFT pin configuration
- TFT_eSPI configuration
- Board power enable GPIO15
- Screen backlight GPIO21

## No RF reception

Check:

- CC1101 SPI pins
- CC1101 CS pin
- GDO0/GDO2 pin selection
- RF switch setting
- Antenna connection
- Frequency set to 433.92 MHz

## No TPMS data after boot

TPMS sensors may not transmit immediately after vehicle startup.

Try:

- Wait for several minutes
- Drive the vehicle for a short distance
- Test near each wheel
- Check serial monitor output

## WT status

`WT` means the display is waiting for new TPMS data after boot.

If cached values are available, they will be displayed with `WT`.

## OK status

`OK` means the wheel has received a valid TPMS packet in the current boot/session.

## OD status

`OD` means the data is old. No recent valid packet has been received for that wheel.

## Wrong wheel mapping

If pressure and temperature values appear under the wrong wheel, update the ID-to-wheel mapping in:

```text
include/xc60_tpms_config.h