ssh in to rpi - 3 separate terminals.
terminal 1 - `cd 3002/comm`, then `python comm.py .. readings`
terminal 2 - `cd 3002`, then `tail -f readings.csv` - to check if output is still running

If with server, also open a terminal on your laptop (not ssh),
`cd server` then `python final_eval_server.py <laptop ip> 8080`
terminal 3 - `cd 3002`, then

`python main.py` - without server
`python main.py <laptop ip> 8080 1234123412341234` - with server
It will prompt for a secret key. Enter 1234123412341234
