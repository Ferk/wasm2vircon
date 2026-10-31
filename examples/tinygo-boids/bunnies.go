// BunnyMark workload using deterministic moving BIOS glyphs.
package main

type bunny struct{ x, y, dx, dy float32 }

var bunnies [9999]bunny
var bunnyCount, initializedBunnies int32

// initializeBunny assigns a reproducible position and velocity.
func initializeBunny(i int32) {
	bunnies[i] = bunny{float32(10 + (i*13)%600), float32(20 + (i*17)%300), float32((i&7)-3) * 0.5, float32((i&3)-2) * 0.5}
}

// resetBunnies restores the starting workload.
func resetBunnies() {
	for i := int32(0); i < 500; i++ {
		initializeBunny(i)
	}
	initializedBunnies = 500
	bunnyCount = 100
}

// setBunnyCount initializes newly enabled sprites and clamps capacity.
func setBunnyCount(n int32) {
	if n < 0 {
		n = 0
	}
	if n > 9999 {
		n = 9999
	}
	for initializedBunnies < n {
		initializeBunny(initializedBunnies)
		initializedBunnies++
	}
	bunnyCount = n
}

// updateBunnies advances and bounces the sprites.
func updateBunnies() {
	for i := int32(0); i < bunnyCount; i++ {
		b := &bunnies[i]
		b.x += b.dx
		b.y += b.dy
		if b.x < 0 || b.x > 629 {
			b.dx = -b.dx
		}
		if b.y < 0 || b.y > 339 {
			b.dy = -b.dy
		}
	}
}

// drawBunnies renders gray BIOS glyphs.
func drawBunnies() {
	virconGPUSelectTexture(-1)
	virconGPUSelectRegion('b')
	virconGPUSetMultiplyColor(-8355712)
	for i := int32(0); i < bunnyCount; i++ {
		virconGPUSetDrawingPoint(int32(bunnies[i].x), int32(bunnies[i].y))
		virconGPUDrawRegion()
	}
	virconGPUSetMultiplyColor(colorWhite)
}
