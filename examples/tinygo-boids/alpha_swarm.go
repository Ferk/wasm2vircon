// Large translucent rotating BIOS glyphs for GPU load measurement.
package main

type particle struct {
	x, y, dx, dy int32
	angle        float32
}

var particles [96]particle
var alphaCount int32

// particleAngle points the BIOS v glyph in the direction of travel.
func particleAngle(p *particle) float32 {
	return virconCPUAtan2(float32(p.dy), float32(p.dx)) - 3.1415926/2
}

// resetAlpha restores particles and copies the selected BIOS font region.
func resetAlpha() {
	alphaCount = 4
	for i := int32(0); i < 96; i++ {
		p := &particles[i]
		p.x = 114 + (i*83)%413
		p.y = 194 + (i*29)%53
		p.dx = -3
		p.dy = -2
		if i&1 != 0 {
			p.dx = 3
		}
		if i&2 != 0 {
			p.dy = 2
		}
		p.angle = particleAngle(p)
	}
	virconGPUSelectTexture(-1)
	virconGPUSelectRegion('v')
	x := virconGPUGetRegionMinX()
	y := virconGPUGetRegionMinY()
	virconGPUSelectRegion(257)
	virconGPUSetRegionMinimum(x, y)
	virconGPUSetRegionMaximum(x+9, y+19)
	virconGPUSetRegionHotspot(x+5, y+10)
}

// setAlphaCount selects the number of layers.
func setAlphaCount(n int32) {
	if n < 0 {
		n = 0
	}
	if n > 96 {
		n = 96
	}
	alphaCount = n
}

// updateAlpha bounces layers and updates their angle.
func updateAlpha() {
	for i := int32(0); i < alphaCount; i++ {
		p := &particles[i]
		p.x += p.dx
		p.y += p.dy
		b := false
		if p.x < 114 {
			p.x = 114
			p.dx = -p.dx
			b = true
		} else if p.x > 526 {
			p.x = 526
			p.dx = -p.dx
			b = true
		}
		if p.y < 194 {
			p.y = 194
			p.dy = -p.dy
			b = true
		} else if p.y > 246 {
			p.y = 246
			p.dy = -p.dy
			b = true
		}
		if b {
			p.angle = particleAngle(p)
		}
	}
}

// drawAlpha renders the layers and restores GPU state.
func drawAlpha() {
	colors := [4]uint32{0x70ff4030, 0x7040d060, 0x703060ff, 0x70e0c040}
	virconGPUSelectTexture(-1)
	virconGPUSelectRegion(257)
	virconGPUSetActiveBlending(0x20)
	virconGPUSetDrawingScale(10, 10)
	for i := int32(0); i < alphaCount; i++ {
		p := &particles[i]
		virconGPUSetMultiplyColor(int32(colors[i&3]))
		virconGPUSetDrawingAngle(p.angle)
		virconGPUSetDrawingPoint(p.x, p.y)
		virconGPUDrawRegionRotozoomed()
	}
	virconGPUSetDrawingScale(1, 1)
	virconGPUSetDrawingAngle(0)
	virconGPUSetMultiplyColor(colorWhite)
}
