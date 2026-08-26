/ Tests for generator/config/schema.q, the q schema consumed by the
/ trade/quote benchmark generator (README "Trade/quote benchmark
/ generator" section). Pure q, no native/AMPS dependency, so it runs
/ anywhere.

.qt.describe["generator schema"]{

  .qt.beforeAll{ system "l generator/config/schema.q" };
  .qt.afterAll{ ![`.;();0b;`trade`quote] };

  .qt.should["trade and quote both start empty"]{
    .qt.assertEquals[count trade;0;"trade has no rows"];
    .qt.assertEquals[count quote;0;"quote has no rows"]
   };

  .qt.should["trade has the documented columns with the documented kdb+ types"]{
    m:meta trade;
    .qt.assertEquals[cols trade;`ts`sym`price`size`side`venue`seq;"column order"];
    .qt.assertEquals[m[`ts;`t];"p";"ts: timestamp"];
    .qt.assertEquals[m[`sym;`t];"s";"sym: symbol"];
    .qt.assertEquals[m[`price;`t];"f";"price: float"];
    .qt.assertEquals[m[`size;`t];"j";"size: long"];
    .qt.assertEquals[m[`side;`t];"s";"side: symbol"];
    .qt.assertEquals[m[`venue;`t];"s";"venue: symbol"];
    .qt.assertEquals[m[`seq;`t];"j";"seq: long"]
   };

  .qt.should["quote has the documented columns with the documented kdb+ types"]{
    m:meta quote;
    .qt.assertEquals[cols quote;`ts`sym`bid`ask`bidSize`askSize`venue`seq;"column order"];
    .qt.assertEquals[m[`ts;`t];"p";"ts: timestamp"];
    .qt.assertEquals[m[`sym;`t];"s";"sym: symbol"];
    .qt.assertEquals[m[`bid;`t];"f";"bid: float"];
    .qt.assertEquals[m[`ask;`t];"f";"ask: float"];
    .qt.assertEquals[m[`bidSize;`t];"j";"bidSize: long"];
    .qt.assertEquals[m[`askSize;`t];"j";"askSize: long"];
    .qt.assertEquals[m[`venue;`t];"s";"venue: symbol"];
    .qt.assertEquals[m[`seq;`t];"j";"seq: long"]
   };
 };
