EESchema Schematic File Version 4
LIBS:cadenza-encoder-a
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Cadenza D1 Encoder + A Daughterboard"
Comment1 "Six-wire replaceable control module"
Comment2 "EC12D1524403 + RC debounce"
Comment3 "D1 WIP"
Comment4 "CadenzaOS"
$EndDescr
Text Notes 900 850 0 100 ~ 20
J1: 1=+3V3 2=GND 3=ENC_A 4=ENC_B 5=A_SW 6=LED_CTRL
Text Notes 900 1100 0 80 ~ 16
SW1 axial push is the front-panel A button. Encoder common and switch common return to GND.
Text Notes 900 1350 0 80 ~ 16
R1/C1, R2/C2, R3/C3: 10k pull-up and 10nF debounce. Firmware still performs state-machine debounce.
Text Notes 900 1600 0 80 ~ 16
LED_CTRL is reserved and intentionally no-connect on the D1 daughterboard.
$Comp
L Device:R R1
U 1 1 1010
P 4700 2600
F 0 "R1" H 4770 2646 50 0000 L CNN
F 1 "10k" H 4770 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 4630 2600 50 0001 C CNN
	1    4700 2600
	1 0 0 -1
$EndComp
$Comp
L Device:C C1
U 1 1 1011
P 4700 3200
F 0 "C1" H 4815 3246 50 0000 L CNN
F 1 "10n" H 4815 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4738 3050 50 0001 C CNN
	1    4700 3200
	1 0 0 -1
$EndComp
Text Label 4700 2350 1 50 ~ 0
+3V3
Text Label 4700 2900 1 50 ~ 0
ENC_A
Text Label 4700 3450 3 50 ~ 0
GND
Wire Wire Line
	4700 2350 4700 2450
Wire Wire Line
	4700 2750 4700 3050
Wire Wire Line
	4700 3350 4700 3450
$Comp
L Device:R R2
U 1 1 1020
P 5500 2600
F 0 "R2" H 5570 2646 50 0000 L CNN
F 1 "10k" H 5570 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 5430 2600 50 0001 C CNN
	1    5500 2600
	1 0 0 -1
$EndComp
$Comp
L Device:C C2
U 1 1 1021
P 5500 3200
F 0 "C2" H 5615 3246 50 0000 L CNN
F 1 "10n" H 5615 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 5538 3050 50 0001 C CNN
	1    5500 3200
	1 0 0 -1
$EndComp
Text Label 5500 2350 1 50 ~ 0
+3V3
Text Label 5500 2900 1 50 ~ 0
ENC_B
Text Label 5500 3450 3 50 ~ 0
GND
Wire Wire Line
	5500 2350 5500 2450
Wire Wire Line
	5500 2750 5500 3050
Wire Wire Line
	5500 3350 5500 3450
$Comp
L Device:R R3
U 1 1 1030
P 6300 2600
F 0 "R3" H 6370 2646 50 0000 L CNN
F 1 "10k" H 6370 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 6230 2600 50 0001 C CNN
	1    6300 2600
	1 0 0 -1
$EndComp
$Comp
L Device:C C3
U 1 1 1031
P 6300 3200
F 0 "C3" H 6415 3246 50 0000 L CNN
F 1 "10n" H 6415 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6338 3050 50 0001 C CNN
	1    6300 3200
	1 0 0 -1
$EndComp
Text Label 6300 2350 1 50 ~ 0
+3V3
Text Label 6300 2900 1 50 ~ 0
A_SW
Text Label 6300 3450 3 50 ~ 0
GND
Wire Wire Line
	6300 2350 6300 2450
Wire Wire Line
	6300 2750 6300 3050
Wire Wire Line
	6300 3350 6300 3450
$Comp
L Cadenza_Encoder:EC12D1524403 SW1
U 1 1 1001
P 3500 3300
F 0 "SW1" H 3500 3900 50  0000 C CNN
F 1 "EC12D1524403" H 3500 3800 50 0000 C CNN
F 2 "Cadenza_Encoder:ALPS_EC12D1524403" H 3500 3300 50 0001 C CNN
	1    3500 3300
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x06 J1
U 1 1 1002
P 7600 3300
F 0 "J1" H 7680 3292 50 0000 L CNN
F 1 "JST-SH-6" H 7680 3201 50 0000 L CNN
F 2 "Cadenza_Encoder:JST_SH_SM06B-SRSS-TB" H 7600 3300 50 0001 C CNN
	1    7600 3300
	1 0 0 -1
$EndComp
Text Label 3000 3100 2 50 ~ 0
ENC_A
Text Label 3000 3300 2 50 ~ 0
ENC_B
Text Label 3000 3500 2 50 ~ 0
GND
Text Label 4000 3200 0 50 ~ 0
A_SW
Text Label 4000 3400 0 50 ~ 0
GND
Wire Wire Line
	3000 3100 3200 3100
Wire Wire Line
	3000 3300 3200 3300
Wire Wire Line
	3000 3500 3200 3500
Wire Wire Line
	3800 3200 4000 3200
Wire Wire Line
	3800 3400 4000 3400
Text Label 7100 3100 2 50 ~ 0
+3V3
Text Label 7100 3200 2 50 ~ 0
GND
Text Label 7100 3300 2 50 ~ 0
ENC_A
Text Label 7100 3400 2 50 ~ 0
ENC_B
Text Label 7100 3500 2 50 ~ 0
A_SW
Text Label 7100 3600 2 50 ~ 0
LED_CTRL
Wire Wire Line
	7100 3100 7400 3100
Wire Wire Line
	7100 3200 7400 3200
Wire Wire Line
	7100 3300 7400 3300
Wire Wire Line
	7100 3400 7400 3400
Wire Wire Line
	7100 3500 7400 3500
Wire Wire Line
	7100 3600 7400 3600
$EndSCHEMATC
