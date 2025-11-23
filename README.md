# Online Python code executor

A very simple python code executor writen in C.

Implements concepts of Socket and threads. 
It also has a simple HTTP parsing for HTTP support.

### Pre-requisites
- Linux
- GCC
- Python3


### Running the server
Compiling the server app

```bash
gcc server.c -o server
```

Running the server app
```bash
./server 8080
```

Testing the web app
```bash
curl -X POST      -H "Content-Type: text/plain"      --data-binary 'print("O resultado eh: " + str(10*5))'      http://127.0.0.1:8080
```


