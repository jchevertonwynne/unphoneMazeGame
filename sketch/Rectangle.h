#ifndef RECTANGLE_H
#define RECTANGLE_H

class Rectangle
{
    private:
        int x, y, width, height;
    public:
        Rectangle(int, int, int, int);
        bool intersecting(Rectangle);
        bool pointInRect(int, int);
        int getX();
        int getY();
        int getWidth();
        int getHeight();
        void setX(int);
        void setY(int);
};

#endif