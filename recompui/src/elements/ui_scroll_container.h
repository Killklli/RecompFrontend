#pragma once

#include "ui_element.h"
#include <functional>

namespace recompui {

    enum class ScrollDirection {
        Horizontal,
        Vertical
    };

    class ScrollContainer : public Element {
    protected:
        std::string_view get_type_name() override { return "ScrollContainer"; }
        void process_event(const Event &e) override;
    public:
        ScrollContainer(ResourceId rid, Element *parent, ScrollDirection direction);
        void set_scroll_callback(std::function<void()> callback);
    private:
        std::function<void()> scroll_callback;
    };

} // namespace recompui
