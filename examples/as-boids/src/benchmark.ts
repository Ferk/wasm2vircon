import * as v from "./vircon";
import * as boids from "./boids";
import * as bunnies from "./bunnies";
import * as alpha from "./alpha_swarm";

enum Mode { Boids, Bunnies, Alpha }
const cases = memory.data<i32>([10, 50, 500, 2500, 4, 64]);

@unmanaged class Repeat {
  direction: i32;
  start: u32;
  emitted: u32;

  steps(direction: i32, now: u32): i32 {
    if (direction == 0) { this.direction = 0; return 0; }
    if (direction != this.direction) {
      this.direction = direction; this.start = now; this.emitted = 0;
      return direction;
    }
    const elapsed = now - this.start;
    const n: u32 = elapsed < 18 ? 0 : 1 + (elapsed - 18) / 6;
    const steps = min(<u32>100, n - this.emitted);
    this.emitted = n;
    return direction * <i32>steps;
  }
}
@unmanaged class Metrics {
  cpu: u32;
  gpu: u32;
  start: u32;
  completed: u32;
  hundredths: u32;

  reset(): void {
    this.cpu = 0; this.gpu = 0;
    this.start = v.frameCounter(); this.completed = 0; this.hundredths = 6000;
  }
}
const metrics = changetype<Metrics>(memory.data(offsetof<Metrics>()));
const vertical = changetype<Repeat>(memory.data(offsetof<Repeat>()));
const horizontal = changetype<Repeat>(memory.data(offsetof<Repeat>()));

function reset(mode: Mode): void {
  if (mode == Mode.Boids) boids.reset();
  else if (mode == Mode.Bunnies) bunnies.reset();
  else alpha.reset();
}
function count(mode: Mode): i32 {
  if (mode == Mode.Boids) return boids.count();
  if (mode == Mode.Bunnies) return bunnies.count();
  return alpha.count();
}
function setCount(mode: Mode, n: i32): void {
  if (mode == Mode.Boids) boids.setCount(n);
  else if (mode == Mode.Bunnies) bunnies.setCount(n);
  else alpha.setCount(n);
}
function hud(mode: Mode, index: i32): void {
  v.selectTexture(-1); v.selectRegion(256); v.blending(0x20);
  v.multiplyColor(0xb0000000); v.drawingScale(380, 70);
  v.drawingPoint(5, 5); v.drawZoomed();
  v.drawingScale(1, 1); v.multiplyColor(v.WHITE);
  v.printAt(10, 10, mode == Mode.Boids ? "Boids" : mode == Mode.Bunnies ? "BunnyMark" : "Alpha Swarm");
  v.printAt(230, 10, "Count:"); v.printUIntAt(295, 10, <u32>count(mode));
  v.printAt(10, 30, "FPS:"); v.printHundredthsAt(55, 30, metrics.hundredths);
  v.printAt(120, 30, "GPU%:"); v.printHundredthsAt(175, 30, metrics.gpu);
  v.printAt(245, 30, "CPU%:"); v.printHundredthsAt(300, 30, metrics.cpu);
  if (index != 0) {
    v.printAt(10, 50, "Benchmark tour:"); v.printUIntAt(170, 50, <u32>index); v.printAt(180, 50, "/6");
  } else v.printAt(10, 50, "L/R mode  DPAD +/-  START tour");
}
function frame(mode: Mode, index: i32): void {
  const start = v.frameCounter(), cycles = v.cycleCounter();
  if (mode == Mode.Boids) boids.update();
  else if (mode == Mode.Bunnies) bunnies.update();
  else alpha.update();
  v.clearScreen(v.BLACK);
  // The high alpha load can consume the GPU budget: keep its HUD visible.
  if (mode == Mode.Alpha) { hud(mode, index); alpha.draw(); }
  else {
    if (mode == Mode.Boids) boids.draw(); else bunnies.draw();
    hud(mode, index);
  }
  metrics.cpu = ((v.frameCounter() - start) * 250000 + <u32>(v.cycleCounter() - cycles)) / 25;
  const used = 2073600 - max(0, min(2073600, v.remainingPixels()));
  metrics.gpu = <u32>((used / 20736) * 100 + (used % 20736) * 100 / 20736);
  v.endFrame();
  ++metrics.completed;
  const now = v.frameCounter(), elapsed = now - metrics.start;
  if (elapsed >= 60) {
    metrics.hundredths = metrics.completed * 6000 / elapsed;
    metrics.completed = 0; metrics.start = now;
  }
}
function tour(): void {
  let checksum = 0;
  for (let i = 0; i < 6; ++i) {
    const mode = <Mode>(i / 2);
    reset(mode); setCount(mode, load<i32>(cases + <usize>(i * 4)));
    // Exclude setup and its partial frame from the one-second measurement.
    v.endFrame(); metrics.reset();
    const start = v.frameCounter();
    let updates: u32 = 0, cpus: u32 = 0, gpus: u32 = 0;
    do {
      frame(mode, i + 1); ++updates;
      cpus += metrics.cpu; gpus += metrics.gpu;
    } while (v.frameCounter() - start < 60);
    const n = count(mode);
    const averageCPU = min(<u32>262143, cpus / updates);
    v.multiplyColor(<i32>((gpus / updates) | (averageCPU << 14)));
    v.drawingScale(<f32>(updates * 6000 / (v.frameCounter() - start)) / 100, 1);
    v.drawingPoint(n % 1000, 1001 + i + 6 * (n / 1000));
    v.multiplyColor(v.WHITE); v.drawingScale(1, 1);
    checksum += (i + 1) * (n + 1);
  }
  v.drawingPoint(checksum % 997, 6);
}

export function run(): void {
  let mode = Mode.Boids;
  let previousL = false, previousR = false, previousStart = false;
  v.selectGamepad(0); reset(mode); metrics.reset();
  while (true) {
    const start = v.buttonStart() > 0, l = v.buttonL() > 0, r = v.buttonR() > 0;
    if (start && !previousStart) {
      tour(); reset(mode); metrics.reset();
      vertical.direction = 0; horizontal.direction = 0;
      previousL = v.buttonL() > 0; previousR = v.buttonR() > 0; previousStart = true;
      continue;
    }
    if ((l && !previousL) || (r && !previousR)) {
      mode = <Mode>((<i32>mode + (r && !previousR ? 1 : 2)) % 3);
      reset(mode); metrics.reset(); vertical.direction = 0; horizontal.direction = 0;
    }
    const up = v.up() > 0 ? 1 : v.down() > 0 ? -1 : 0;
    const side = v.right() > 0 ? 1 : v.left() > 0 ? -1 : 0;
    const step = mode == Mode.Boids ? 10 : mode == Mode.Bunnies ? 100 : 5;
    const now = v.frameCounter();
    const delta = vertical.steps(up, now) * step + horizontal.steps(side, now);
    if (delta != 0) setCount(mode, count(mode) + delta);
    previousL = l; previousR = r; previousStart = start;
    frame(mode, 0);
  }
}
