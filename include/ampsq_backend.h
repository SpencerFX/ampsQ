#ifndef AMPSQ_BACKEND_H
#define AMPSQ_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *ampsq_handle;

typedef struct {
    char *topic;
    size_t topic_len;

    char *command;
    size_t command_len;

    char *data;
    size_t data_len;

    char *timestamp;
    size_t timestamp_len;

    char *bookmark;
    size_t bookmark_len;

    char *sequence;
    size_t sequence_len;
} ampsq_event;

ampsq_handle ampsq_backend_create(const char *name, int event_fd,
                                  char *err, size_t err_len);

int ampsq_backend_connect(ampsq_handle h, const char *uri,
                          const char *logon_options,
                          char *err, size_t err_len);

int ampsq_backend_publish(ampsq_handle h, const char *topic, size_t topic_len,
                          const char *data, size_t data_len,
                          uint64_t *sequence,
                          char *err, size_t err_len);

int ampsq_backend_subscribe(ampsq_handle h, const char *topic,
                            const char *filter, const char *options,
                            const char *sub_id,
                            char *subscription_id, size_t subscription_id_len,
                            char *err, size_t err_len);

int ampsq_backend_sow(ampsq_handle h, const char *topic,
                      const char *filter, const char *order_by,
                      const char *options,
                      char *subscription_id, size_t subscription_id_len,
                      char *err, size_t err_len);

int ampsq_backend_unsubscribe(ampsq_handle h, const char *subscription_id,
                              char *err, size_t err_len);

int ampsq_backend_drain(ampsq_handle h, ampsq_event *out);
void ampsq_backend_free_event(ampsq_event *ev);

void ampsq_backend_close(ampsq_handle h);

#ifdef __cplusplus
}
#endif

#endif
