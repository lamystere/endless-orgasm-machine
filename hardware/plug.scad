include <BOSL2/std.scad> //You will need to install the BOSL2 library to your openSCAD library (https://github.com/BelfrySCAD/BOSL2):
$fn = 100;

show_body = true;
show_lid = true;
show_cap = true;
preview_electronics = true;
show_mold = false;

seed_w = 20;
seed_h = 24;
seed_d = 2;
usb_w = 11;
usb_h = 5;

ps_w = 20;
ps_h = 20;
ps_d = 10;

battery_w = 40;
battery_h = 25;
battery_d = 15;

box_iw = 65;
box_ih = 29;
box_id = 18;
box_top_rise = 3;
box_fillet = 5;
box_bottom_fillet_hack = 5;

shaft_od = 12;
shaft_h = 25;

cap_bottom_h = 10;
cap_middle_h = 5;
cap_top_h = 25;
cap_middle_od = 27;

wall_thickness = 2;
tubing_od = 3;
tubing_od_allowance = 1;

if (show_lid && !show_mold) lid();
if (show_body && !show_mold) body();
if (show_cap && !show_mold) cap();

box_points = [
  [-box_iw / 2 - wall_thickness * 2, -box_ih / 2 - box_fillet - wall_thickness], //bottom left
  [box_iw / 2 + wall_thickness * 2, -box_ih / 2 - box_fillet - wall_thickness], //bottom right
  //[box_iw/2 + wall_thickness,                     box_ih/2], //top right corner
  for (
    //top right side curve;
    i = bezpath_curve(
      [
        [box_iw / 2 + wall_thickness, box_ih / 2 - box_fillet],
        [box_iw / 2 + wall_thickness, box_ih / 2 - box_fillet / 2],
        [box_iw / 2 + wall_thickness - box_fillet / 2, box_ih / 2],
        [box_iw / 2 + wall_thickness - box_fillet, box_ih / 2 + (box_fillet / (box_iw / 2 + wall_thickness))],
      ]
    )
  ) i,
  [shaft_od / 2 + tubing_od + tubing_od_allowance, box_ih / 2 + box_top_rise], //shaft right
  [-shaft_od / 2 - tubing_od - tubing_od_allowance, box_ih / 2 + box_top_rise], //shaft left
  //[-box_iw/2 - wall_thickness,                    box_ih/2]  //top left corner
  for (
    //top left side curve;
    i = bezpath_curve(
      [
        [-box_iw / 2 - wall_thickness + box_fillet, box_ih / 2 + (box_fillet / (box_iw / 2 + wall_thickness))],
        [-box_iw / 2 - wall_thickness + box_fillet / 2, box_ih / 2],
        [-box_iw / 2 - wall_thickness, box_ih / 2 - box_fillet / 2],
        [-box_iw / 2 - wall_thickness, box_ih / 2 - box_fillet],
      ]
    )
  ) i,
];

bottom_of_shaft = box_ih / 2 + box_top_rise + wall_thickness;

module body() {
  //components compartment
  difference() {
    union() {
      translate([-box_id / 2 - wall_thickness, 0, tubing_od_allowance * 2])
        rotate([90, 0, 90])
          //filleted top and bottom
          fillet_extrude(height=box_id + wall_thickness * 2, r1=-5, r2=-5)
            polygon(box_points);

      //shaft tubing collar
      difference() {
        translate([0, 0, bottom_of_shaft]) cylinder(h=tubing_od / 2, d1=shaft_od + tubing_od, d2=shaft_od, center=true);
        translate([0, 0, bottom_of_shaft + tubing_od / 4]) torus(id=shaft_od, od=shaft_od + tubing_od, $fn=100);
      }
    }
    translate([0, 0, -box_id - box_fillet / 2])
      cube([box_iw + wall_thickness * 2, box_iw + wall_thickness * 2 + box_fillet * 2, box_fillet * 2], center=true);
    //cut off bottom of box
    if (!show_mold) {

      translate([0, 0, -wall_thickness])
        cube([box_id, box_iw, box_ih], center=true);
      //cavity for electronics
      cylinder(h=shaft_h + 2, d=tubing_od, center=true); //center hole for wire
      translate([shaft_od / 2 + tubing_od / 2, 0, bottom_of_shaft])
        rotate([45, 0, 0])
          cylinder(h=box_ih, d=tubing_od + tubing_od_allowance / 2, center=true);
      //hole for pressure tube
    }
  }

  if (!show_mold) {
    //shaft
    difference() {
      translate([0, 0, bottom_of_shaft + shaft_h / 2])
        cylinder(h=shaft_h, d=shaft_od, center=true);
      //shaft

      translate([0, 0, bottom_of_shaft + shaft_h / 2])
        cylinder(h=shaft_h + 2, d=tubing_od, center=true);
      //hole up the middle
      translate([0, 0, bottom_of_shaft + shaft_h - tubing_od / 2])
        rotate([90, 0, 0])
          cylinder(h=cap_middle_od + 2, d=tubing_od + tubing_od_allowance / 2, center=true);
      //hole for tubing end
    }
  }
}

top_of_shaft = bottom_of_shaft + shaft_h;

module cap() {
  cap_points = [
    [0, top_of_shaft], //point
    [tubing_od / 4, top_of_shaft],
    [tubing_od / 2, top_of_shaft],
    [shaft_od / 2 + tubing_od / 2, top_of_shaft], //point
    [shaft_od / 2 + tubing_od / 2, top_of_shaft + cap_bottom_h / 4],
    [cap_middle_od / 2 * .75, top_of_shaft + cap_bottom_h - cap_middle_h / 4],
    [cap_middle_od / 2 * .875, top_of_shaft + cap_bottom_h], //point
    [cap_middle_od / 2, top_of_shaft + cap_bottom_h + cap_middle_h / 4],
    [cap_middle_od / 2, top_of_shaft + cap_bottom_h + cap_middle_h * .75],
    [cap_middle_od / 2, top_of_shaft + cap_bottom_h + cap_middle_h], //point
    [cap_middle_od / 2, top_of_shaft + cap_bottom_h + cap_middle_h + cap_top_h / 4],
    [shaft_od / 2, top_of_shaft + cap_bottom_h + cap_middle_h + cap_top_h],
    [0, top_of_shaft + cap_bottom_h + cap_middle_h + cap_top_h], //point
  ];
  rotate_extrude()
    //create cap shape
    polygon(bezpath_curve(cap_points));
}

module lid() {
  //components back cover
  translate([box_id / 2 + wall_thickness + 5, 0, -box_id / 2 + .75]) {
    //?
    difference() {
      scale([1, 1, .5])
        difference() {
          translate([0, 0, -box_ih - wall_thickness + tubing_od_allowance * 2])
            rotate([90, 180, 90])
              fillet_extrude(height=box_id + wall_thickness * 2, r1=-5, r2=-5)
                polygon(box_points);
          //filleted top and bottom

          //cut off top this time
          translate([-box_iw / 2, -box_iw / 2 - wall_thickness - box_fillet, -box_id * 2 - box_id / 2 - box_fillet])
            cube([box_iw + wall_thickness * 2, box_iw + wall_thickness * 2 + box_fillet * 2, box_id * 2]);
        }

      //usb hole
      translate([usb_h / 2 + wall_thickness, -1 * (box_iw / 2 - wall_thickness - usb_w), -usb_h - 2])
        //cube([usb_h, usb_w, usb_h], center=true);
        linear_extrude(height=usb_h, scale=2)
          square([usb_h, usb_w], center=true);
      //expand the square outward
    }
  }
}

overall_height = bottom_of_shaft + box_id / 2 + shaft_h + cap_bottom_h + cap_middle_h + cap_top_h + box_top_rise + wall_thickness;

if (show_mold) {
  scale([1.1, 1.1, 1.1]) {
    difference() {
      union() {
        #translate([0, 0, (overall_height) / 2 - box_id + wall_thickness *2])
          cylinder(h=overall_height + wall_thickness*2, d=cap_middle_od + 4, center=true);//shaft

        #translate([0, 0, (wall_thickness + box_top_rise) / 2])
          cube([box_id + wall_thickness * 4 + 6, box_iw + wall_thickness * 6, box_ih + wall_thickness * 2 + box_top_rise], center=true);
      }
      translate([0, 0, - box_id + wall_thickness]) cylinder(h=overall_height + wall_thickness , d=cap_middle_od);

      body();
      cap();

      translate([0, 0, -box_id + 2.5])
        cube([box_id + wall_thickness * 4 + 6, box_iw + wall_thickness * 6, 1], center=true);
    }
  }
}





// From The_Hans: https://gist.github.com/thehans/b47ab7077c862361eb5d8f095448b2d4
// linear_extrude with optional fillet radius on each end
// Parameters:
//   height - total extrusion length including radii
//   r1 - bottom radius
//   r2 - top radius
//    positive radii will expand outward towards their end
//    negative will shrink inward towards their end
// Limitations: 
//   - individual children of fillet_extrude should be convex
//   - only straight extrudes with no twist or scaling supported
//   - fillets only for 90 degress betweeen Z axis and top/bottom surface
module fillet_extrude(height = 100, r1 = 0, r2 = 0) {
  function fragments(r = 1) =
    ($fn > 0) ? ($fn >= 3 ? $fn : 3) : ceil(max(min(360.0 / $fa, r * 2 * PI / $fs), 5));
  assert(abs(r1) + abs(r2) <= height);
  midh = height - abs(r1) - abs(r2);
  eps = 1 / 1024;
  union() {
    if (r1 != 0) {
      fn1 = ceil(fragments(abs(r1)) / 4); // only covering 90 degrees
      for (i = [0:1:$children - 1], j = [1:1:fn1]) {
        a1 = 90 * (j - 1) / fn1;
        a2 = 90 * j / fn1;
        h1 = abs(r1) * (1 - cos(a1));
        h2 = abs(r1) * (1 - cos(a2));
        off1 = r1 * (1 - sin(a1));
        off2 = r1 * (1 - sin(a2));
        hull() {
          translate([0, 0, h1]) {
            // in case radius*2 matches width of object, don't make first layer zero width
            off1 = r1 < 0 && j == 1 ? off1 * (1 - eps) : off1;
            linear_extrude(eps) offset(r=off1) children(i);
          }
          translate([0, 0, h2])
            linear_extrude(eps) offset(r=off2) children(i);
        }
      }
    }
    if (midh > 0) {
      translate([0, 0, abs(r1)])for (i = [0:1:$children - 1]) linear_extrude(midh) children(i);
    }
    if (r2 != 0) {
      fn2 = ceil(fragments(abs(r2)) / 4); // only covering 90 degrees
      translate([0, 0, height - abs(r2) - eps]) {
        for (i = [0:1:$children - 1], j = [1:1:fn2]) {
          a1 = 90 * (j - 1) / fn2;
          a2 = 90 * j / fn2;
          h1 = abs(r2) * (sin(a1));
          h2 = abs(r2) * (sin(a2));
          off1 = r2 * (1 - cos(a1));
          off2 = r2 * (1 - cos(a2));
          hull() {
            translate([0, 0, h1])
              linear_extrude(eps) offset(r=off1) children(i);
            translate([0, 0, h2]) {
              // in case radius*2 matches width of object, don't make last layer zero width
              off2 = r2 < 0 && j == fn2 ? off2 * (1 - eps) : off2;
              linear_extrude(eps) offset(r=off2) children(i);
            }
          }
        }
      }
    }
  }
}
