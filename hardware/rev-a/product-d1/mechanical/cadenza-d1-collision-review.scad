// Cadenza D1 digital collision review against the aligned KiCad assembly mesh.
use <cadenza-d1-enclosure.scad>

target = "rear"; // front or rear
pcb_mesh = "../kicad/production-v50-release-20260721/mechanical/cadenza-d1-v50-pcb-aligned.stl";

module pcb_real() {
    import(pcb_mesh, convexity=20);
}

if (target == "front") intersection() {
    front_shell();
    pcb_real();
}

if (target == "rear") intersection() {
    rear_shell();
    pcb_real();
}
