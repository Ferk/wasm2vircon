import * as v from "./vircon";

// Unmanaged records live in fixed linear-memory storage, with no GC or heap.
@unmanaged class Boid {
  x: f32;
  y: f32;
  vx: f32;
  vy: f32;
}
@unmanaged class State { active: i32; random: u32; }
const storage = memory.data(150 * offsetof<Boid>());
const state = changetype<State>(memory.data(offsetof<State>()));

function at(index: i32): Boid {
  return changetype<Boid>(storage + <usize>index * offsetof<Boid>());
}
function randomFloat(minimum: f32, maximum: f32): f32 {
  state.random = state.random * 1664525 + 1013904223;
  return minimum + (maximum - minimum) * (<f32>(state.random & 0xffff) / 65535);
}
function initialize(index: i32): void {
  const b = at(index);
  b.x = randomFloat(100, 540);
  b.y = randomFloat(100, 260);
  b.vx = randomFloat(-2, 2);
  b.vy = randomFloat(-2, 2);
}
export function reset(): void {
  state.random = 1;
  state.active = 50;
  for (let i = 0; i < state.active; ++i) initialize(i);
}
export function count(): i32 { return state.active; }
export function setCount(n: i32): void {
  n = max(0, min(150, n));
  for (let i = state.active; i < n; ++i) initialize(i);
  state.active = n;
}
export function update(): void {
  for (let i = 0; i < state.active; ++i) {
    const b = at(i);
    let centerX: f32 = 0, centerY: f32 = 0;
    let averageVX: f32 = 0, averageVY: f32 = 0;
    let avoidX: f32 = 0, avoidY: f32 = 0;
    let neighbors = 0;
    for (let j = 0; j < state.active; ++j) {
      if (i == j) continue;
      const other = at(j);
      const dx = b.x - other.x, dy = b.y - other.y;
      const distance = dx * dx + dy * dy;
      if (distance < 3600) {
        centerX += other.x; centerY += other.y;
        averageVX += other.vx; averageVY += other.vy;
        ++neighbors;
      }
      if (distance < 400) { avoidX += dx; avoidY += dy; }
    }
    if (neighbors > 0) {
      const n = <f32>neighbors;
      centerX /= n; centerY /= n;
      averageVX /= n; averageVY /= n;
      b.vx += (centerX - b.x) * <f32>0.005;
      b.vy += (centerY - b.y) * <f32>0.005;
      b.vx += (averageVX - b.vx) * <f32>0.05;
      b.vy += (averageVY - b.vy) * <f32>0.05;
    }
    b.vx += avoidX * <f32>0.05;
    b.vy += avoidY * <f32>0.05;
    if (b.x < 50) b.vx += <f32>0.2;
    if (b.x > 590) b.vx -= <f32>0.2;
    if (b.y < 50) b.vy += <f32>0.2;
    if (b.y > 310) b.vy -= <f32>0.2;
    const speedSquared = b.vx * b.vx + b.vy * b.vy;
    if (speedSquared > 16) { b.vx *= <f32>0.9; b.vy *= <f32>0.9; }
    if (speedSquared < 4) { b.vx *= <f32>1.1; b.vy *= <f32>1.1; }
    b.x += b.vx; b.y += b.vy;
  }
}
export function draw(): void {
  v.selectTexture(-1); v.selectRegion(256);
  v.multiplyColor(0xffffff00); v.drawingScale(3, 3);
  for (let i = 0; i < state.active; ++i) {
    const b = at(i);
    v.drawingPoint(<i32>b.x, <i32>b.y); v.drawZoomed();
  }
  v.drawingScale(1, 1); v.multiplyColor(v.WHITE);
}
