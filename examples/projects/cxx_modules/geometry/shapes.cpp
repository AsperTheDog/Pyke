module geometry;

import geometry.internal_math;

double area(const Rect& p_rect)
{
    return p_rect.width * p_rect.height;
}

double perimeter(const Rect& p_rect)
{
    return twice(p_rect.width + p_rect.height);
}
