EESchema Schematic File Version 4
LIBS:cadenza_rev_a_smoke-cache
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Cadenza Rev A JLCEDA Import Smoke Test"
Date "2026-07-20"
Rev "D0"
Comp "CadenzaOS"
Comment1 "Two nets, two connectors, self-contained legacy library"
$EndDescr
$Comp
L Smoke_Connector J1
U 1 1 5F000301
P 3500 3300
F 0 "J1" H 3580 3292 50  0000 L CNN
F 1 "SMOKE_IN" H 3580 3201 50 0000 L CNN
F 2 "Smoke:Conn_01x02" H 3500 3300 50 0001 C CNN
	1    3500 3300
	1 0 0 -1
$EndComp
$Comp
L Smoke_Connector J2
U 1 1 5F000302
P 6500 3300
F 0 "J2" H 6580 3292 50 0000 L CNN
F 1 "SMOKE_OUT" H 6580 3201 50 0000 L CNN
F 2 "Smoke:Conn_01x02" H 6500 3300 50 0001 C CNN
	1    6500 3300
	-1 0 0 -1
$EndComp
Wire Wire Line
	3250 3250 3000 3250
Text Label 3000 3250 2 50 ~ 0
TEST_SIG
Wire Wire Line
	3250 3350 3000 3350
Text Label 3000 3350 2 50 ~ 0
GND
Wire Wire Line
	6750 3250 7000 3250
Text Label 7000 3250 0 50 ~ 0
TEST_SIG
Wire Wire Line
	6750 3350 7000 3350
Text Label 7000 3350 0 50 ~ 0
GND
$EndSCHEMATC
