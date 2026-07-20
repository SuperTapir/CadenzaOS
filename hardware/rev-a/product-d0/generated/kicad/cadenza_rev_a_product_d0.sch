EESchema Schematic File Version 4
LIBS:power
LIBS:device
LIBS:Connector_Generic
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Cadenza Rev A Product D0"
Date "2026-07-20"
Rev "D0 - DO NOT FABRICATE"
Comp "CadenzaOS"
Comment1 "System architecture and interface contract"
$EndDescr
$Comp
L Connector_Generic:Conn_01x02 J1
U 1 1 60000001
P 1800 1700
F 0 "J1" H 1880 1917 50  0000 C CNN
F 1 "POWER: USB-C -> BQ24074 -> LiPo; AP2112 3V3; TPS61023 5V" H 1880 1826 50  0000 C CNN
	1    1800 1700
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J2
U 1 1 60000002
P 4700 1700
F 0 "J2" H 4780 1917 50  0000 C CNN
F 1 "MCU: ESP32-S3-WROOM-1-N16R8" H 4780 1826 50  0000 C CNN
	1    4700 1700
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J3
U 1 1 60000003
P 7700 1700
F 0 "J3" H 7780 1917 50  0000 C CNN
F 1 "DISPLAY: LS027B7DH01 / 10-pin FPC / 5V + 3V3 logic" H 7780 1826 50  0000 C CNN
	1    7700 1700
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J4
U 1 1 60000004
P 1800 3800
F 0 "J4" H 1880 4017 50  0000 C CNN
F 1 "INPUT: Menu, B, encoder A/B and centre A" H 1880 3926 50  0000 C CNN
	1    1800 3800
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J5
U 1 1 60000005
P 4700 3800
F 0 "J5" H 4780 4017 50  0000 C CNN
F 1 "AUDIO: IM69D130 + MAX98357A + 8ohm speaker" H 4780 3926 50  0000 C CNN
	1    4700 3800
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J6
U 1 1 60000006
P 7700 3800
F 0 "J6" H 7780 4017 50  0000 C CNN
F 1 "STORAGE: microSD + 16MB module flash" H 7780 3926 50  0000 C CNN
	1    7700 3800
	1 0 0 -1
$EndComp
Text Notes 1200 1250 0    60   ~ 12
POWER
Text Notes 4100 1250 0    60   ~ 12
MCU
Text Notes 7100 1250 0    60   ~ 12
DISPLAY
Text Notes 1200 3350 0    60   ~ 12
INPUT
Text Notes 4100 3350 0    60   ~ 12
AUDIO
Text Notes 7100 3350 0    60   ~ 12
STORAGE
Text Notes 1200 5100 0    45   ~ 0
LCD: SCLK SI SCS EXTCOMIN DISP / 5V VDD+VDDA / VSS+VSSA
Text Notes 1200 5280 0    45   ~ 0
INPUT: ENC_A GPIO4, ENC_B GPIO5, A_SW GPIO0, B GPIO47, MENU GPIO48
Text Notes 1200 5460 0    45   ~ 0
AUDIO: MIC GPIO39/42; AMP GPIO46/40/7; differential SPK+/-
Text Notes 1200 5640 0    45   ~ 0
USB: native D-/D+ GPIO19/20; ESD and 5.1k CC resistors required
Text Notes 1200 5820 0    45   ~ 0
D0 uses interface blocks; production symbols/footprints require release-gate review
$EndSCHEMATC
