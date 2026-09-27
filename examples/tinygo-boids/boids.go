// boids.go contains the benchmark and its direct Vircon32 platform bindings.
// It intentionally uses neither the standard library nor heap allocation.
package main

const (
	maxBoidCount     int32   = 150
	initialBoidCount int32   = 50
	boidCountStep    int32   = 10
	width            float32 = 640.0
	height           float32 = 360.0
	viewRange        float32 = 60.0
	avoidRange       float32 = 20.0
	centeringFactor  float32 = 0.005
	matchingFactor   float32 = 0.05
	avoidFactor      float32 = 0.05
	maxSpeed         float32 = 4.0
	minSpeed         float32 = 2.0
	margin           float32 = 50.0
	turnFactor       float32 = 0.2
	framesPerSecond  int32   = 60
	colorBlack       int32   = -16777216 // 0xFF000000
	colorWhite       int32   = -1        // 0xFFFFFFFF
	colorCyan        int32   = -256      // 0xFFFFFF00
)

// The following declarations are narrow, hardware-like Wasm imports. Their
// ordinary Go wrappers below deliberately contain all higher-level behavior.

//go:wasmimport env vircon_set_background_color
func virconSetBackgroundColor(color int32)

//go:wasmimport env vircon_end_frame
func virconEndFrame()

//go:wasmimport env vircon_gpu_get_selected_texture
func virconGPUGetSelectedTexture() int32

//go:wasmimport env vircon_gpu_select_texture
func virconGPUSelectTexture(texture int32)

//go:wasmimport env vircon_gpu_select_region
func virconGPUSelectRegion(region int32)

//go:wasmimport env vircon_gpu_set_drawing_point
func virconGPUSetDrawingPoint(x, y int32)

//go:wasmimport env vircon_gpu_set_multiply_color
func virconGPUSetMultiplyColor(color int32)

//go:wasmimport env vircon_gpu_set_drawing_scale
func virconGPUSetDrawingScale(x, y float32)

//go:wasmimport env vircon_gpu_draw_region
func virconGPUDrawRegion()

//go:wasmimport env vircon_gpu_draw_region_zoomed
func virconGPUDrawRegionZoomed()

//go:wasmimport env vircon_input_select_gamepad
func virconInputSelectGamepad(gamepad int32)

//go:wasmimport env vircon_input_gamepad_up
func virconInputGamepadUp() int32

//go:wasmimport env vircon_input_gamepad_down
func virconInputGamepadDown() int32

//go:wasmimport env vircon_timer_get_frame_counter
func virconTimerGetFrameCounter() int32

// Boid stores the position and velocity of one simulated flock member.
type Boid struct {
	x  float32
	y  float32
	vx float32
	vy float32
}

var boids [maxBoidCount]Boid
var boidCount = initialBoidCount
var randomState uint32 = 1

// randomU32 advances the benchmark's deterministic local random generator.
func randomU32() uint32 {
	randomState = randomState*1664525 + 1013904223
	return randomState
}

// randomFloat produces a deterministic float within the requested interval.
func randomFloat(min, max float32) float32 {
	fraction := float32(randomU32()&0xffff) / 65535.0
	return min + (max-min)*fraction
}

// initializeBoid initializes one newly enabled boid.
func initializeBoid(index int32) {
	boids[index].x = randomFloat(100.0, width-100.0)
	boids[index].y = randomFloat(100.0, height-100.0)
	boids[index].vx = randomFloat(-2.0, 2.0)
	boids[index].vy = randomFloat(-2.0, 2.0)
}

// initializeBoids initializes the visible starting workload.
func initializeBoids() {
	for index := int32(0); index < boidCount; index++ {
		initializeBoid(index)
	}
}

// increaseBoidCount adds one fixed batch without exceeding capacity.
func increaseBoidCount() {
	target := boidCount + boidCountStep
	if target > maxBoidCount {
		target = maxBoidCount
	}
	for boidCount < target {
		initializeBoid(boidCount)
		boidCount++
	}
}

// decreaseBoidCount removes one batch while keeping a visible flock.
func decreaseBoidCount() {
	if boidCount > boidCountStep {
		boidCount -= boidCountStep
	} else {
		boidCount = 10
	}
}

// updateBoids runs one deliberate O(N²) flocking simulation step.
func updateBoids() {
	for i := int32(0); i < boidCount; i++ {
		var centerX, centerY float32
		var averageVX, averageVY float32
		var avoidX, avoidY float32
		var neighbors int32

		for j := int32(0); j < boidCount; j++ {
			if i == j {
				continue
			}

			current := &boids[i]
			other := &boids[j]
			dx := current.x - other.x
			dy := current.y - other.y
			distanceSquared := dx*dx + dy*dy

			if distanceSquared < viewRange*viewRange {
				centerX += other.x
				centerY += other.y
				averageVX += other.vx
				averageVY += other.vy
				neighbors++
			}
			if distanceSquared < avoidRange*avoidRange {
				avoidX += dx
				avoidY += dy
			}
		}

		current := &boids[i]
		if neighbors > 0 {
			neighborCount := float32(neighbors)
			centerX /= neighborCount
			centerY /= neighborCount
			averageVX /= neighborCount
			averageVY /= neighborCount
			current.vx += (centerX - current.x) * centeringFactor
			current.vy += (centerY - current.y) * centeringFactor
			current.vx += (averageVX - current.vx) * matchingFactor
			current.vy += (averageVY - current.vy) * matchingFactor
		}

		current.vx += avoidX * avoidFactor
		current.vy += avoidY * avoidFactor
		if current.x < margin {
			current.vx += turnFactor
		}
		if current.x > width-margin {
			current.vx -= turnFactor
		}
		if current.y < margin {
			current.vy += turnFactor
		}
		if current.y > height-margin {
			current.vy -= turnFactor
		}

		speedSquared := current.vx*current.vx + current.vy*current.vy
		if speedSquared > maxSpeed*maxSpeed {
			current.vx *= 0.9
			current.vy *= 0.9
		}
		if speedSquared < minSpeed*minSpeed {
			current.vx *= 1.1
			current.vy *= 1.1
		}
		current.x += current.vx
		current.y += current.vy
	}
}

// drawGlyph draws one BIOS-font glyph at a screen coordinate.
func drawGlyph(x, y int32, glyph uint8) {
	virconGPUSelectRegion(int32(glyph))
	virconGPUSetDrawingPoint(x, y)
	virconGPUDrawRegion()
}

// printAt draws a CP-1252 byte string using the BIOS font texture.
func printAt(initialX, initialY int32, text string) {
	previousTexture := virconGPUGetSelectedTexture()
	x, y := initialX, initialY
	virconGPUSelectTexture(-1)
	for index := 0; index < len(text); index++ {
		glyph := text[index]
		drawGlyph(x, y, glyph)
		x += 10
		if glyph == '\n' {
			x = initialX
			y += 20
		}
	}
	virconGPUSelectTexture(previousTexture)
}

// printDigits recursively emits the digits of an unsigned decimal value.
func printDigits(x, y int32, value uint32) int32 {
	quotient := value / 10
	if quotient != 0 {
		x = printDigits(x, y, quotient)
	}
	drawGlyph(x, y, uint8('0'+value-quotient*10))
	return x + 10
}

// printUIntAt prints a compact unsigned decimal value with the BIOS font.
func printUIntAt(x, y int32, value uint32) {
	printDigits(x, y, value)
}

// printFixed2At prints a float with two decimal places without using fmt.
func printFixed2At(initialX, y int32, value float32) {
	x := initialX
	hundredths := int32(value * 100.0)
	if hundredths < 0 {
		drawGlyph(x, y, '-')
		x += 10
		hundredths = -hundredths
	}
	magnitude := uint32(hundredths)
	printUIntAt(x, y, magnitude/100)
	drawGlyph(x+20, y, '.')
	drawGlyph(x+30, y, uint8('0'+(magnitude/10)%10))
	drawGlyph(x+40, y, uint8('0'+magnitude%10))
}

// drawBoids draws every boid as a small cyan BIOS-pixel square.
func drawBoids() {
	virconGPUSelectTexture(-1)
	virconGPUSelectRegion(256)
	virconGPUSetMultiplyColor(colorCyan)
	virconGPUSetDrawingScale(3.0, 3.0)
	for index := int32(0); index < boidCount; index++ {
		boid := &boids[index]
		virconGPUSetDrawingPoint(int32(boid.x), int32(boid.y))
		virconGPUDrawRegionZoomed()
	}
	virconGPUSetDrawingScale(1.0, 1.0)
	virconGPUSetMultiplyColor(colorWhite)
}
