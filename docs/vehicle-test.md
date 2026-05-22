
# Vehicle Test

## Tested Vehicle

- Volvo XC60
- Model year: 2012
- TPMS frequency: 433.92 MHz

## Test Result

The project has been tested successfully on a real vehicle.

Verified items:

- CC1101 signal reception
- rtl_433_ESP decoder registration
- Abarth 124 Spider TPMS decoder compatibility
- Four TPMS sensor IDs detected
- Pressure values displayed
- Temperature values displayed
- Cached values displayed after reboot

## Example Values

Example values observed during testing:

| Wheel | Pressure | Temperature |
|---|---:|---:|
| LF | 2.37 bar | 17°C |
| RF | 2.73 bar | 16°C |
| LR | 2.70 bar | 17°C |
| RR | 2.50 bar | 17°C |

## Startup Behavior

After power-on, the device may not receive new TPMS packets immediately.

If cached values are available, the display shows the last valid pressure and temperature values with `WT` status.

When new TPMS packets are received, the corresponding wheel status changes to `OK`.

If no recent update is received for a configured period, the status changes to `OD`.