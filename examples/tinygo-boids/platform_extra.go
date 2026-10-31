// Additional hardware ports used by the benchmark workloads.
package main

//go:wasmimport env vircon_gpu_get_remaining_pixels
func virconGPUGetRemainingPixels() int32

//go:wasmimport env vircon_gpu_get_region_min_x
func virconGPUGetRegionMinX() int32

//go:wasmimport env vircon_gpu_get_region_min_y
func virconGPUGetRegionMinY() int32

//go:wasmimport env vircon_gpu_set_region_minimum
func virconGPUSetRegionMinimum(x, y int32)

//go:wasmimport env vircon_gpu_set_region_maximum
func virconGPUSetRegionMaximum(x, y int32)

//go:wasmimport env vircon_gpu_set_region_hotspot
func virconGPUSetRegionHotspot(x, y int32)

//go:wasmimport env vircon_gpu_set_active_blending
func virconGPUSetActiveBlending(mode int32)

//go:wasmimport env vircon_gpu_set_drawing_angle
func virconGPUSetDrawingAngle(angle float32)

//go:wasmimport env vircon_gpu_draw_region_rotozoomed
func virconGPUDrawRegionRotozoomed()

//go:wasmimport env vircon_cpu_atan2
func virconCPUAtan2(y, x float32) float32

//go:wasmimport env vircon_input_gamepad_left
func virconInputGamepadLeft() int32

//go:wasmimport env vircon_input_gamepad_right
func virconInputGamepadRight() int32

//go:wasmimport env vircon_input_gamepad_button_l
func virconInputGamepadL() int32

//go:wasmimport env vircon_input_gamepad_button_r
func virconInputGamepadR() int32

//go:wasmimport env vircon_input_gamepad_button_start
func virconInputGamepadStart() int32
