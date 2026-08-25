\l q/amps.q

h:.amps.connect["tcp://localhost:9007/amps/json";"q-amps-sow"];

callback:{[m]
  -1 "SOW/AMPS message:";
  -1 m;
  };

 / SOW query with a filter and order-by expression.
sub:.amps.sow[h;`trades;"/sym = `AAPL";"/timestamp asc"];

-1 "SOW subscription: ",string sub;
