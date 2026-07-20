// Cadenza Rev A D0 enclosure envelope - dimensions in mm
$fn=72;
W=122; H=74; D=19; wall=2; clearance=0.35;
screen_view=[59.2,35.7]; screen_glass=[63.2,43.2];
module rounded_box(w,h,d,r=8){ hull(){ for(x=[-w/2+r,w/2-r]) for(y=[-h/2+r,h/2-r]) translate([x,y,0]) cylinder(h=d,r=r); } }
module front_shell(){ difference(){ rounded_box(W,H,D/2,9); translate([0,0,wall]) rounded_box(W-2*wall,H-2*wall,D/2,7); translate([0,0,-1]) cube([screen_view[0],screen_view[1],D],center=true); translate([48,0,-1]) cylinder(h=D,r=14); translate([-48,10,-1]) cylinder(h=D,r=4); translate([-48,-10,-1]) cylinder(h=D,r=4); for(x=[-54:2:-40]) for(y=[-28:2:-20]) translate([x,y,-1]) cylinder(h=D,r=.55); translate([-53,21,-1]) cylinder(h=D,r=.55); } }
module rear_shell(){ difference(){ translate([0,0,D/2]) rounded_box(W,H,D/2,9); translate([0,0,D/2-0.1]) rounded_box(W-2*wall,H-2*wall,D/2,7); } translate([-35,-H/2,D/2]) rotate([0,18,0]) cube([35,4,9]); translate([18,-H/2,D/2]) rotate([0,35,0]) cube([28,4,9]); }
front_shell();
translate([0,90,0]) rear_shell();
