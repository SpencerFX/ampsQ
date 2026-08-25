#include "ampsq_backend.h"

#include <amps/ampsplusplus.hpp>

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

struct ampsq_backend_client {
    AMPS::Client client;
    int event_fd;
    std::mutex mutex;
    std::deque<ampsq_event> queue;
    bool closed;

    explicit ampsq_backend_client(const char *name, int fd)
        : client(name ? name : ""), event_fd(fd), closed(false) {}
};

static void set_error(char *out, size_t n, const std::string &s) {
    if (!out || n == 0) return;
    std::snprintf(out, n, "%s", s.c_str());
}

static char *dup_bytes(const std::string &s) {
    char *p = static_cast<char *>(std::malloc(s.size() + 1));
    if (!p) return nullptr;
    if (!s.empty()) std::memcpy(p, s.data(), s.size());
    p[s.size()] = '\0';
    return p;
}

static ampsq_event make_event(const AMPS::Message &m) {
    ampsq_event e{};
    const std::string topic = m.getTopic();
    const std::string command = m.getCommand();
    const std::string data = m.getData();
    const std::string timestamp = m.getTimestamp();
    const std::string bookmark = m.getBookmark();

    e.topic = dup_bytes(topic);
    e.topic_len = topic.size();
    e.command = dup_bytes(command);
    e.command_len = command.size();
    e.data = dup_bytes(data);
    e.data_len = data.size();
    e.timestamp = dup_bytes(timestamp);
    e.timestamp_len = timestamp.size();
    e.bookmark = dup_bytes(bookmark);
    e.bookmark_len = bookmark.size();

    std::string seq;
    try {
        seq = std::to_string(m.getSequence());
    } catch (...) {
        seq.clear();
    }
    e.sequence = dup_bytes(seq);
    e.sequence_len = seq.size();

    return e;
}

static bool valid_event(const ampsq_event &e) {
    return (!e.topic && e.topic_len) || (!e.command && e.command_len) ||
           (!e.data && e.data_len) || (!e.timestamp && e.timestamp_len) ||
           (!e.bookmark && e.bookmark_len) || (!e.sequence && e.sequence_len)
           ? false : true;
}

static void notify(ampsq_backend_client *c) {
    uint64_t one = 1;
    ssize_t rc = ::write(c->event_fd, &one, sizeof(one));
    (void)rc;
}

static void on_message(ampsq_backend_client *c, const AMPS::Message &m) {
    ampsq_event e = make_event(m);
    if (!valid_event(e)) {
        ampsq_backend_free_event(&e);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(c->mutex);
        if (c->closed) {
            ampsq_backend_free_event(&e);
            return;
        }
        c->queue.emplace_back(e);
    }
    notify(c);
}

ampsq_handle ampsq_backend_create(const char *name, int event_fd,
                                  char *err, size_t err_len) {
    try {
        return new ampsq_backend_client(name, event_fd);
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return nullptr;
    }
}

int ampsq_backend_connect(ampsq_handle h, const char *uri,
                          const char *logon_options,
                          char *err, size_t err_len) {
    if (!h || !uri) {
        set_error(err, err_len, "invalid client or URI");
        return -1;
    }

    auto *c = static_cast<ampsq_backend_client *>(h);
    try {
        c->client.connect(uri);
        if (logon_options && *logon_options)
            c->client.logon(std::string(logon_options));
        else
            c->client.logon();
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return -1;
    }
}

int ampsq_backend_publish(ampsq_handle h, const char *topic, size_t topic_len,
                          const char *data, size_t data_len,
                          uint64_t *sequence,
                          char *err, size_t err_len) {
    if (!h || !topic || !data) {
        set_error(err, err_len, "invalid publish arguments");
        return -1;
    }

    auto *c = static_cast<ampsq_backend_client *>(h);
    try {
        const auto seq = c->client.publish(topic, topic_len, data, data_len);
        if (sequence) *sequence = static_cast<uint64_t>(seq);
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return -1;
    }
}

int ampsq_backend_subscribe(ampsq_handle h, const char *topic,
                            const char *filter, const char *options,
                            const char *sub_id,
                            char *subscription_id, size_t subscription_id_len,
                            char *err, size_t err_len) {
    if (!h || !topic) {
        set_error(err, err_len, "invalid subscribe arguments");
        return -1;
    }

    auto *c = static_cast<ampsq_backend_client *>(h);
    try {
        const std::string sub = c->client.subscribe(
            [c](const AMPS::Message &m) {
                on_message(c, m);
            },
            std::string(topic),
            0,
            filter ? std::string(filter) : std::string(),
            options ? std::string(options) : std::string(),
            sub_id ? std::string(sub_id) : std::string());

        if (subscription_id && subscription_id_len) {
            std::snprintf(subscription_id, subscription_id_len, "%s", sub.c_str());
        }
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return -1;
    }
}

int ampsq_backend_sow(ampsq_handle h, const char *topic,
                      const char *filter, const char *order_by,
                      const char *options,
                      char *subscription_id, size_t subscription_id_len,
                      char *err, size_t err_len) {
    if (!h || !topic) {
        set_error(err, err_len, "invalid SOW arguments");
        return -1;
    }

    auto *c = static_cast<ampsq_backend_client *>(h);
    try {
        const std::string sub = c->client.sow(
            [c](const AMPS::Message &m) {
                on_message(c, m);
            },
            std::string(topic),
            filter ? std::string(filter) : std::string(),
            order_by ? std::string(order_by) : std::string(),
            "",
            0,
            0,
            options ? std::string(options) : std::string(),
            0);

        if (subscription_id && subscription_id_len) {
            std::snprintf(subscription_id, subscription_id_len, "%s", sub.c_str());
        }
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return -1;
    }
}

int ampsq_backend_unsubscribe(ampsq_handle h, const char *subscription_id,
                              char *err, size_t err_len) {
    if (!h || !subscription_id) {
        set_error(err, err_len, "invalid subscription id");
        return -1;
    }

    auto *c = static_cast<ampsq_backend_client *>(h);
    try {
        c->client.unsubscribe(subscription_id);
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_len, e.what());
        return -1;
    }
}

int ampsq_backend_drain(ampsq_handle h, ampsq_event *out) {
    if (!h || !out) return 0;
    auto *c = static_cast<ampsq_backend_client *>(h);

    std::lock_guard<std::mutex> lock(c->mutex);
    if (c->queue.empty()) return 0;

    *out = c->queue.front();
    c->queue.pop_front();
    return 1;
}

void ampsq_backend_free_event(ampsq_event *e) {
    if (!e) return;
    std::free(e->topic);
    std::free(e->command);
    std::free(e->data);
    std::free(e->timestamp);
    std::free(e->bookmark);
    std::free(e->sequence);
    std::memset(e, 0, sizeof(*e));
}

void ampsq_backend_close(ampsq_handle h) {
    if (!h) return;
    auto *c = static_cast<ampsq_backend_client *>(h);

    {
        std::lock_guard<std::mutex> lock(c->mutex);
        c->closed = true;
        for (auto &e : c->queue)
            ampsq_backend_free_event(&e);
        c->queue.clear();
    }

    try {
        c->client.disconnect();
    } catch (...) {
    }

    delete c;
}
