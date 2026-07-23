// Cadenza compact handheld — industrial-design concept model V1
// Units: millimetres. This is a visual/space concept, not production CAD.

$fn = 72;

body_w = 120;
body_h = 50;
body_d = 16;
rear_wedge_depth = 10;

screen_glass_w = 62.8;
screen_glass_h = 42.82;
screen_view_w = 58.8;
screen_view_h = 35.28;
screen_aperture_w = 60.0;
screen_aperture_h = 36.5;
screen_x = 0;

pearl = [0.84, 0.83, 0.79];
pearl_highlight = [0.93, 0.92, 0.88];
button_gray = [0.816, 0.816, 0.808]; // screen approximation: PANTONE Cool Gray 2 C
button_highlight = [0.86, 0.86, 0.85];
graphite = [0.10, 0.105, 0.10];
display_gray = [0.55, 0.57, 0.52];
label_gray = [0.245, 0.245, 0.225]; // screen approximation: PANTONE Black 7 C
metal_silver = [0.62, 0.64, 0.65];
ring_silver_blue = [0.53, 0.60, 0.64]; // muted cool silver-blue; PANTONE 7544 C direction

module pebble(size = [106, 50, 16], plan_r = 12, edge_r = 3.4) {
    core = [size[0] - 2 * plan_r, size[1] - 2 * plan_r, size[2] - 2 * edge_r];
    minkowski() {
        cube(core, center = true);
        scale([plan_r, plan_r, edge_r]) sphere(1);
    }
}

module rounded_plate(size = [10, 10, 1], radius = 2) {
    linear_extrude(height = size[2], center = true)
        offset(r = radius)
            square([size[0] - 2 * radius, size[1] - 2 * radius], center = true);
}

module capsule_slot(width, height, depth) {
    hull() {
        translate([-(width - height) / 2, 0, 0]) cylinder(h = depth, r = height / 2, center = true);
        translate([ (width - height) / 2, 0, 0]) cylinder(h = depth, r = height / 2, center = true);
    }
}

// Instrument-like keycap: a broad, nearly flat crown with a small continuous
// shoulder radius. This avoids both a raw cylinder and an over-soft pillow.
module softened_round_button(diameter, core_h, edge_r) {
    minkowski() {
        cylinder(h = core_h, d = diameter - 2 * edge_r, center = true);
        sphere(r = edge_r, $fn = 36);
    }
}

module softened_capsule_button(width, height, core_h, edge_r) {
    minkowski() {
        capsule_slot(width - 2 * edge_r, height - 2 * edge_r, core_h);
        sphere(r = edge_r, $fn = 36);
    }
}

// Rounded annular section for a jewellery-like bezel instead of a sharp,
// stacked cylindrical ring.
module rounded_control_ring(outer_d, inner_d, rise) {
    rotate_extrude(convexity = 10)
        translate([(outer_d + inner_d) / 4, 0, 0])
            scale([(outer_d - inner_d) / 4, rise]) circle(r = 1);
}

// Capsule cut/insert on the left short end. The long axis follows the body
// thickness (Z), matching the quiet horizontal window seen on handheld IR
// remotes; shallow cylinder depth follows the body X axis.
module left_end_capsule(length, height, depth, x) {
    hull() {
        for (z = [-(length - height) / 2, (length - height) / 2]) {
            translate([x, 0, z])
                rotate([0, 90, 0]) cylinder(h = depth, d = height, center = true);
        }
    }
}

module face_label(value, size, at, z) {
    color(label_gray)
        translate([at[0], at[1], z])
            linear_extrude(height = 0.22)
                text(value, size = size, halign = "center", valign = "center",
                     font = "Arial:style=Regular");
}

module enclosure() {
    color(pearl)
        difference() {
            // One continuous moulded exterior: the rear wedge is part of the
            // same base volume as the rounded handheld body, not an add-on.
            hull() {
                pebble([body_w, body_h, body_d], 12, 3.4);
                translate([0, -3.0, -body_d / 2 - rear_wedge_depth + 1.0])
                    rounded_plate([78, 22, 2.0], 6.0);
            }

            // Shallow screen pocket: the cover surface sits below the pearl
            // shell, leaving a narrow protective lip around the aperture.
            translate([screen_x, 0, body_d / 2 - 0.15])
                rounded_plate([screen_aperture_w + 1.0,
                               screen_aperture_h + 1.0, 1.50], 1.35);

            // Top-edge USB-C charging port, moved inward from the left end.
            translate([-32, body_h / 2 - 0.35, -1.0])
                rotate([90, 0, 0]) capsule_slot(10, 3.4, 2.4);

            // Nine visually identical top slots: the centre slot is the mic
            // inlet, while the four slots on either side serve the speaker.
            // Their internal acoustic chambers remain fully separated.
            for (x = [-14 : 3.5 : 14]) {
                hull() {
                    for (z = [-0.9, 2.1]) {
                        translate([x, body_h / 2 - 0.35, z])
                            rotate([90, 0, 0]) cylinder(h = 2.4, d = 0.95, center = true);
                    }
                }
            }
            // MicroSD sits alone at the bottom-right corner.
            translate([42.5, -body_h / 2 + 0.4, -0.8])
                rotate([90, 0, 0]) cube([13.5, 1.5, 3.0], center = true);

            // Shallow pocket for a flush smoked IR transmit/receive window on
            // the left short end. No emitter package is exposed externally.
            left_end_capsule(10.0, 3.0, 2.2, -body_w / 2 + 0.45);
        }

    // Dark recesses immediately behind the USB-C and MicroSD openings.
    color(graphite) {
        translate([-32, body_h / 2 - 1.25, -1.0])
            rotate([90, 0, 0]) capsule_slot(8.6, 2.3, 0.5);
        translate([42.5, -body_h / 2 + 1.25, -0.8])
            rotate([90, 0, 0]) cube([12, 0.7, 0.5], center = true);
    }

    // Closed deep-wine smoke optical lens, slightly crowned rather than cut
    // through like a connector. The production resin must pass the selected
    // IR wavelength while still hiding the electronics in normal light.
    color([0.16, 0.045, 0.035])
        left_end_capsule(9.6, 2.6, 0.34, -body_w / 2 - 0.08);

    // Two extremely restrained optical ghosts behind the smoked cover make
    // its purpose legible: a larger emitter optic and a smaller receiver eye.
    // They are visual concept cues, not exposed packages.
    color([0.31, 0.055, 0.040]) {
        translate([-body_w / 2 - 0.265, 0, -2.15])
            rotate([0, 90, 0]) cylinder(h = 0.035, d = 1.45, center = true);
        translate([-body_w / 2 - 0.265, 0, 2.15])
            rotate([0, 90, 0]) cylinder(h = 0.035, d = 1.10, center = true);
    }

    // Two tiny moulded shell ridges create the bottom footprint. They are not
    // separate dark rubber strips and should almost disappear visually.
    for (x = [-30, 30]) {
        color(pearl_highlight)
            translate([x, -body_h / 2 - 0.10, -0.2])
                rotate([90, 0, 0]) rounded_plate([2.2, 6.0, 0.42], 0.7);
    }
}

module display() {
    // The physical 62.8 x 42.82 mm glass envelope is buried under the front
    // enclosure. Only a narrow dark aperture remains visible around the LCD;
    // the mounting border and adhesive area are hidden by the pearl shell.
    color(graphite)
        translate([screen_x, 0, body_d / 2 - 0.65])
            rounded_plate([screen_aperture_w, screen_aperture_h, 0.14], 1.1);
    color(display_gray)
        translate([screen_x, 0, body_d / 2 - 0.56])
            rounded_plate([screen_view_w, screen_view_h, 0.08], 0.8);
}

module left_buttons() {
    // B remains the secondary action key. Only a shallow ellipsoidal dome is
    // exposed; a broad flat crown keeps it precise rather than pillow-like.
    color(button_gray)
        translate([-45.7, 9.0, body_d / 2 + 0.15])
            softened_round_button(10.5, 0.35, 0.45);

    // Menu is deliberately smaller and quieter: a low horizontal capsule,
    // not another action button competing with B or A.
    color(button_gray)
        translate([-45.7, -8.0, body_d / 2 + 0.10])
            softened_capsule_button(8.5, 3.4, 0.20, 0.35);

    // Three quiet dots identify Menu directly on the keycap.
    for (x = [-48.0, -45.7, -43.4]) {
        color(pearl_highlight)
            translate([x, -8.0, body_d / 2 + 0.55])
                sphere(d = 0.58);
    }

    face_label("B", 3.7, [-45.7, 9.0], body_d / 2 + 0.78);
}

module a_controller() {
    controller_x = 45.7;

    // A restrained cool silver-blue rounded bezel adds identity while keeping
    // the active face matte and visually quiet.
    color(ring_silver_blue)
        translate([controller_x, 0, body_d / 2 + 0.16])
            rounded_control_ring(22.6, 20.8, 0.40);

    color(button_gray)
        translate([controller_x, 0, body_d / 2 + 0.55])
            softened_round_button(20.5, 1.20, 0.50);

    // A faint centre ring marks the press zone without stacking another
    // cylindrical button on top of the rotary surface.
    color(button_highlight)
        translate([controller_x, 0, body_d / 2 + 1.66])
            difference() {
                cylinder(h = 0.08, d = 8.2, center = true);
                cylinder(h = 0.10, d = 7.7, center = true);
            }

    // Tactile dot ring on the rotary face.
    for (a = [0 : 36 : 324]) {
        color(button_highlight)
            translate([controller_x + 7.5 * cos(a), 7.5 * sin(a), body_d / 2 + 1.68])
                sphere(d = 0.72);
    }
    color(label_gray)
        translate([controller_x, 0, body_d / 2 + 1.66])
            linear_extrude(height = 0.22)
                text("A", size = 4.3, halign = "center", valign = "center",
                     font = "Arial:style=Regular");
}

module speaker_and_mic() {
    // A continuous recessed acoustic dust mesh sits behind the complete slot
    // array. Externally it reads as one ordered Braun/Sony-like grille; inside,
    // the centre mic inlet must use a sealed side channel isolated from the
    // left/right speaker chambers.
    color([0.16, 0.17, 0.16])
        translate([0, body_h / 2 - 1.05, 0.6])
            rotate([90, 0, 0]) rounded_plate([30, 5.2, 0.28], 1.1);

    // Dark acoustic cavities directly behind the open top-edge pill slots.
    for (x = [-14 : 3.5 : 14]) {
        color(graphite)
            hull() {
                for (z = [-0.9, 2.1]) {
                    translate([x, body_h / 2 - 0.45, z])
                        rotate([90, 0, 0]) cylinder(h = 0.55, d = 0.95, center = true);
                }
            }
    }
}

module power_lock() {
    // Small top-edge capsule, separate from the main A/B/Menu controls.
    color(metal_silver)
        hull() {
            for (x = [33.2, 40.2]) {
                translate([x, body_h / 2 + 0.30, 1.0])
                    rotate([90, 0, 0]) cylinder(h = 1.1, r = 1.65, center = true);
            }
        }
}

module rear_service_details() {
    back_z = -body_d / 2;
    outer_z = back_z - rear_wedge_depth;

    // The solid wedge itself is already part of enclosure(); only quiet
    // service details are layered onto its outer face here.
    color([0.73, 0.72, 0.69])
        translate([0, -3.0, outer_z - 0.06])
            rounded_plate([66, 14, 0.14], 4.2);
    color(pearl_highlight)
        translate([0, -3.0, outer_z - 0.15])
            rounded_plate([65.2, 13.2, 0.12], 3.9);

    // Shallow same-material script emboss: visible mainly through grazing
    // light and shadow, never a loud printed logo.
    color([0.78, 0.77, 0.73])
        translate([0, -3.0, outer_z - 0.25])
            rotate([180, 0, 0])
                linear_extrude(height = 0.42)
                    text("Cadenza", size = 6.2, halign = "center", valign = "center",
                         font = "Snell Roundhand:style=Bold");

    for (x = [-30 : 2 : -24]) {
        color([0.76, 0.75, 0.72])
            translate([x, -3.0, outer_z - 0.23])
                rounded_plate([0.7, 5.0, 0.10], 0.25);
    }
    color(metal_silver)
        translate([29, -3.0, outer_z - 0.24])
            cylinder(h = 0.14, d = 1.8, center = true);

}

module cadenza_concept_v1() {
    enclosure();
    display();
    left_buttons();
    a_controller();
    speaker_and_mic();
    power_lock();
    rear_service_details();
}

cadenza_concept_v1();
