// main.go is deliberately small: it provides TinyGo's package entry and the
// explicit cartridge export, while boids.go owns all benchmark behavior.
package main

// main satisfies TinyGo's package-main requirement without becoming the
// cartridge entry selected by wasm2vircon.
func main() {}

//go:export vircon_main
// Owns the benchmark lifetime and intentionally never returns.
func virconMain() {
	previousUpPressed := false
	previousDownPressed := false
	initializeBoids()

	for {
		virconInputSelectGamepad(0)
		upPressed := virconInputGamepadUp() > 0
		downPressed := virconInputGamepadDown() > 0
		if upPressed && !previousUpPressed {
			increaseBoidCount()
		} else if downPressed && !previousDownPressed {
			decreaseBoidCount()
		}
		previousUpPressed = upPressed
		previousDownPressed = downPressed

		updateStartFrame := virconTimerGetFrameCounter()
		updateBoids()
		updateFrames := virconTimerGetFrameCounter() - updateStartFrame + 1
		fps := float32(framesPerSecond) / float32(updateFrames)

		virconSetBackgroundColor(colorBlack)
		drawBoids()
		printAt(10, 10, "Boids:")
		printUIntAt(80, 10, uint32(boidCount))
		printAt(120, 10, "(up/down +/-10)")
		printAt(10, 30, "Update frames:")
		printUIntAt(160, 30, uint32(updateFrames))
		printAt(10, 50, "FPS:")
		printFixed2At(60, 50, fps)
		virconEndFrame()
	}
}
