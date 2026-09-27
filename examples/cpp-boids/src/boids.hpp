#ifndef CPP_BOIDS_BOIDS_HPP
#define CPP_BOIDS_BOIDS_HPP

/* The benchmark module deliberately exposes only its frame-level operations.
 * main.cpp owns input, timing and the small status display. */
namespace boids {

void initialize();
void increase();
void decrease();
void update();
void draw();
int count();

} // namespace boids

#endif
