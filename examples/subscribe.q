\l q/amps.q

h:.amps.connect["tcp://localhost:9007/amps/json";"q-amps-subscriber"];

callback:{[m]
  -1 "AMPS message:";
  -1 m;
  };

sub:.amps.on[h;callback;"trades";""];

-1 "subscription: ",string sub;
-1 "waiting for AMPS messages...";

 / Keep q alive. In an interactive session you can simply leave q running.
 / For a script, use `\\` to terminate it manually.
