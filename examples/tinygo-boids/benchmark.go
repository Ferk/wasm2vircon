// Interactive controller and deterministic tour for the TinyGo benchmarks.
package main

type benchCase struct{ mode, count int32 }

var benchCases = [6]benchCase{{0, 10}, {0, 50}, {1, 500}, {1, 2500}, {2, 4}, {2, 64}}

type repeatState struct {
	direction      int32
	start, emitted uint32
}
type fpsState struct{ start, completed, hundredths uint32 }

// frameNumber reads the wrapping display-frame counter.
func frameNumber() uint32 { return uint32(virconTimerGetFrameCounter()) }

// resetWorkload initializes a selected workload.
func resetWorkload(mode int32) {
	switch mode {
	case 0:
		initializeBoids()
	case 1:
		resetBunnies()
	case 2:
		resetAlpha()
	}
}

// workloadCount reports the selected workload size.
func workloadCount(mode int32) int32 {
	switch mode {
	case 0:
		return boidCount
	case 1:
		return bunnyCount
	default:
		return alphaCount
	}
}

// setWorkloadCount changes the selected workload size.
func setWorkloadCount(mode, n int32) {
	switch mode {
	case 0:
		setBoidCount(n)
	case 1:
		setBunnyCount(n)
	case 2:
		setAlphaCount(n)
	}
}

// repeatSteps implements frame-based held-button repeat.
func repeatSteps(r *repeatState, d int32, now uint32) int32 {
	if d == 0 {
		r.direction = 0
		return 0
	}
	if d != r.direction {
		*r = repeatState{direction: d, start: now}
		return d
	}
	elapsed := now - r.start
	due := uint32(0)
	if elapsed >= 18 {
		due = 1 + (elapsed-18)/6
	}
	steps := due - r.emitted
	r.emitted = due
	if steps > 100 {
		steps = 100
	}
	return d * int32(steps)
}

// drawHUD displays shared measurements over a translucent background.
func drawHUD(mode, cpu, gpu int32, fps uint32, tour int32) {
	virconGPUSelectTexture(-1)
	virconGPUSelectRegion(256)
	virconGPUSetActiveBlending(0x20)
	virconGPUSetMultiplyColor(-1342177280)
	virconGPUSetDrawingScale(380, 70)
	virconGPUSetDrawingPoint(5, 5)
	virconGPUDrawRegionZoomed()
	virconGPUSetDrawingScale(1, 1)
	virconGPUSetMultiplyColor(colorWhite)
	name := "Boids"
	if mode == 1 {
		name = "BunnyMark"
	} else if mode == 2 {
		name = "Alpha Swarm"
	}
	printAt(10, 10, name)
	printAt(230, 10, "Count:")
	printUIntAt(295, 10, uint32(workloadCount(mode)))
	printAt(10, 30, "FPS:")
	printFixed2At(55, 30, float32(fps)/100)
	printAt(120, 30, "GPU%:")
	printFixed2At(175, 30, float32(gpu)/100)
	printAt(245, 30, "CPU%:")
	printFixed2At(300, 30, float32(cpu)/100)
	if tour != 0 {
		printAt(10, 50, "Benchmark tour:")
		printUIntAt(170, 50, uint32(tour))
		printAt(180, 50, "/6")
	} else {
		printAt(10, 50, "L/R mode  DPAD +/-  START tour")
	}
}

// benchmarkFrame measures a complete update, draw, and display wait.
func benchmarkFrame(mode int32, cpu, gpu *int32, fps *fpsState, tour int32) {
	start := virconTimerGetFrameCounter()
	cycles := virconTimerGetCycleCounter()
	switch mode {
	case 0:
		updateBoids()
	case 1:
		updateBunnies()
	case 2:
		updateAlpha()
	}
	virconSetBackgroundColor(colorBlack)
	if mode == 2 {
		drawHUD(mode, *cpu, *gpu, fps.hundredths, tour)
		drawAlpha()
	} else {
		if mode == 0 {
			drawBoids()
		} else {
			drawBunnies()
		}
		drawHUD(mode, *cpu, *gpu, fps.hundredths, tour)
	}
	*cpu = ((virconTimerGetFrameCounter()-start)*250000 + virconTimerGetCycleCounter() - cycles) / 25
	remaining := virconGPUGetRemainingPixels()
	if remaining < 0 {
		remaining = 0
	}
	if remaining > 2073600 {
		remaining = 2073600
	}
	used := int32(2073600) - remaining
	*gpu = (used/20736)*100 + (used%20736)*100/20736
	virconEndFrame()
	fps.completed++
	now := frameNumber()
	elapsed := now - fps.start
	if elapsed >= 60 {
		fps.hundredths = fps.completed * 6000 / elapsed
		fps.completed = 0
		fps.start = now
	}
}

// runTour reports the six cases through the shared simulator checkpoint format.
func runTour() {
	checksum := int32(0)
	for i := int32(0); i < 6; i++ {
		c := benchCases[i]
		resetWorkload(c.mode)
		setWorkloadCount(c.mode, c.count)
		// Keep setup time and its partial display frame outside the timed case.
		virconEndFrame()
		cpu, gpu := int32(0), int32(0)
		start := frameNumber()
		fps := fpsState{start: start, hundredths: 6000}
		updates, cpus, gpus := uint32(0), uint32(0), uint32(0)
		for {
			benchmarkFrame(c.mode, &cpu, &gpu, &fps, i+1)
			updates++
			cpus += uint32(cpu)
			gpus += uint32(gpu)
			if frameNumber()-start >= 60 {
				break
			}
		}
		n := workloadCount(c.mode)
		averageCPU := cpus / updates
		if averageCPU > 262143 {
			averageCPU = 262143
		}
		virconGPUSetMultiplyColor(int32(gpus/updates | averageCPU<<14))
		virconGPUSetDrawingScale(float32(updates*6000/(frameNumber()-start))/100, 1)
		virconGPUSetDrawingPoint(n%1000, 1001+i+6*(n/1000))
		virconGPUSetMultiplyColor(colorWhite)
		virconGPUSetDrawingScale(1, 1)
		checksum += (i + 1) * (n + 1)
	}
	virconGPUSetDrawingPoint(checksum%997, 6)
}

// runBenchmark owns the interactive workload controller.
func runBenchmark() {
	mode, cpu, gpu := int32(0), int32(0), int32(0)
	virconInputSelectGamepad(0)
	resetWorkload(mode)
	fps := fpsState{start: frameNumber(), hundredths: 6000}
	previousL, previousR, previousStart := false, false, false
	vertical, horizontal := repeatState{}, repeatState{}
	for {
		start := virconInputGamepadStart() > 0
		l := virconInputGamepadL() > 0
		r := virconInputGamepadR() > 0
		if start && !previousStart {
			runTour()
			resetWorkload(mode)
			cpu = 0
			gpu = 0
			fps = fpsState{start: frameNumber(), hundredths: 6000}
			vertical = repeatState{}
			horizontal = repeatState{}
			previousStart = true
			previousL = virconInputGamepadL() > 0
			previousR = virconInputGamepadR() > 0
			continue
		}
		if l && !previousL || r && !previousR {
			if r && !previousR {
				mode = (mode + 1) % 3
			} else {
				mode = (mode + 2) % 3
			}
			resetWorkload(mode)
			cpu = 0
			gpu = 0
			fps = fpsState{start: frameNumber(), hundredths: 6000}
			vertical = repeatState{}
			horizontal = repeatState{}
		}
		up, side := int32(0), int32(0)
		if virconInputGamepadUp() > 0 {
			up = 1
		} else if virconInputGamepadDown() > 0 {
			up = -1
		}
		if virconInputGamepadRight() > 0 {
			side = 1
		} else if virconInputGamepadLeft() > 0 {
			side = -1
		}
		step := int32(10)
		if mode == 1 {
			step = 100
		} else if mode == 2 {
			step = 5
		}
		delta := repeatSteps(&vertical, up, frameNumber())*step + repeatSteps(&horizontal, side, frameNumber())
		if delta != 0 {
			setWorkloadCount(mode, workloadCount(mode)+delta)
		}
		previousL = l
		previousR = r
		previousStart = start
		benchmarkFrame(mode, &cpu, &gpu, &fps, 0)
	}
}
