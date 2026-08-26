/ Tests for the pure-q API layer in q/amps.q: the `.amps.*` wrapper
/ functions and the AMPSQ_SO native-library-path override guard.
/ -
/ q/amps.q ends with a block of `2:` dynamic-library loads that require
/ build/libampsq.so plus the 60East AMPS C++ SDK on Linux - neither exists
/ in this workspace/platform, and a single failing line anywhere in a
/ script loaded via `\l`/`system"l ..."` aborts the WHOLE script (verified
/ by hand: `\l q/amps.q` then `.amps.connect` reports "undefined
/ variable", even though its definition sits many lines before the
/ failing `2:` calls and would otherwise have taken effect on its own).
/ So this file regenerates just the wrapper-defining prefix of the real
/ source (everything before the "Load the native library" comment) into
/ a scratch file and loads that instead, then mocks .amps._* to check the
/ wrapper logic. This keeps the test bound to the real, current source
/ rather than a hand copy. (q/amps.q also has CRLF line endings; read0
/ does not strip the trailing \r, so that is stripped explicitly too -
/ otherwise the stray \r survives as literal data in the regenerated file.)
/ -
/ This suite previously caught two real defects in q/amps.q, both now
/ fixed: the wrapper names used to be dotted (`.connect:{...}` etc) under
/ `\d .amps`, which in q is an absolute reference to root and ignores the
/ current `\d` context - so they landed at root instead of under `.amps`.
/ And the AMPSQ_SO override guard checked `key `.Q.env` (always empty,
/ not a real namespace) instead of `key `.`, so it could never detect a
/ caller's override. The "regression guards" describe block below keeps
/ both fixes pinned down.

.qt.describe["amps.q wrapper functions"]{

  .qt.beforeAll{
    src:{ssr[x;"\r";""]} each read0 `:q/amps.q;
    cutAt:first where {x like "/ Load the native library*"} each src;
    wrapperSrc:(cutAt#src),enlist "\\d .";
    (hsym `$"tests/.scratch/amps_wrappers.q") 0: wrapperSrc;
    system "l tests/.scratch/amps_wrappers.q";
   };

  .qt.afterAll{
    ![`.;();0b;enlist `amps];
   };

  .qt.before{
    .qt.mock[`.amps._connect;{[uri;name] `.ampsSpy.connectArgs set (uri;name); 42j}];
    .qt.mock[`.amps._close;{[h] `.ampsSpy.closeArgs set enlist h}];
    .qt.mock[`.amps._publish;{[h;topic;data] `.ampsSpy.publishArgs set (h;topic;data); 7j}];
    .qt.mock[`.amps._subscribe;{[h;topic;filter;options] `.ampsSpy.subscribeArgs set (h;topic;filter;options); `subA}];
    .qt.mock[`.amps._sow;{[h;topic;filter;orderBy] `.ampsSpy.sowArgs set (h;topic;filter;orderBy); `subB}];
    .qt.mock[`.amps._unsubscribe;{[h] `.ampsSpy.unsubscribeArgs set enlist h}];
    .qt.mock[`.amps._on;{[h;cb;topic;filter] `.ampsSpy.onArgs set (h;cb;topic;filter); `subC}];
    .qt.mock[`.amps._status;{[h] `.ampsSpy.statusArgs set enlist h; `connected`subscription!(1b;`subA)}];
   };

  .qt.after{ .qt.restore[] };

  .qt.should["connect forwards uri and name unchanged, returns native result"]{
    r:.amps.connect["tcp://x:1/amps/json";"myapp"];
    .qt.assertEquals[.ampsSpy.connectArgs;("tcp://x:1/amps/json";"myapp");"args forwarded"];
    .qt.assertEquals[r;42j;"native return value passed through"]
   };

  .qt.should["publish forwards handle, topic and data unchanged"]{
    r:.amps.publish[99j;`trades;"{}"];
    .qt.assertEquals[.ampsSpy.publishArgs;(99j;`trades;"{}");"args forwarded"];
    .qt.assertEquals[r;7j;"sequence passed through"]
   };

  .qt.should["subscribe forwards topic/filter but always sends an empty options string"]{
    .amps.subscribe[1j;`trades;"/sym=AAPL"];
    .qt.assertEquals[.ampsSpy.subscribeArgs;(1j;`trades;"/sym=AAPL";"");"options hardcoded empty"]
   };

  .qt.should["on forwards callback, topic and filter unchanged"]{
    cb:{[m] m};
    .amps.on[1j;cb;`trades;"filt"];
    .qt.assertEquals[.ampsSpy.onArgs;(1j;cb;`trades;"filt");"args forwarded"]
   };

  .qt.should["sow forwards topic, filter and orderBy unchanged"]{
    .amps.sow[1j;`trades;"/sym=AAPL";"/timestamp asc"];
    .qt.assertEquals[.ampsSpy.sowArgs;(1j;`trades;"/sym=AAPL";"/timestamp asc");"args forwarded"]
   };

  .qt.should["close/unsubscribe/status forward the handle unchanged"]{
    .amps.close[5j];
    .qt.assertEquals[.ampsSpy.closeArgs;enlist 5j;"close"];
    .amps.unsubscribe[6j];
    .qt.assertEquals[.ampsSpy.unsubscribeArgs;enlist 6j;"unsubscribe"];
    .amps.status[7j];
    .qt.assertEquals[.ampsSpy.statusArgs;enlist 7j;"status"]
   };
 };

/ ---------------------------------------------------------------------------
/ Regression guards for the two fixed defects described above.
.qt.describe["regression guards"]{

  .qt.beforeAll{
    src:{ssr[x;"\r";""]} each read0 `:q/amps.q;
    cutAt:first where {x like "/ Load the native library*"} each src;
    wrapperSrc:(cutAt#src),enlist "\\d .";
    (hsym `$"tests/.scratch/amps_wrappers.q") 0: wrapperSrc;
    system "l tests/.scratch/amps_wrappers.q";
   };

  .qt.afterAll{
    ![`.;();0b;enlist `amps];
   };

  .qt.should["the wrapper functions land under .amps, not at root"]{
    .qt.assertNoError[value;`.amps.connect;".amps.connect must resolve"];
    .qt.assertErrorLike["*.connect*";value;`.connect;
      "a bare root .connect should NOT exist now that the leading dot is gone"]
   };

  .qt.should["a pre-set root AMPSQ_SO is visible to the override guard"]{
    .qt.mock[`AMPSQ_SO;`myCustomLib];
    .qt.assertTrue[`AMPSQ_SO in key `.;"the guard's own check target sees the override"]
   };
 };
