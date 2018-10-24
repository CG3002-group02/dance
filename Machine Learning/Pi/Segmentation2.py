
# coding: utf-8

# In[5]:


# data analysis
import pandas as pd

import time

# scale the data points
from sklearn import preprocessing

import datetime

def data_processing():
    start = datetime.datetime.now()
    # Wait for 2 seconds
    time.sleep(1.6)
    # read data
    data = pd.read_csv(r'/home/pi/Desktop/test_dataset.csv')
    # data = pd.read_csv(r'C:\Users\User\Desktop\chicken_dataset.csv')
	data = data.tail(220).copy()
    cleaned_data = data.drop(['voltage','current','power','energy'],axis =1).copy() #delete column
    cleaned_data = cleaned_data.astype(float)
    cleaned_data = pd.DataFrame(preprocessing.scale(cleaned_data),
                                                   columns=['AcX 1', 'AcY 1', 'AcZ 1', 'GyX 1', 'GyY 1', 'GyZ 1', 
                                                
                                                            
                                                            'AcX 2', 'AcY 2', 'AcZ 2', 'GyX 2', 'GyY 2', 'GyZ 2',
                                                            'AcX 3', 'AcY 3', 'AcZ 3', 'GyX 3', 'GyY 3', 'GyZ 3'])
    i = 0
    mean_data = pd.DataFrame() 
    std_data = pd.DataFrame() 
    max_data = pd.DataFrame()
    
    for count in range(5):
        a = cleaned_data[i:i+110].copy()
        a.loc['mean'] = a.mean()
        a.loc['std'] = a.std()
        a.loc['max'] = a.max()

        mean_data = mean_data.append(a.loc['mean'], ignore_index=True)
        std_data = std_data.append(a.loc['std'], ignore_index=True)
        max_data = max_data.append(a.loc['max'], ignore_index=True)
        i += 21

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
    
    max_data = max_data.rename(index=str, columns=  {'AcX 1': 'max_AcX 1', 'AcY 1': 'max_AcY 1', 'AcZ 1': 'max_AcZ 1', 
                                                     'GyX 1': 'max_GyX 1', 'GyY 1': 'max_GyY 1', 'GyZ 1': 'max_GyZ 1',
                                                     'AcX 2': 'max_AcX 2', 'AcY 2': 'max_AcY 2', 'AcZ 2': 'max_AcZ 2', 
                                                     'GyX 2': 'max_GyX 2', 'GyY 2': 'max_GyY 2', 'GyZ 2': 'max_GyZ 2',
                                                     'AcX 3': 'max_AcX 3', 'AcY 3': 'max_AcY 3', 'AcZ 3': 'max_AcZ 3', 
                                                     'GyX 3': 'max_GyX 3', 'GyY 3': 'max_GyY 3', 'GyZ 3': 'max_GyZ 3',})
    
    
    # combine features extracted into 1 
    extracted_data = mean_data.join(std_data).join(max_data)

    # store row 110
    # data = pd.read_csv(r'C:\Users\User\Desktop\chicken_dataset.csv')
    # data = data[110::].copy()
    # data.to_csv(r'C:\Users\User\Desktop\chicken_dataset.csv',index=False)
    extracted_data.to_csv(r'C:\Users\User\Desktop\chicken_extracted_dataset.csv', index=False)
    
    voltage = data['voltage'].iloc[-1]
    current = data['current'].iloc[-1]
    power = data['power'].iloc[-1]
    energy = data['energy'].iloc[-1]
    return [voltage, current, power, energy]

