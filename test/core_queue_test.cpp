#include "core/async_transport_session.hpp"

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace {

std::atomic<bool> g_count_allocations = false;
std::atomic<std::size_t> g_allocation_count = 0;

void* allocate(std::size_t size) {
    if (g_count_allocations.load(std::memory_order_relaxed)) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }

    if (void* ptr = std::malloc(size)) {
        return ptr;
    }
    throw std::bad_alloc();
}

} // namespace

void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

namespace {

struct Message {
    std::uint32_t sequence = 0;
    std::array<char, 32> payload {};
};

class TestSession:
    public session::AsyncTransportSession<TestSession, Message, Message>,
    public std::enable_shared_from_this<TestSession> {
public:
    using Base = session::AsyncTransportSession<TestSession, Message, Message>;

    explicit TestSession(boost::asio::io_context& io_context):
        Base(io_context) {}

    template<typename Handler>
    void run_on_strand(Handler&& handler) {
        boost::asio::dispatch(strand(), std::forward<Handler>(handler));
    }

    bool can_start_read() const {
        return false;
    }

    bool can_start_write() const {
        return !is_write_in_progress() && has_pending_writes() && !is_closed();
    }

    void do_read() {}

    void do_write() {
        auto message = current_write();
        if (!message) {
            return;
        }

        mark_write_started();
        written_sequences.push_back(message->sequence);
        mark_write_finished();
        consume_write();
    }

    void close_transport() {}

    std::vector<std::uint32_t> written_sequences;
};

Message make_message(std::uint32_t sequence) {
    Message message;
    message.sequence = sequence;
    for (std::size_t i = 0; i < message.payload.size(); ++i) {
        message.payload[i] = static_cast<char>((sequence + i) & 0xffU);
    }
    return message;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

} // namespace

int main() {
    boost::asio::io_context io_context;
    auto test_session = std::make_shared<TestSession>(io_context);
    test_session->written_sequences.reserve(2);

    test_session->run_on_strand([test_session]() {
        test_session->send(make_message(1));
    });
    io_context.run();

    io_context.restart();
    std::size_t allocations = 0;
    test_session->run_on_strand([test_session, &allocations]() {
        g_allocation_count.store(0, std::memory_order_relaxed);
        g_count_allocations.store(true, std::memory_order_relaxed);
        test_session->send(make_message(2));
        g_count_allocations.store(false, std::memory_order_relaxed);
        allocations = g_allocation_count.load(std::memory_order_relaxed);
    });
    io_context.run();

    if (test_session->written_sequences.size() != 2U) {
        return fail("expected two writes to complete");
    }

    if (test_session->written_sequences[0] != 1U || test_session->written_sequences[1] != 2U) {
        return fail("expected writes to preserve FIFO order");
    }

    if (allocations != 0U) {
        std::cerr << "expected warmed inline send to avoid heap allocations, got "
                  << allocations << "\n";
        return 1;
    }

    return 0;
}
