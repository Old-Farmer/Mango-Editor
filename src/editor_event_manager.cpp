#include "editor_event_manager.h"

namespace mango {

EditorEventHandlerID EditorEventManager::AddHandler(
    EditorEvent event, EditorEventHandler handler) {
    auto& h = handlers_[static_cast<size_t>(event)];
    for (size_t i = 0; i < h.size(); i++) {
        if (h[i].handler == nullptr) {
            h[i].handler = std::move(handler);
            return {i, h[i].generation};
        }
    }
    h.push_back({std::move(handler), 0});
    return {h.size() - 1, 0};
}

void EditorEventManager::RemoveHandler(EditorEvent event,
                                       EditorEventHandlerID id) {
    auto& h = handlers_[static_cast<size_t>(event)];
    if (h.size() <= id.id) {
        return;
    }

    if (h[id.id].handler == nullptr) {
        return;
    }
    if (h[id.id].generation != id.generation) {
        return;
    }
    h[id.id].handler = nullptr;
    h[id.id].generation++;
}

void EditorEventManager::EmitEvent(EditorEvent event, void* arg) {
    for (const auto& callback_store : handlers_[static_cast<size_t>(event)]) {
        if (callback_store.handler != nullptr) {
            callback_store.handler(arg);
        }
    }
}
}  // namespace mango
