# data analysis
import pandas as pd
import time

# scale the data points
from sklearn import preprocessing


def DataProcessing():
    time.sleep(2)

    # read data
    data = pd.read_csv('/home/pi/3002/readings.csv')
    data = data.tail(110).copy()

    cleaned_data = data.drop(['voltage','current','power','energy'],axis =1) #delete column
    cleaned_data = pd.DataFrame(preprocessing.scale(cleaned_data),
                                                   columns=['AcX 1', 'AcY 1', 'AcZ 1', 'GyX 1', 'GyY 1', 'GyZ 1',
                                                            'AcX 2', 'AcY 2', 'AcZ 2', 'GyX 2', 'GyY 2', 'GyZ 2',
                                                            'AcX 3', 'AcY 3', 'AcZ 3', 'GyX 3', 'GyY 3', 'GyZ 3'])
    mean_data = pd.DataFrame()
    std_data = pd.DataFrame()

    # i = 0

    # segmentation at 80% @ 55Hz 2s
    a = cleaned_data[0:110].copy()
    a.loc['mean'] = a.mean()
    a.loc['std'] = a.std()

    mean_data = mean_data.append(a.loc['mean'], ignore_index=True) #store in mean_data list
    std_data = std_data.append(a.loc['std'], ignore_index=True)    #store in std_data list

    # i += 21


    # rearrangement of columns
    mean_data = mean_data.rename(index=str, columns={'AcX 1': 'mean_AcX 1', 'AcY 1': 'mean_AcY 1', 'AcZ 1': 'mean_AcZ 1',
                                                     'GyX 1': 'mean_GyX 1', 'GyY 1': 'mean_GyY 1', 'GyZ 1': 'mean_GyZ 1',
                                                     'AcX 2': 'mean_AcX 2', 'AcY 2': 'mean_AcY 2', 'AcZ 2': 'mean_AcZ 2',
                                                     'GyX 2': 'mean_GyX 2', 'GyY 2': 'mean_GyY 2', 'GyZ 2': 'mean_GyZ 2',
                                                     'AcX 3': 'mean_AcX 3', 'AcY 3': 'mean_AcY 3', 'AcZ 3': 'mean_AcZ 3',
                                                     'GyX 3': 'mean_GyX 3', 'GyY 3': 'mean_GyY 3', 'GyZ 3': 'mean_GyZ 3',})

    std_data = std_data.rename(index=str, columns=  {'AcX 1': 'std_AcX 1', 'AcY 1': 'std_AcY 1', 'AcZ 1': 'std_AcZ 1',
                                                     'GyX 1': 'std_GyX 1', 'GyY 1': 'std_GyY 1', 'GyZ 1': 'std_GyZ 1',
                                                     'AcX 2': 'std_AcX 2', 'AcY 2': 'std_AcY 2', 'AcZ 2': 'std_AcZ 2',
                                                     'GyX 2': 'std_GyX 2', 'GyY 2': 'std_GyY 2', 'GyZ 2': 'std_GyZ 2',
                                                     'AcX 3': 'std_AcX 3', 'AcY 3': 'std_AcY 3', 'AcZ 3': 'std_AcZ 3',
                                                     'GyX 3': 'std_GyX 3', 'GyY 3': 'std_GyY 3', 'GyZ 3': 'std_GyZ 3',})


    # combine features extracted into 1
    extracted_data = mean_data.join(std_data) #merge the mean_data list and std_data list

    # store row 110
    # data = pd.read_csv('/home/pi/3002/readings.csv')
    # data = data[110::].copy()
    # data.to_csv(r'/home/pi/3002/readings.csv',index=False)

    extracted_data.to_csv('/home/pi/3002/ml/extracted_data.csv', index=False)

    voltage = data["voltage"].iloc[-1]
    current = data["current"].iloc[-1]
    power = data["power"].iloc[-1]
    energy = data["energy"].iloc[-1]

    return (voltage, current, power, energy)
