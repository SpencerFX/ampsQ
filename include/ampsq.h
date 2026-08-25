#ifndef AMPSQ_H
#define AMPSQ_H

#include "k.h"

#ifdef __cplusplus
extern "C" {
#endif

K ampsq_connect(K x, K y);
K ampsq_close(K x);
K ampsq_publish(K x, K y, K z);
K ampsq_subscribe(K x, K y, K z, K w);
K ampsq_sow(K x, K y, K z, K w);
K ampsq_unsubscribe(K x);
K ampsq_on(K x, K y, K z, K w);
K ampsq_status(K x);

#ifdef __cplusplus
}
#endif

#endif
