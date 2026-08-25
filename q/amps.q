\d .amps

.connect:{[uri;name]
  .amps._connect[uri;name]}

.close:{[h]
  .amps._close h}

.publish:{[h;topic;data]
  .amps._publish[h;topic;data]}

.subscribe:{[h;topic;filter]
  .amps._subscribe[h;topic;filter;""]}

.sow:{[h;topic;filter;orderBy]
  .amps._sow[h;topic;filter;orderBy]}

.unsubscribe:{[h]
  .amps._unsubscribe h}

.on:{[h;callback;topic;filter]
  .amps._on[h;callback;topic;filter]}

.status:{[h]
  .amps._status h}

/ Load the native library from the project build directory.
/ Override AMPSQ_SO if you want a different location.
if[not `AMPSQ_SO in key `.Q.env;
  `AMPSQ_SO set `$"build/libampsq.so"];

.amps._connect:`$AMPSQ_SO 2:(`ampsq_connect;2)
.amps._close:`$AMPSQ_SO 2:(`ampsq_close;1)
.amps._publish:`$AMPSQ_SO 2:(`ampsq_publish;3)
.amps._subscribe:`$AMPSQ_SO 2:(`ampsq_subscribe;4)
.amps._sow:`$AMPSQ_SO 2:(`ampsq_sow;4)
.amps._unsubscribe:`$AMPSQ_SO 2:(`ampsq_unsubscribe;1)
.amps._on:`$AMPSQ_SO 2:(`ampsq_on;4)
.amps._status:`$AMPSQ_SO 2:(`ampsq_status;1)

\d .
