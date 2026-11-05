// ESP32 multiple-IO board case
// Board: 75 x 64 mm, 1.6 mm thick, 4x 2.032 mm mounting holes
// Case: rounded base tray + friction-fit lid + optional KEY1 plunger
// Print base upright, lid upside-down (top face on the bed).

part = "base";          // "base", "base_501", "lid", "lid_501", "lid_pir", "plunger",
                        // "pir501_cover", "mic_tube", "assembly", "assembly_pir",
                        // "assembly_501", "none"

$fn = 64;

// ---------------- board / case parameters ----------------
pcb_w = 75.0;
pcb_h = 64.0;
pcb_t = 1.6;

clearance   = 1.5;      // gap around PCB
wall        = 1.6;
floor_t     = 1.6;
bottom_clr  = 6.5;      // PCB bottom to inner floor (M2x8 clearance)
top_clr     = 21.0;     // PCB top to lid inner top (20 mm requested + 1 mm margin)
rim_above   = 3.0;      // base wall above PCB top
lid_top_t   = 1.6;

case_extra  = 1.6;      // original extra width/depth for the external rabbet
side_extra  = 2.0;      // additional width on each side of the PCB
corner_r    = 4.0;      // outer vertical corner radius
edge_cham   = 1.0;      // top/bottom outer edge chamfer

boss_w      = 8.0;      // square corner bosses
pilot_d     = 1.85;     // M2 self-tapping pilot
pilot_depth = 6.5;

// ---------------- derived ----------------
inner_w = pcb_w + 2*clearance + 2*side_extra;
inner_h = pcb_h + 2*clearance;
outer_w = inner_w + 2*wall + case_extra;
outer_h = inner_h + 2*wall + case_extra;
inner_r = corner_r - wall;

// Optional extra length toward the U3/USB cable (front = -Y).
// Set with -D front_ext=30; the PCB and screw bosses do not move.
front_ext = is_undef(front_ext) ? 0 : front_ext;
case_depth = outer_h + front_ext;
case_y0    = -front_ext;

// Base/lid joint.  The base rim has a lowered outer shelf and a raised inner
// lip; the lid has a raised outer skirt that sits on that shelf.  The lid
// skirt is 0.2 mm clear of the base inner lip so the parts can seat.
base_x0  = -case_extra/2 - side_extra;
base_y0  = case_y0 - case_extra/2;
base_w   = outer_w;
base_d   = case_depth;
shelf_w  = 1.0;              // width of the lowered outer shelf
skirt_h  = 0.8;              // shelf depth / lid skirt height
skirt_w  = 0.8;              // lid outer skirt thickness
lid_wall = wall + 0.1;       // matches the base cavity wall

z_floor     = floor_t;
z_pcb_b     = z_floor + bottom_clr;
z_pcb_t     = z_pcb_b + pcb_t;
z_base_top  = z_pcb_t + rim_above;
z_lid_inner = z_pcb_t + top_clr;
z_top       = z_lid_inner + lid_top_t;

// Lid screws: inserted from under the base, heads recessed in the floor.
// M2x20: 1.6 mm floor + 11.1 mm base post + 7.3 mm lid-post grip.
lid_screw_edge   = 2.4;   // screw centre to the outer side wall
lid_screw_corner = 3.55;  // screw centre to the outer front/back wall
lid_screw_x   = [base_x0 + lid_screw_edge,
                 base_x0 + base_w - lid_screw_edge];
lid_screw_y   = [base_y0 + lid_screw_corner,
                 base_y0 + base_d - lid_screw_corner];
lid_screw_d   = 2.2;    // clearance through base floor and boss
lid_pilot_d   = 1.85;   // pilot in the lid post
lid_post_d    = 3.8;
lid_pilot_h   = 8.0;

// board -> case coordinates
function bx(gx) = wall + clearance + gx;
function by(gy) = wall + clearance + pcb_h + gy;   // gy is negative

// 4 mounting holes, Gerber coords
holes = [
  [bx( 2.032), by( -1.905)],
  [bx(73.152), by( -1.905)],
  [bx(73.152), by(-62.230)],
  [bx( 2.032), by(-62.230)]
];

// component reference points
key1_x = bx(8.255);   key1_y = by(-53.848);   // 12x12 mm tactile switch
phr_x  = bx(2.667);   phr_y  = by(-38.862);   // LDR body 2.54 x 5.08 mm
mic_x  = bx(62.865);  mic_y  = by( -7.493);   // round I2S mic module
mic_tube_id   = 4.0;    // INMP441 sound tube inner diameter
mic_tube_len  = 8.6;    // tube length below the lid inner face
mic_tube_wall = 1.2;    // tube wall thickness
vacg_cx = bx(62.357); vacg_cy = by(-36.220);  // 4x6 I2C header field

// ---------------- 2D helpers ----------------
module rrect(w, h, r) {
  hull() {
    translate([r, r]) circle(r=r);
    translate([w-r, r]) circle(r=r);
    translate([r, h-r]) circle(r=r);
    translate([w-r, h-r]) circle(r=r);
  }
}

module rrect_z(w, h, r, z0, z1) {
  translate([0, 0, z0]) linear_extrude(height=z1-z0) rrect(w, h, r);
}

module rounded_slot(x, y, z, len, wid, h) {
  hull() {
    translate([x - (len - wid)/2, y, z]) cylinder(d=wid, h=h);
    translate([x + (len - wid)/2, y, z]) cylinder(d=wid, h=h);
  }
}

// USB cable hole, split by the base/lid seam at z_base_top.
// 4 mm hole for a 3.2 mm cable, centred on the U3/USB x position.
usb_x = 39.44;
usb_d = 4.0;

module usb_slot() {
  translate([usb_x, case_y0 - 1, z_base_top])
    rotate([-90, 0, 0])
      cylinder(d=usb_d, h=wall + 2);
}

// ---------------- base ----------------
module base_outer() {
  // main body (overlaps the chamfer slightly to avoid coplanar faces)
  translate([base_x0, base_y0, 0])
    rrect_z(base_w, base_d, corner_r, edge_cham - 0.1, z_base_top);
  // bottom chamfer
  hull() {
    translate([base_x0 + edge_cham, base_y0 + edge_cham, 0])
      rrect_z(base_w - 2*edge_cham, base_d - 2*edge_cham,
              corner_r - edge_cham, 0, 0.01);
    translate([base_x0, base_y0, 0])
      rrect_z(base_w, base_d, corner_r, edge_cham - 0.01, edge_cham + 0.1);
  }
}

module base() {
  difference() {
    union() {
      // shell with rounded corners and chamfered bottom
      difference() {
        base_outer();
        translate([base_x0 + lid_wall, base_y0 + lid_wall, 0])
          rrect_z(inner_w + case_extra - 0.2, case_depth - 2*wall - 0.2,
                  inner_r, z_floor, z_base_top + 0.1);
      }
      // corner screw bosses
      for (p = holes)
        translate([p[0] - boss_w/2, p[1] - boss_w/2, z_floor])
          cube([boss_w, boss_w, bottom_clr]);
      // flat pads keep the recessed lid-screw heads supported at the chamfer
      for (sx = lid_screw_x)
        for (sy = lid_screw_y)
          translate([sx, sy, 0])
            cylinder(d=4.6, h=edge_cham + 0.1);
      // full-height lid-screw posts at the corners
      for (sx = lid_screw_x)
        for (sy = lid_screw_y)
          translate([sx, sy, z_floor])
            cylinder(d=lid_post_d, h=z_base_top - z_floor);
    }

    // M2 pilot holes with a lead-in counterbore at the boss top
    for (p = holes) {
      translate([p[0], p[1], z_floor])
        cylinder(d=pilot_d, h=pilot_depth + 0.1);
      translate([p[0], p[1], z_pcb_b - 0.8])
        cylinder(d=pilot_d + 0.4, h=0.9);
    }
    // lid-screw clearance through the corner posts and base floor
    for (sx = lid_screw_x)
      for (sy = lid_screw_y) {
        translate([sx, sy, -0.1])
          cylinder(d=lid_screw_d, h=z_base_top + 0.2);
        translate([sx, sy, -0.1])
          cylinder(d=4.0, h=1.5);
      }

    // external rabbet: lower the outer shelf_w of the rim.  The cut starts
    // outside the true outer surface so no raised external lip remains.
    translate([base_x0 - 0.1, base_y0 - 0.1, z_base_top - skirt_h])
      difference() {
        rrect_z(base_w + 0.2, base_d + 0.2, corner_r, 0, skirt_h + 0.01);
        translate([shelf_w + 0.1, shelf_w + 0.1, 0])
          rrect_z(base_w - 2*shelf_w, base_d - 2*shelf_w,
                  corner_r - shelf_w, 0, skirt_h + 0.01);
      }

    // USB cable gap (lower half)
    usb_slot();
  }
}

// ---------------- HC-SR501 front bay (base variant) ----------------
// The bay is on the front-right, outside the main case. The PIR sits on the
// 2 mm bay floor, dome facing forward through a 25 mm hole. The top is open
// for access to the sensitivity/time potentiometers.
pir501_w       = 32.0;   // PCB width
pir501_d       = 25.0;   // PCB depth (including dome)
pir501_h       = 24.0;   // PCB height
pir501_clear   = 0.2;
pir501_wall    = 1.6;
pir501_floor   = 1.6;
pir501_bay_h   = 30.0;   // outer bay height
pir501_lens_d  = 25.0;
pir501_y0      = case_y0 - 27.4;  // bay outer front

pir501_inner_w = pir501_w + 2*pir501_clear;      // 32.4
pir501_inner_d = pir501_d + 2*pir501_clear;      // 25.4
pir501_outer_w = pir501_inner_w + 2*pir501_wall; // 35.6
pir501_outer_d = pir501_inner_d + pir501_wall;   // 27.0
// keep the bay 0.6 mm outside the new right-hand case wall
pir501_x0      = base_x0 + base_w + 0.6 - pir501_outer_w;
pir501_cx      = pir501_x0 + pir501_outer_w/2;
pir501_cz      = pir501_floor + pir501_h/2;      // 14.0

module pir501_bay_outer() {
  // lower part: 1 mm flange into the front wall, up to the joint shelf
  translate([pir501_x0, pir501_y0, 0])
    rrect_z(pir501_outer_w, pir501_outer_d + 1.0, 3, 0, z_base_top - skirt_h);
  // upper part: starts just below the shelf, clear of the lid front wall
  translate([pir501_x0, pir501_y0, z_base_top - skirt_h - 0.1])
    rrect_z(pir501_outer_w, pir501_outer_d - 0.2, 3, 0,
            pir501_bay_h - (z_base_top - skirt_h - 0.1));
}

module pir501_bay_cuts() {
  // inner cavity
  translate([pir501_x0 + pir501_wall, pir501_y0 + pir501_wall, pir501_floor])
    cube([pir501_inner_w, pir501_inner_d, pir501_bay_h - pir501_floor + 0.1]);
  // lens hole through the front wall
  translate([pir501_cx, pir501_y0 - 1.0, pir501_cz])
    rotate([-90, 0, 0])
      cylinder(d=pir501_lens_d, h=pir501_wall + 2.0);
  // 3-pin cable hole through the main front wall, centred in the bay
  translate([pir501_cx - 4, case_y0 - 1, 4])
    cube([8, 4, 6]);
}

module base_501() {
  difference() {
    union() {
      base();
      pir501_bay_outer();
    }
    pir501_bay_cuts();
  }
}

// preview-only HC-SR501 mock (not exported)
module pir501_mock() {
  color("White", 0.55)
    translate([pir501_x0 + pir501_wall + pir501_clear,
               pir501_y0 + pir501_wall + pir501_clear,
               pir501_floor])
      cube([pir501_w, pir501_d - 1.0, pir501_h]);
  color("White", 0.35)
    translate([pir501_cx, pir501_y0 + pir501_wall + pir501_clear - 0.1, pir501_cz])
      rotate([-90, 0, 0])
        cylinder(d=23, h=0.2);
}

// ---------------- HC-SR501 bay cover ----------------
module pir501_cover() {
  // top plate, 1 mm overhang at the front and sides for lifting; the back
  // stops 0.2 mm in front of the lid so it cannot collide with the case
  translate([pir501_x0 - 1.0, pir501_y0 - 1.0, pir501_bay_h])
    rrect_z(pir501_outer_w + 2.0, pir501_outer_d + 0.4, 3, 0, 1.6);
  // friction lip inside the bay, stopping just short of the lid front wall
  translate([pir501_x0 + pir501_wall + 0.2,
             pir501_y0 + pir501_wall + 0.2,
             pir501_bay_h - 1.6])
    cube([pir501_inner_w - 0.4, pir501_inner_d - 0.7, 1.7]);
}

// ---------------- lid ----------------
module lid_outer() {
  // main body, flush with the base outer surface
  translate([base_x0, base_y0, 0])
    rrect_z(base_w, base_d, corner_r, z_base_top, z_top - edge_cham + 0.1);
  // top chamfer
  hull() {
    translate([base_x0, base_y0, 0])
      rrect_z(base_w, base_d, corner_r, z_top - edge_cham - 0.1, z_top - edge_cham + 0.01);
    translate([base_x0 + edge_cham, base_y0 + edge_cham, 0])
      rrect_z(base_w - 2*edge_cham, base_d - 2*edge_cham,
              corner_r - edge_cham, z_top - 0.01, z_top);
  }
}

module lid(use_pir = false) {
  difference() {
    union() {
      // walls + top plate; cavity matches the base cavity
      difference() {
        lid_outer();
        translate([base_x0 + lid_wall, base_y0 + lid_wall, 0])
          rrect_z(inner_w + case_extra - 0.2, case_depth - 2*wall - 0.2,
                  inner_r, z_base_top - 0.1, z_lid_inner);
      }
      // microphone acoustic tube on the lid underside
      translate([mic_x, mic_y, z_lid_inner - mic_tube_len])
        cylinder(d=mic_tube_id + 2*mic_tube_wall, h=mic_tube_len + 0.1);
      // outer skirt that sits on the base shelf.  It is 0.01 mm inside the
      // main body and 0.2 mm clear of the raised base inner lip.
      translate([base_x0 + 0.01, base_y0 + 0.01, z_base_top - skirt_h])
        difference() {
          rrect_z(base_w - 0.02, base_d - 0.02, corner_r - 0.01, 0, skirt_h + 0.05);
          translate([skirt_w - 0.01, skirt_w - 0.01, 0])
            rrect_z(base_w - 2*skirt_w, base_d - 2*skirt_w,
                    corner_r - skirt_w, 0, skirt_h + 0.05);
        }
      // lid-screw posts sit on the base rim / bosses, not below it
      for (sx = lid_screw_x)
        for (sy = lid_screw_y)
          translate([sx, sy, z_base_top])
            cylinder(d=lid_post_d, h=z_lid_inner - z_base_top);
    }

    // KEY1 opening + chamfer
    translate([key1_x, key1_y, z_lid_inner - 1])
      cylinder(d=14, h=z_top - z_lid_inner + 2);
    translate([key1_x, key1_y, z_top - 1])
      cylinder(d1=14, d2=16, h=1.01);

    // PHR top light hole + chamfer
    translate([phr_x, phr_y, z_lid_inner - 1])
      cylinder(d=5, h=z_top - z_lid_inner + 2);
    translate([phr_x, phr_y, z_top - 1])
      cylinder(d1=5, d2=7, h=1.01);

    // MIC1 sound hole through the lid and acoustic tube, plus top chamfer
    translate([mic_x, mic_y, z_lid_inner - mic_tube_len - 1])
      cylinder(d=mic_tube_id,
               h=z_top - (z_lid_inner - mic_tube_len - 1) + 1);
    translate([mic_x, mic_y, z_top - 1])
      cylinder(d1=mic_tube_id, d2=6, h=1.01);

    // VACG top vent slots (5x 8 x 2 mm)
    for (yy = [25.5, 28.5, 31.5, 34.5, 37.5])
      rounded_slot(vacg_cx, yy, z_lid_inner - 1, 8, 2,
                   z_top - z_lid_inner + 2);

    // PHR side window: one piece in the lid, starting at the seam
    translate([base_x0 - 1, phr_y - 6, z_base_top - 0.1])
      cube([lid_wall + 2, 12, 10.1]);

    // VACG right-side vents: one piece in the lid, starting at the seam
    for (yy = [26, 31, 36])
      translate([base_x0 + base_w - wall - 1, yy - 1.5, z_base_top])
        cube([wall + 2, 3.0, 10.0 + 0.1]);

    // USB cable gap (upper half)
    usb_slot();

    // lid-screw pilots in the posts, open from the base side
    for (sx = lid_screw_x)
      for (sy = lid_screw_y)
        translate([sx, sy, z_base_top])
          cylinder(d=lid_pilot_d, h=lid_pilot_h);

    // optional PIR window (lid only, 13 x 13.4 mm from the seam up)
    if (use_pir)
      translate([18, case_y0 - 1, z_base_top - 0.1])
        cube([13, wall + 2, (z_pcb_t + 16.4) - z_base_top + 0.2]);
  }
}

// ---------------- KEY1 plunger (optional) ----------------
// Fits through the 14 mm lid hole, cap 16 mm stays above the lid.
// Base plate 13 mm rests on the 12 mm switch. Adjust switch_h if needed.
switch_h = 5.5;                 // switch body + button above PCB
switch_top = z_pcb_t + switch_h;
plunger_base_t = 2.0;
plunger_cap_t  = 2.0;
plunger_gap    = 2.0;           // cap above lid
plunger_shaft  = (z_top + plunger_gap) - (switch_top + plunger_base_t);

module plunger() {
  // base plate on the switch
  translate([0, 0, switch_top])
    cylinder(d=13, h=plunger_base_t);
  // shaft through the lid
  translate([0, 0, switch_top + plunger_base_t])
    cylinder(d=6, h=plunger_shaft + 0.1);
  // cap above the lid
  translate([0, 0, z_top + plunger_gap])
    difference() {
      cylinder(d=16, h=plunger_cap_t);
      translate([0, 0, plunger_cap_t - 1.0])
        cylinder(d1=8, d2=12, h=1.01);   // finger dish
    }
}

// ---------------- MIC1 gasket / tube (HS-3 test part) ----------------
// Short tube between the INMP441 port and the 4 mm lid hole. 4 mm ID,
// 1.2 mm wall, 10 mm flange at each end. mic_tube_len is the overall
// length, flanges included. Print standing on a flange (as oriented).
mic_tube_id       = 4.0;
mic_tube_wall     = 1.2;
mic_tube_len      = is_undef(mic_tube_len) ? 8.6 : mic_tube_len;
mic_tube_flange_d = 10.0;
mic_tube_flange_t = 1.2;
mic_tube_ft       = min(mic_tube_flange_t, mic_tube_len / 2);

module mic_tube() {
  difference() {
    union() {
      cylinder(d=mic_tube_id + 2*mic_tube_wall, h=mic_tube_len);
      cylinder(d=mic_tube_flange_d, h=mic_tube_ft);
      translate([0, 0, mic_tube_len - mic_tube_ft])
        cylinder(d=mic_tube_flange_d, h=mic_tube_ft);
    }
    // 4 mm through-bore
    translate([0, 0, -0.1])
      cylinder(d=mic_tube_id, h=mic_tube_len + 0.2);
  }
}

// ---------------- output ----------------
if (part == "base") {
  base();
} else if (part == "base_501") {
  base_501();
} else if (part == "pir501_cover") {
  // print orientation: plate on the bed, lip up
  translate([0, 0, pir501_bay_h + 1.6]) rotate([180, 0, 0]) pir501_cover();
} else if (part == "lid") {
  // print orientation: top face on the bed, open side up
  translate([0, outer_h, z_top]) rotate([180, 0, 0]) lid(false);
} else if (part == "lid_501") {
  // same as the standard lid, no mini-PIR window
  translate([0, outer_h, z_top]) rotate([180, 0, 0]) lid(false);
} else if (part == "lid_pir") {
  // print orientation, with 13 mm PIR window in the front wall
  translate([0, outer_h, z_top]) rotate([180, 0, 0]) lid(true);
} else if (part == "plunger") {
  // print orientation: base plate on the bed
  translate([0, 0, -switch_top]) plunger();
} else if (part == "mic_tube") {
  // print orientation: flange on the bed, tube standing up
  mic_tube();
} else if (part == "assembly_pir") {
  color("SteelBlue") base();
  color("LightSteelBlue", 0.35) lid(true);
  color("Orange") translate([key1_x, key1_y, 0]) plunger();
} else if (part == "assembly_501") {
  color("SteelBlue") base_501();
  color("LightSteelBlue", 0.35) lid(false);
  color("Orange") translate([key1_x, key1_y, 0]) plunger();
  pir501_mock();
  color("LightSteelBlue") pir501_cover();
} else if (part == "none") {
  // no top-level geometry; used by the joint-check wrapper
} else {
  // assembly preview
  color("SteelBlue") base();
  color("LightSteelBlue", 0.35) lid(false);
  color("Orange") translate([key1_x, key1_y, 0]) plunger();
}
