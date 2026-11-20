// Check: the DLL's snow maths against snow_ramps.lua's, on a grid of tiles.
#include <cstdio>
#include <cstdint>
#include <cmath>
static double lattice(int64_t ix, int64_t iy, int64_t seed) {
    uint64_t h = (uint64_t)(ix * 374761393LL + iy * 668265263LL + seed * 2246822519LL) & 0xffffffffull;
    h = ((h ^ (h >> 13)) * 1274126177ull) & 0xffffffffull;
    return (double)((h ^ (h >> 16)) & 0xffff) / 65535.0;
}
static double drift(double x, double y) {
    static const int TILES[2] = {11, 5};
    static const double WEIGHTS[2] = {0.7, 0.3};
    double total = 0;
    for (int n = 0; n < 2; n++) {
        double fx = x / TILES[n], fy = y / TILES[n];
        double ixf = std::floor(fx), iyf = std::floor(fy);
        double tx = fx - ixf, ty = fy - iyf;
        tx = tx * tx * (3 - 2 * tx);
        ty = ty * ty * (3 - 2 * ty);
        int64_t ix = (int64_t)ixf, iy = (int64_t)iyf;
        double a = lattice(ix, iy, n + 1), b = lattice(ix + 1, iy, n + 1);
        double c = lattice(ix, iy + 1, n + 1), d = lattice(ix + 1, iy + 1, n + 1);
        double v = (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
        total += (v * 2 - 1) * WEIGHTS[n];
    }
    return total;
}
static int level_for(double amount, double nx, double ny) {
    const double snowy = 100;
    double a = amount < snowy ? snowy : (amount > 255 ? 255 : amount);
    double v = (a - snowy) / (255 - snowy) + drift(nx, ny) * 0.07;
    return (v >= 0.16) + (v >= 0.39) + (v >= 0.71);
}
int main() {
    for (int x = -2; x < 300; x += 7)
        for (int y = -2; y < 300; y += 11)
            for (int amount = 100; amount <= 255; amount += 31) {
                double l9 = lattice(x, y, 9);
                printf("%d %d %d %d %d %d %d\n", x, y, amount, level_for(amount, x, y),
                       level_for(amount, x - 0.5, y - 0.5), (int)std::floor(l9 * 4) % 4, (int)std::floor(l9 * 2) % 2);
            }
}
