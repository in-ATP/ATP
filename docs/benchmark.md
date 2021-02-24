# Benchmark

## Requirment - Run with Signal Switch

In this experiment, 2 physical workers, 1 physical PS (Parameter Server) and 1 switch is used.


## Getting Started
```
$ git clone https://github.com/ATP-NSDI/ATP.git
```

### Run Tofino Switch 

#### Compile P4 Program and Start the Tofino Model (Terminal1)
If you are using physical switch, compile the switch program then jump to Terminal 2 directly.
```
$ cd $SDE
```
```
$ $TOOLS/p4_build.sh ~/git/p4ml/p4src/p4ml.p4
```
```
# (Optional) for software Tofino behavior model
$ ./run_tofino_model.sh -p p4ml
```
#### Load Specified Switch Program (Terminal2)
```
$ cd $SDE
```
```
$ ./run_switchd.sh -p p4ml
```
#### Enable Ports and Install Entries (Terminal3)
```
$ $SDE/run_p4_tests.sh -t $ATP_REPO/ptf/ -p p4ml 
```
```
$ $TOOLS/run_pd_rpc.py -p p4ml $ATP_REPO/run_pd_rpc/setup.py 
```

### Run Parameter Server 
#### Compile and Run Server (Terminal4)
```
$ cd $ATP_REPO/server/
```
```
$ make
```
```
# Usage: ./app [AppID]
sudo ./app 1
```

wait until all threads finish their QP creation.

### Compile and Run Workers
```
$ cd $ATP_REPO/client/
```
```
$ make
```
#### Run Worker1 (Terminal5)
```
# Usage: ./app [MyID] [Num of Worker] [AppID] [Num of PS]
$ sudo ./app 0 2 1 1
```
#### Run Worker2 (Terminal6)
```
# Usage: ./app [MyID] [Num of Worker] [AppID] [Num of PS]
$ sudo ./app 1 2 1 1
```

Then you can switch to Terminal 5/6 to the see bandwidth report.
