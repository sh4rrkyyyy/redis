#pragma once

namespace cactus {

template <class Tag>
struct ListElement {
    ListElement(ListElement&&) = delete;

    ListElement* prev;
    ListElement* next;

    ListElement() : prev(this), next(this) {
    }

    ~ListElement() {
        Unlink();
    }

    void Attach(ListElement* other) {
        other->next = next;
        other->prev = this;

        next->prev = other;
        next = other;
    }

    void Unlink() {
        prev->next = next;
        next->prev = prev;

        prev = next = this;
    }

    bool IsLinked() {
        return prev != this;
    }

    template <class T>
    T* As() {
        return static_cast<T*>(this);
    }
};

template <class Tag>
class List {
public:
    bool IsEmpty() {
        return !list_head_.IsLinked();
    }

    ListElement<Tag>* Begin() {
        return list_head_.next;
    }

    ListElement<Tag>* Back() {
        return list_head_.prev;
    }

    ListElement<Tag>* End() {
        return &list_head_;
    }

    void PushBack(ListElement<Tag>* element) {
        list_head_.prev->Attach(element);
    }

    void Erase(ListElement<Tag>* element) {
        element->Unlink();
    }

private:
    ListElement<Tag> list_head_;
};

}  // namespace cactus
