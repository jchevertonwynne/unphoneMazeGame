#include "Rectangle.h"

Rectangle::Rectangle(int a, int b, int w, int h)
{
    x = a;
    y = b;
    width = w;
    height = h;
}

// for two rectangles to intersect one has to have at least one corner within the other rectangle
bool Rectangle::intersecting(Rectangle other)
{
    return pointInRect(other.getX(), other.getY()) || 
           pointInRect(other.getX() + other.getWidth() - 1, other.getY()) ||
           pointInRect(other.getX() + other.getWidth() - 1, other.getY() + other.getHeight() - 1) ||
           pointInRect(other.getX(), other.getY() + other.getHeight() - 1) ||
           other.pointInRect(x, y) ||
           other.pointInRect(x + width - 1, y) ||
           other.pointInRect(x + width - 1, y + height - 1) ||
           other.pointInRect(x, y + height - 1);
}

bool Rectangle::pointInRect(int a, int b) 
{
    return a >= x && a < x + width && b >= y && b < y + height;
}

int Rectangle::getX() 
{
    return x;
}

int Rectangle::getY()
{
    return y;
}

void Rectangle::setX(int newX) 
{
    x = newX;
}

void Rectangle::setY(int newY)
{
    y = newY;
}

int Rectangle::getWidth()
{
    return width;
}

int Rectangle::getHeight() 
{
    return height;
}