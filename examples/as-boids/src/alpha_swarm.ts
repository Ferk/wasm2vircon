import * as v from "./vircon";

@unmanaged class Particle { x: i32; y: i32; dx: i32; dy: i32; angle: f32; }
@unmanaged class State { active: i32; }
const storage = memory.data(96 * offsetof<Particle>());
const state = changetype<State>(memory.data(offsetof<State>()));
const colors = memory.data<i32>([0x70ff4030, 0x7040d060, 0x703060ff, 0x70e0c040]);
function at(i: i32): Particle {
  return changetype<Particle>(storage + <usize>i * offsetof<Particle>());
}
function angle(p: Particle): f32 {
  return v.atan2(<f32>p.dy, <f32>p.dx) - <f32>(3.1415926 / 2);
}
export function reset(): void {
  state.active = 4;
  for (let i = 0; i < 96; ++i) {
    const p = at(i);
    p.x = 114 + (i * 83) % 413; p.y = 194 + (i * 29) % 53;
    p.dx = (i & 1) != 0 ? 3 : -3; p.dy = (i & 2) != 0 ? 2 : -2;
    p.angle = angle(p);
  }
  v.selectTexture(-1); v.selectRegion(118);
  const x = v.regionMinX(), y = v.regionMinY();
  v.selectRegion(257);
  v.regionMinimum(x, y); v.regionMaximum(x + 9, y + 19); v.regionHotspot(x + 5, y + 10);
}
export function count(): i32 { return state.active; }
export function setCount(n: i32): void { state.active = max(0, min(96, n)); }
export function update(): void {
  for (let i = 0; i < state.active; ++i) {
    const p = at(i);
    p.x += p.dx; p.y += p.dy;
    let bounced = false;
    if (p.x < 114 || p.x > 526) { p.x = max(114, min(526, p.x)); p.dx = -p.dx; bounced = true; }
    if (p.y < 194 || p.y > 246) { p.y = max(194, min(246, p.y)); p.dy = -p.dy; bounced = true; }
    if (bounced) p.angle = angle(p);
  }
}
export function draw(): void {
  v.selectTexture(-1); v.selectRegion(257); v.blending(0x20); v.drawingScale(10, 10);
  for (let i = 0; i < state.active; ++i) {
    const p = at(i);
    v.multiplyColor(load<i32>(colors + <usize>((i & 3) * 4)));
    v.drawingAngle(p.angle); v.drawingPoint(p.x, p.y); v.drawRotozoomed();
  }
  v.drawingScale(1, 1); v.drawingAngle(0); v.multiplyColor(v.WHITE);
}
