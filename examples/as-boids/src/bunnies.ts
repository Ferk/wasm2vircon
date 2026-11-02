import * as v from "./vircon";

@unmanaged class Bunny { x: f32; y: f32; dx: f32; dy: f32; }
@unmanaged class State { active: i32; initialized: i32; }
const storage = memory.data(9999 * offsetof<Bunny>());
const state = changetype<State>(memory.data(offsetof<State>()));
function at(i: i32): Bunny {
  return changetype<Bunny>(storage + <usize>i * offsetof<Bunny>());
}
function initialize(i: i32): void {
  const b = at(i);
  b.x = 10 + <f32>((i * 13) % 600);
  b.y = 20 + <f32>((i * 17) % 300);
  b.dx = <f32>((i & 7) - 3) * <f32>0.5;
  b.dy = <f32>((i & 3) - 2) * <f32>0.5;
}
export function reset(): void {
  for (let i = 0; i < 500; ++i) initialize(i);
  state.initialized = 500;
  state.active = 100;
}
export function count(): i32 { return state.active; }
export function setCount(n: i32): void {
  n = max(0, min(9999, n));
  while (state.initialized < n) { initialize(state.initialized); ++state.initialized; }
  state.active = n;
}
export function update(): void {
  for (let i = 0; i < state.active; ++i) {
    const b = at(i);
    b.x += b.dx; b.y += b.dy;
    if (b.x < 0 || b.x > 629) b.dx = -b.dx;
    if (b.y < 0 || b.y > 339) b.dy = -b.dy;
  }
}
export function draw(): void {
  v.selectTexture(-1); v.selectRegion(98); v.multiplyColor(0xff808080);
  for (let i = 0; i < state.active; ++i) {
    const b = at(i);
    v.drawingPoint(<i32>b.x, <i32>b.y); v.drawRegion();
  }
  v.multiplyColor(v.WHITE);
}
