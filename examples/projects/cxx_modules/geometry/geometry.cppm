export module geometry;

export struct Rect
{
    double width;
    double height;
};

export double area(const Rect& p_rect);
export double perimeter(const Rect& p_rect);
