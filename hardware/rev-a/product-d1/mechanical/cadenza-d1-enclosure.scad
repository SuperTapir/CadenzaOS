// Cadenza Rev A D1 enclosure draft.
// Units: mm. Coordinate origin: front-view lower-left of the product.
// This file is a mechanical-interface draft, not fabrication release data.

$fn = 64;

part = "assembly"; // front, rear, knob, assembly, exploded

// Product and process parameters.
body_w = 122;
body_h = 74;
front_depth = 7;
rear_depth = 14.5; // 1.2 mm clearance above tallest rear-side JST at inner back skin
wall = 2.0;
front_skin = 1.8;
corner_chamfer = 6;
fit_clearance = 0.25;
pcb_w = 116.0;
pcb_h = 64.0;
pcb_t = 1.6;
pcb_origin = [3.0, 5.0];
pcb_z = front_depth + 3.2;

// Main-board holes, translated from PCB local coordinates.
// PCB-local coordinates transformed to front-view (+Y upward).
main_holes = [[5.05,59.05], [43.55,56.55], [5.05,5.05], [111.05,5.05]];

// LS027B7DH01. The window is the active area plus a 0.6 mm reveal.
lcd_panel = [62.8, 42.82, 1.64];
lcd_active = [58.8, 35.28];
lcd_center = [61.0, 37.0];
lcd_window = [lcd_active[0] + 1.2, lcd_active[1] + 1.2];
lcd_pocket = [lcd_panel[0] + 0.4, lcd_panel[1] + 0.4];

// Front controls. Right encoder is the centre A control.
menu_center = [13.0, 53.0];
b_center = [13.0, 43.0];
button_d = 8.5;
encoder_center = [104.0, 37.0];
encoder_knob_od = 28.0;
encoder_aperture_d = encoder_knob_od + 1.2;
encoder_shaft_d = 6.0;
encoder_board = [34, 38, 1.6];

// Acoustic parts remain sample-gated. Update these values after measurement.
speaker_center = [13.0, 20.0];
speaker_frame_d = 24.0; // provisional: supported range 20..28
speaker_depth = 5.0;   // provisional
speaker_hole_d = 1.2;
mic_center = [11.0, 61.0];
mic_port_d = 1.0;

// Battery remains sample-gated. This envelope is a collision placeholder only.
battery_envelope = [65, 45, 6.5];
battery_center = [58, 33];

// ESP32-S3 antenna exclusion volume in enclosure front-view coordinates.
antenna_keepout_xy = [94.75, 51.75];
antenna_keepout_size = [24.30, 17.50];

// Edge interfaces. Final dimensions follow connector samples and PCB placement.
usb_center_x = 61.0;
usb_opening = [10.0, 4.0];
sd_center_y = 15.0;
sd_opening = [14.5, 2.8];
power_center_y = 27.0;
power_opening = [8.5, 3.5];

module chamfered_2d(w, h, c) {
    polygon([[c,0], [w-c,0], [w, c], [w,h-c],
             [w-c,h], [c,h], [0,h-c], [0,c]]);
}

module body(depth) {
    linear_extrude(height=depth) chamfered_2d(body_w, body_h, corner_chamfer);
}

module rounded_rect_2d(size, r=1) {
    offset(r=r) offset(delta=-r) square(size, center=true);
}

module front_apertures() {
    translate([lcd_center[0], lcd_center[1], -1])
        linear_extrude(front_skin + 2) rounded_rect_2d(lcd_window, 1.2);
    for (p = [menu_center, b_center])
        translate([p[0], p[1], -1]) cylinder(h=front_skin + 2, d=button_d);
    translate([encoder_center[0], encoder_center[1], -1])
        cylinder(h=front_skin + 2, d=encoder_aperture_d);
    translate([mic_center[0], mic_center[1], -1])
        cylinder(h=front_skin + 2, d=mic_port_d);
    // Seven-hole speaker grille; cavity and gasket remain separate behind it.
    for (a = [0:60:300])
        translate([speaker_center[0] + cos(a)*3.1,
                   speaker_center[1] + sin(a)*3.1, -1])
            cylinder(h=front_skin + 2, d=speaker_hole_d);
    translate([speaker_center[0], speaker_center[1], -1])
        cylinder(h=front_skin + 2, d=speaker_hole_d);
}

module lcd_supports() {
    support_x = lcd_pocket[0]/2 + 1.6;
    support_y = lcd_pocket[1]/2 + 1.6;
    for (sx = [-1,1], sy = [-1,1])
        translate([lcd_center[0] + sx*support_x,
                   lcd_center[1] + sy*support_y, front_skin])
            cylinder(h=1.2, d=3.0);
}

module encoder_bosses() {
    // Daughterboard datum: upper-left in front view; four M2 holes 3 mm in.
    db0 = [encoder_center[0]-17, encoder_center[1]-17];
    for (p = [[3,3], [31,3], [3,35], [31,35]])
        translate([db0[0]+p[0], db0[1]+p[1], front_skin])
            difference() {
                cylinder(h=3.0, d=5.0);
                cylinder(h=3.5, d=1.7);
            }
}

module front_shell() {
    difference() {
        body(front_depth);
        translate([wall, wall, front_skin])
            linear_extrude(front_depth-front_skin+1)
                chamfered_2d(body_w-2*wall, body_h-2*wall,
                             max(1,corner_chamfer-wall));
        front_apertures();
        // LCD panel pocket; leaves a continuous active-area bezel.
        translate([lcd_center[0], lcd_center[1], front_skin-0.01])
            linear_extrude(2.0) rounded_rect_2d(lcd_pocket, 0.8);
    }
    lcd_supports();
    encoder_bosses();
}

module pcb_standoff(p) {
    translate([pcb_origin[0]+p[0], pcb_origin[1]+p[1], front_depth])
        difference() {
            cylinder(h=3.2, d=5.0);
            cylinder(h=3.7, d=1.7);
        }
}

module rear_facets() {
    // Two sacrificial rails provide nominal 18 and 35 degree desk attitudes.
    // Their final height is tuned after centre-of-mass measurement.
    translate([16, 8, front_depth+rear_depth-0.1])
        rotate([0,18,0]) cube([3,58,3], center=false);
    translate([101, 8, front_depth+rear_depth-0.1])
        rotate([0,-35,0]) cube([3,58,3], center=false);
}

module rear_shell() {
    difference() {
        translate([0,0,front_depth]) body(rear_depth);
        translate([wall,wall,front_depth-0.01])
            linear_extrude(rear_depth-wall+0.02)
                chamfered_2d(body_w-2*wall, body_h-2*wall,
                             max(1,corner_chamfer-wall));
        // Bottom USB-C opening.
        translate([usb_center_x-usb_opening[0]/2, -1,
                   pcb_z-usb_opening[1]/2])
            cube([usb_opening[0], wall+2, usb_opening[1]]);
        // Right edge microSD opening.
        translate([body_w-wall-1, sd_center_y-sd_opening[0]/2,
                   pcb_z-sd_opening[1]/2])
            cube([wall+2, sd_opening[0], sd_opening[1]]);
        // Left edge latching power-switch opening.
        translate([-1, power_center_y-power_opening[0]/2,
                   pcb_z-power_opening[1]/2])
            cube([wall+2, power_opening[0], power_opening[1]]);
    }
    for (p = main_holes) pcb_standoff(p);
    rear_facets();
}

module knob() {
    difference() {
        union() {
            cylinder(h=5.5, d=encoder_knob_od);
            translate([0,0,5.5]) cylinder(h=1.2, d1=encoder_knob_od, d2=26.5);
        }
        translate([0,0,-1]) cylinder(h=8, d=encoder_shaft_d+0.15);
        // D-flat approximation for the EC12 shaft.
        translate([2.35,-4,-1]) cube([4,8,8]);
    }
}

module reference_envelopes() {
    color([0.15,0.45,0.15,0.35])
        translate([pcb_origin[0],pcb_origin[1],pcb_z])
            cube([pcb_w,pcb_h,pcb_t]);
    color([0.2,0.2,0.2,0.45])
        translate([lcd_center[0]-lcd_panel[0]/2,
                   lcd_center[1]-lcd_panel[1]/2,0.1])
            cube(lcd_panel);
    color([0.1,0.2,0.8,0.25])
        translate([battery_center[0]-battery_envelope[0]/2,
                   battery_center[1]-battery_envelope[1]/2,
                   pcb_z+pcb_t+0.8]) cube(battery_envelope);
    color([0.9,0.1,0.1,0.25])
        translate([antenna_keepout_xy[0], antenna_keepout_xy[1], front_depth])
            cube([antenna_keepout_size[0], antenna_keepout_size[1], rear_depth]);
}

if (part == "front") front_shell();
if (part == "rear") rear_shell();
if (part == "knob") knob();
if (part == "assembly") {
    color("ivory") front_shell();
    color("gainsboro") rear_shell();
    color("orange")
        translate([encoder_center[0],encoder_center[1],-6.7]) knob();
    reference_envelopes();
}
if (part == "exploded") {
    translate([0,0,-10]) front_shell();
    translate([0,0,10]) rear_shell();
    translate([encoder_center[0],encoder_center[1],-20]) knob();
    reference_envelopes();
}
