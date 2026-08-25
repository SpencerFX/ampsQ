\l q/amps.q

h:.amps.connect["tcp://localhost:9007/amps/json";"q-amps-publisher"];

-1 "connected: ",string .amps.status h;

seq:.amps.publish[h;`trades;"{\"sym\":\"AAPL\",\"px\":231.42,\"qty\":100}"];
-1 "published sequence: ",string seq;

.amps.close h;
