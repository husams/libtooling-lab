// widget.h — part of the capstone's sample project. Contains, on purpose:
//   * NULL in a constructor (auto-fixable -> nullptr)
//   * Widget::draw() overriding Drawable::draw() without `override`
//   * build_label() declared with 6 parameters (report-only)
// Because this header is included by BOTH translation units (main.cpp and
// widget.cpp), the capstone must de-duplicate findings — see the [dedup] note
// in Part 6.
#ifndef WIDGET_H
#define WIDGET_H

#include <cstddef>
#include <string>

class Drawable {
public:
    virtual ~Drawable() {}
    virtual void draw() const = 0;
};

class Widget : public Drawable {
public:
    Widget() : parent_(NULL) {}          // NULL  -> nullptr
    void draw() const { /* render */ }   // missing `override`
    void set_parent(Widget* p) { parent_ = p; }
private:
    Widget* parent_;
};

// 6 parameters — over the default max of 4.
std::string build_label(const std::string& a, const std::string& b,
                        int x, int y, int z, bool flag);

#endif  // WIDGET_H
