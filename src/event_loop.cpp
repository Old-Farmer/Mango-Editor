#include "event_loop.h"

#include "options.h"

namespace mango {

EventLoop::EventLoop(GlobalOpts* global_opts) : global_opts_(global_opts) {}

void EventLoop::AddEventHandler(EventInfo info) {
    event_infos_.emplace(info.fd, std::move(info));
    changed_ = true;
}

void EventLoop::RemoveEventHandler(EventFD fd) {
    event_infos_.erase(fd);
    changed_ = true;
}

void EventLoop::BeforePoll(std::function<void()> f) {
    before_poll_ = std::move(f);
}
void EventLoop::AfterAllEvents(std::function<void()> f) {
    after_all_events_ = std::move(f);
}

void EventLoop::Loop() {
    while (!quit_) {
        if (before_poll_) {
            before_poll_();
        }

        if (changed_) {
            poll_fds_.clear();
            for (const auto& [fd, info] : event_infos_) {
                pollfd poll_fd;
                poll_fd.fd = fd;
                bzero(&poll_fd.events, sizeof(poll_fd.events));
                poll_fd.events |=
                    (info.Interesting_events & kEventRead ? POLLIN : 0);
                poll_fd.events |=
                    (info.Interesting_events & kEventWrite ? POLLOUT : 0);
                poll_fds_.push_back(poll_fd);
            }
            changed_ = false;
        }

        int rc = poll(poll_fds_.data(), poll_fds_.size(),
                      global_opts_->GetOpt<int64_t>(kOptPollEventTimeout));

        if (rc == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            throw OSException(errno, "poll error: %s", strerror(errno));
        }

        if (rc == 0) {
            continue;
        }

        int cnt = 0;
        for (const pollfd& poll_fd : poll_fds_) {
            if (cnt == rc) {
                break;
            }

            if (poll_fd.revents == 0) {
                continue;
            }

            cnt++;
            auto iter = event_infos_.find(poll_fd.fd);
            if (iter == event_infos_.end()) {
                // Just delete
                continue;
            }
            Event e = 0;
            e |= (poll_fd.revents & POLLIN ? kEventRead : 0);
            e |= (poll_fd.revents & POLLOUT ? kEventWrite : 0);
            e |= (poll_fd.revents & POLLHUP ? kEventClose : 0);
            e |= (poll_fd.revents & POLLERR ? kEventError : 0);
            if (poll_fd.revents & POLLNVAL) {
                throw OSException(errno, "fd not opened: %s", strerror(errno));
            }
            // TODO: POLL_PRI
            iter->second.handler(e);
        }

        if (after_all_events_) {
            after_all_events_();
        }
    }
}

}  // namespace mango