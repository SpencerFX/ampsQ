\l q/amps.q

h:.amps.connect["tcp://localhost:9007/amps/json";"q-amps-demo"];

callback:{[m]
  -1 m;
  };

.amps.on[h;callback;"trades";""];

.doPublish:{[i]
  data:"{\"sym\":\"AAPL\",\"px\":231.42,\"qty\":",string 100+i,"}";
  .amps.publish[h;`trades;data];
  };

.doPublish each til 10;

 / The AMPS client receives asynchronously.
 / Leave q running to observe callbacks.
