// BMO PLAYER - OpenSCAD mirror of bmo_case.py  (by @frandorturtles)
// STATUS: written to match the verified Python model, but NOT compiled in the environment
// where the project was generated (OpenSCAD was unavailable). The STL files in ../STL come
// from bmo_case.py. If this file and the STLs disagree, trust the STLs and report it.
// Usage: set part = "front" | "back" | "all" and render (F6) -> export STL.
part = "all";
$fn = 48;
W=76; H=110; T=26; R=8; WALL=2; SPLIT=13; RE=1;
DISP=[38,76.5]; WIN=[44,33]; BEZEL=[58,41]; 
KEYS_SQ=[[22,45],[22,23],[11,34],[33,34]]; TRI=[52,44]; GREEN=[64,33]; RED=[54,20];
CLOSE=[[8,8],[68,8],[8,102],[68,102]];
module rr(w,h,r){ translate([-w/2+r,-h/2+r]) offset(r=r) square([w-2*r,h-2*r]); }
module body(){ hull(){ for(z=[RE,T-RE]) translate([W/2,H/2,z]) minkowski(){ linear_extrude(0.01) rr(W-2*RE,H-2*RE,R-RE); sphere(RE,$fn=16);} } }
module cavity(){ translate([W/2,H/2,WALL]) linear_extrude(T-2*WALL) rr(W-2*WALL,H-2*WALL,R-WALL); }
module openings(){
  translate([21.6-4.8,-2,12.2-2]) cube([9.6,7.5,4]);                  // USB-C
  translate([54.05,-2,14]) rotate([-90,0,0]) cylinder(d=8.2,h=7.5);   // jack
  translate([W-5,45.5-6.4,12-1.4]) cube([6.5,12.8,2.8]);              // microSD
  translate([-2,76.5-5.2,10.8-2.6]) cube([7,10.4,5.2]);               // micro-USB
  translate([-2,36-3.6,11-1.3]) cube([6,7.2,2.6]);                    // slide switch
}
module front(){
  difference(){
    union(){
      intersection(){ difference(){ body(); cavity(); } translate([-5,-5,-5]) cube([W+10,H+20,SPLIT+5]); }
      translate([63,112.5,0]) cylinder(r=5,h=3.2);                                  // key-ring eyelet
      for(p=CLOSE) translate([p[0],p[1],WALL-.5]) cylinder(r=3.6,h=SPLIT-WALL+.5);  // screw bosses
      translate([DISP[0]-27.6,DISP[1]-19.6,1.5]) cube([55.2,39.2,5.7]);             // display pocket walls
    }
    for(p=CLOSE) translate([p[0],p[1],3.5]) cylinder(d=1.8,h=SPLIT);
    translate([DISP[0]-26.4,DISP[1]-18.4,WALL]) cube([52.8,36.8,7]);                // pocket
    translate([63,112.5,-1]) cylinder(r=2.3,h=5);
    translate([DISP[0],DISP[1],-1]) linear_extrude(WALL+2) rr(WIN[0],WIN[1],2);     // window
    translate([DISP[0],DISP[1],-1]) linear_extrude(1.6) rr(BEZEL[0],BEZEL[1],5);    // bezel recess
    for(k=KEYS_SQ) translate([k[0],k[1],-1]) linear_extrude(4) rr(8.5,8.5,.6);
    translate([GREEN[0],GREEN[1],-1]) cylinder(d=8.5,h=4);
    translate([RED[0],RED[1],-1]) cylinder(d=10.5,h=4);
    translate([TRI[0],TRI[1],-1]) linear_extrude(4) polygon([for(a=[90,210,330]) 7.3*[cos(a),sin(a)]]);
    translate([38,9,-1]) linear_extrude(1.4) text("BMO",size=5,halign="center",font="Liberation Sans:style=Bold");
    openings();
  }
}
module back(){
  difference(){
    union(){
      intersection(){ difference(){ body(); cavity(); } translate([-5,-5,SPLIT]) cube([W+10,H+10,T]); }
      difference(){ translate([W/2,H/2,SPLIT-1.8]) linear_extrude(2.2) rr(W-2*WALL-.4,H-2*WALL-.4,R-WALL-.2);
                    translate([W/2,H/2,SPLIT-2]) linear_extrude(3) rr(W-2*WALL-2.4,H-2*WALL-2.4,R-WALL-1.2); }
      for(p=CLOSE) translate([p[0],p[1],SPLIT]) cylinder(r=3.6,h=T-WALL-SPLIT+.5);
      for(y=[68,85]) translate([20,y-.8,13.9]) cube([24,1.6,T-WALL-13.4]);
    }
    for(p=CLOSE){ translate([p[0],p[1],SPLIT-.5]) cylinder(d=2.4,h=T); translate([p[0],p[1],T-WALL+.8]) cylinder(d=4.2,h=3); }
    openings();
    // reads correctly when looking at the back
    translate([W/2,62,T-.6]) mirror([1,0,0]) linear_extrude(1) text("BMO PLAYER",size=6,halign="center",font="Liberation Sans:style=Bold");
    translate([W/2,44,T-.6]) mirror([1,0,0]) linear_extrude(1) text("@frandorturtles",size=4.4,halign="center",font="Liberation Sans:style=Bold");
  }
}
if(part=="front"||part=="all") front();
if(part=="back"||part=="all") translate([part=="all"?W+10:0,0,0]) back();
