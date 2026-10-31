// TinyGo cartridge entry for the shared benchmark suite.
package main

// main satisfies the TinyGo package entry requirement.
func main() {}

// virconMain enters the benchmark controller.
//
//go:export vircon_main
func virconMain() { runBenchmark() }
