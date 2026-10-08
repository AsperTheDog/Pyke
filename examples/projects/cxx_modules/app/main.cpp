#include <cstdio>

import geometry;

int main()
{
    const Rect l_rect{3.0, 4.0};
    std::printf("area=%.1f perimeter=%.1f\n", area(l_rect), perimeter(l_rect));
    return (area(l_rect) == 12.0 && perimeter(l_rect) == 14.0) ? 0 : 1;
}
