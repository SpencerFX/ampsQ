#define _GNU_SOURCE
#include "ampsq.h"
#include "ampsq_backend.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct ampsq_qclient {
    ampsq_handle backend;
    int event_fd;
    K callback;
    char *last_subscription;
    struct ampsq_qclient *next;
} ampsq_qclient;

static ampsq_qclient *g_clients = NULL;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static ampsq_qclient *find_client(int fd) {
    ampsq_qclient *p;
    pthread_mutex_lock(&g_clients_mutex);
    for (p = g_clients; p; p = p->next)
        if (p->event_fd == fd)
            break;
    pthread_mutex_unlock(&g_clients_mutex);
    return p;
}

static void add_client(ampsq_qclient *c) {
    pthread_mutex_lock(&g_clients_mutex);
    c->next = g_clients;
    g_clients = c;
    pthread_mutex_unlock(&g_clients_mutex);
}

static void remove_client(ampsq_qclient *c) {
    ampsq_qclient **p;
    pthread_mutex_lock(&g_clients_mutex);
    for (p = &g_clients; *p; p = &(*p)->next) {
        if (*p == c) {
            *p = c->next;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

static K qerr(const char *s) {
    return krr((S)s);
}

static int require_long(K x) {
    return x && x->t == -KJ;
}

static int require_symbol_or_char(K x) {
    return x && (x->t == -KS || x->t == KC || x->t == 10);
}

static const char *q_string(K x, char **owned) {
    *owned = NULL;
    if (!x) return NULL;

    if (x->t == -KS)
        return x->s;

    if (x->t == KC) {
        *owned = (char *)malloc((size_t)x->n + 1);
        if (!*owned) return NULL;
        memcpy(*owned, kC(x), (size_t)x->n);
        (*owned)[x->n] = '\0';
        return *owned;
    }

    if (x->t == 10) {
        *owned = (char *)malloc((size_t)x->n + 1);
        if (!*owned) return NULL;
        memcpy(*owned, kC(x), (size_t)x->n);
        (*owned)[x->n] = '\0';
        return *owned;
    }

    return NULL;
}

static K make_event_dict(const ampsq_event *e) {
    K keys = ktn(KS, 6);
    kS(keys)[0] = ss("topic");
    kS(keys)[1] = ss("command");
    kS(keys)[2] = ss("data");
    kS(keys)[3] = ss("timestamp");
    kS(keys)[4] = ss("bookmark");
    kS(keys)[5] = ss("sequence");

    K vals = ktn(0, 6);
    kK(vals)[0] = ss(e->topic ? e->topic : "");
    kK(vals)[1] = ss(e->command ? e->command : "");
    kK(vals)[2] = kpn(e->data ? e->data : "", (J)e->data_len);
    kK(vals)[3] = ss(e->timestamp ? e->timestamp : "");
    kK(vals)[4] = ss(e->bookmark ? e->bookmark : "");
    kK(vals)[5] = ss(e->sequence ? e->sequence : "");

    return xD(keys, vals);
}

static K ampsq_event(I fd) {
    uint64_t counter;
    ampsq_qclient *c = find_client(fd);
    if (!c) {
        close(fd);
        return (K)0;
    }

    while (read(fd, &counter, sizeof(counter)) == sizeof(counter)) {
        /* Drain all currently queued AMPS messages below. */
    }

    if (!c->callback)
        return (K)0;

    ampsq_event ev;
    while (ampsq_backend_drain(c->backend, &ev)) {
        K msg = make_event_dict(&ev);
        ampsq_backend_free_event(&ev);

        if (!msg)
            continue;

        K r = dot(r1(c->callback), msg);
        if (r)
            r0(r);
    }

    return (K)0;
}

K ampsq_connect(K x, K y) {
    if (!require_symbol_or_char(x) || !require_symbol_or_char(y))
        return qerr("type");

    char *uri_owned = NULL, *name_owned = NULL;
    const char *uri = q_string(x, &uri_owned);
    const char *name = q_string(y, &name_owned);

    if (!uri || !name) {
        free(uri_owned);
        free(name_owned);
        return qerr("type");
    }

    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        free(uri_owned);
        free(name_owned);
        return orr((S)"eventfd");
    }

    ampsq_qclient *c = (ampsq_qclient *)calloc(1, sizeof(*c));
    if (!c) {
        close(fd);
        free(uri_owned);
        free(name_owned);
        return qerr("memory");
    }

    char err[1024] = {0};
    c->event_fd = fd;
    c->backend = ampsq_backend_create(name, fd, err, sizeof(err));
    if (!c->backend) {
        close(fd);
        free(c);
        free(uri_owned);
        free(name_owned);
        return qerr(err[0] ? err : "AMPS client create failed");
    }

    if (ampsq_backend_connect(c->backend, uri, NULL, err, sizeof(err)) != 0) {
        ampsq_backend_close(c->backend);
        close(fd);
        free(c);
        free(uri_owned);
        free(name_owned);
        return qerr(err[0] ? err : "AMPS connect failed");
    }

    add_client(c);

    K sd_result = sd1(fd, ampsq_event);
    if (sd_result && sd_result->t == -128) {
        r0(sd_result);
        remove_client(c);
        ampsq_backend_close(c->backend);
        close(fd);
        free(c);
        free(uri_owned);
        free(name_owned);
        return qerr("sd1");
    }
    if (sd_result)
        r0(sd_result);

    free(uri_owned);
    free(name_owned);

    return kj((J)(intptr_t)c);
}

K ampsq_close(K x) {
    if (!require_long(x))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;
    if (!c) return (K)0;

    remove_client(c);

    if (c->callback) {
        r0(c->callback);
        c->callback = NULL;
    }

    if (c->event_fd > 0) {
        sd0x(c->event_fd, 0);
        close(c->event_fd);
        c->event_fd = -1;
    }

    ampsq_backend_close(c->backend);
    free(c->last_subscription);
    free(c);

    return (K)0;
}

K ampsq_publish(K x, K y, K z) {
    if (!require_long(x) || !require_symbol_or_char(y) ||
        !z || z->t != 10)
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;
    char *topic_owned = NULL;
    const char *topic = q_string(y, &topic_owned);

    uint64_t seq = 0;
    char err[1024] = {0};

    int rc = ampsq_backend_publish(c->backend, topic, strlen(topic),
                                    kC(z), (size_t)z->n, &seq,
                                    err, sizeof(err));

    free(topic_owned);

    if (rc != 0)
        return qerr(err[0] ? err : "AMPS publish failed");

    return kj((J)seq);
}

K ampsq_subscribe(K x, K y, K z, K w) {
    if (!require_long(x) || !require_symbol_or_char(y) ||
        !require_symbol_or_char(z) || !require_symbol_or_char(w))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;
    char *topic_owned = NULL, *filter_owned = NULL, *options_owned = NULL;
    const char *topic = q_string(y, &topic_owned);
    const char *filter = q_string(z, &filter_owned);
    const char *options = q_string(w, &options_owned);

    char sub[256] = {0};
    char err[1024] = {0};

    int rc = ampsq_backend_subscribe(c->backend, topic, filter, options, NULL,
                                      sub, sizeof(sub), err, sizeof(err));

    free(topic_owned);
    free(filter_owned);
    free(options_owned);

    if (rc != 0)
        return qerr(err[0] ? err : "AMPS subscribe failed");

    free(c->last_subscription);
    c->last_subscription = strdup(sub);

    return ks(sub);
}

K ampsq_sow(K x, K y, K z, K w) {
    if (!require_long(x) || !require_symbol_or_char(y) ||
        !require_symbol_or_char(z) || !require_symbol_or_char(w))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;
    char *topic_owned = NULL, *filter_owned = NULL, *order_owned = NULL;
    const char *topic = q_string(y, &topic_owned);
    const char *filter = q_string(z, &filter_owned);
    const char *order = q_string(w, &order_owned);

    char sub[256] = {0};
    char err[1024] = {0};

    int rc = ampsq_backend_sow(c->backend, topic, filter, order, NULL,
                               sub, sizeof(sub), err, sizeof(err));

    free(topic_owned);
    free(filter_owned);
    free(order_owned);

    if (rc != 0)
        return qerr(err[0] ? err : "AMPS SOW failed");

    free(c->last_subscription);
    c->last_subscription = strdup(sub);

    return ks(sub);
}

K ampsq_unsubscribe(K x) {
    if (!require_long(x))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;
    if (!c->last_subscription)
        return (K)0;

    char err[1024] = {0};
    int rc = ampsq_backend_unsubscribe(c->backend, c->last_subscription,
                                       err, sizeof(err));
    if (rc != 0)
        return qerr(err[0] ? err : "AMPS unsubscribe failed");

    free(c->last_subscription);
    c->last_subscription = NULL;
    return (K)0;
}

K ampsq_on(K x, K y, K z, K w) {
    if (!require_long(x) || !y || y->t != 100 ||
        !require_symbol_or_char(z) || !require_symbol_or_char(w))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;

    if (c->callback)
        r0(c->callback);
    c->callback = r1(y);

    char *topic_owned = NULL, *filter_owned = NULL;
    const char *topic = q_string(z, &topic_owned);
    const char *filter = q_string(w, &filter_owned);

    char sub[256] = {0};
    char err[1024] = {0};
    int rc = ampsq_backend_subscribe(c->backend, topic, filter, NULL, NULL,
                                     sub, sizeof(sub), err, sizeof(err));

    free(topic_owned);
    free(filter_owned);

    if (rc != 0)
        return qerr(err[0] ? err : "AMPS subscribe failed");

    free(c->last_subscription);
    c->last_subscription = strdup(sub);

    return ks(sub);
}

K ampsq_status(K x) {
    if (!require_long(x))
        return qerr("type");

    ampsq_qclient *c = (ampsq_qclient *)(intptr_t)x->j;

    K keys = ktn(KS, 2);
    kS(keys)[0] = ss("connected");
    kS(keys)[1] = ss("subscription");

    K vals = ktn(0, 2);
    kK(vals)[0] = kb(1);
    kK(vals)[1] = ss(c->last_subscription ? c->last_subscription : "");

    return xD(keys, vals);
}
