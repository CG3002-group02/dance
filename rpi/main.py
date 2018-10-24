import sys

from comm.comm import run_comm_loop
from comm.client import Client
from ml.Prediction import predict
from ml.segmentation import data_processing


if __name__ == '__main__':
    run_client = None

    if len(sys.argv) == 1:
        run_client = False
    elif len(sys.argv) == 4:
        run_client = True
    else:
        print('Invalid number of arguments. Type one of the following commands')
        print('python main.py')
        print('python main.py [IP address] [Port] [AES key]')
        sys.exit()

    if run_client:
        ip_addr = sys.argv[1]
        port_num = int(sys.argv[2])
        key = sys.argv[3]

        if not(len(key) == 16 or len(key) == 24 or len(key) == 32):
            print("AES key must be either 16, 24, or 32 bytes long")
            sys.exit()

        client = Client(ip_addr, port_num, key)
        print('Initialised TCP client')

    action = None

    # run_comm_loop('/home/pi/3002/readings.csv', background=True)
    # print('Comm loop running')
    try:
        while action != 'logout':
            voltage, current, power, energy = data_processing()
            print('Voltage: {}, current: {}, power: {}, energy: {}'.format(voltage, current, power, energy))
            action = predict()
            print('Predicted action: {}'.format(action))
            if run_client:
                client.send(action, {'voltage': voltage, 'current': current, 'power': power, 'cumpower': energy})
                print('Sent to server')
    except KeyboardInterrupt:
        if run_client:
            client.end()
        sys.exit()
