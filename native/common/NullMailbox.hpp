/*
 * NULL JSFX - hands an object (a loaded file) to the audio thread without a lock
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * post() (any thread but the audio thread: setState) publishes a new object; take() (the audio
 * thread, at the start of a block) collects the newest one, and giveBack() returns it once the
 * DSP has swapped its data in, holding the old data now. The audio thread never allocates,
 * frees, locks or waits: what comes back is freed by the next post() or by the destructor.
 * If the previous returned object has not been freed yet, the audio thread holds the new one
 * and passes it back at its next take(); it takes nothing new until it could.
 */

#pragma once

#include <atomic>

namespace nulljsfx {

template <typename T>
class Mailbox {
public:
    Mailbox() = default;
    Mailbox(const Mailbox&) = delete;
    Mailbox& operator=(const Mailbox&) = delete;

    ~Mailbox()
    {
        delete fIn.load();
        delete fBack.load();
        delete fHeld;
    }

    // not on the audio thread: free what came back, publish t (an older unread one is freed)
    void post(T* t)
    {
        delete fBack.exchange(nullptr, std::memory_order_acq_rel);
        delete fIn.exchange(t, std::memory_order_acq_rel);
    }

    // audio thread: the newest posted object, or nullptr; pass it to giveBack() when done
    T* take() noexcept
    {
        if (fHeld != nullptr)
        {
            T* empty = nullptr;
            if (!fBack.compare_exchange_strong(empty, fHeld, std::memory_order_acq_rel))
                return nullptr;
            fHeld = nullptr;
        }
        return fIn.exchange(nullptr, std::memory_order_acq_rel);
    }

    // audio thread: the object from take(), to be freed off the audio thread
    void giveBack(T* t) noexcept
    {
        T* empty = nullptr;
        if (!fBack.compare_exchange_strong(empty, t, std::memory_order_acq_rel))
            fHeld = t;
    }

private:
    std::atomic<T*> fIn { nullptr }, fBack { nullptr };
    T* fHeld = nullptr; // audio thread only
};

} // namespace nulljsfx
