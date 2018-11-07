
# coding: utf-8

# In[13]:


# Importation of  model
import pickle

# data analysis
import pandas as pd

import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning)

from .segmentation import data_processing


def predict():
    filename_mlp = "/home/pi/3002/ml/Neural_Network_Model.sav"
    filename_rd = "/home/pi/3002/ml/RD.sav"
    data = pd.read_csv('/home/pi/3002/ml/extracted_data.csv')
    
    mlp_model = pickle.load(open(filename_mlp, 'rb'))
    rd_model = pickle.load(open(filename_rd, 'rb'))
    
    [voltage, current, power, energy] = data_processing(2)
    
    while(True):
        prediction_mlp = mlp_model.predict(data)
        prediction_rd = rd_model.predict(data)
		print(prediction_mlp, prediction_rd, prediction_svm)
        if ((prediction_mlp == prediction_rd) & (prediction_mlp != 0)):
            break
        [voltage, current, power, energy] = data_processing(0.2)
		data = pd.read_csv('/home/pi/3002/ml/extracted_data.csv')
        
    mapping = {1 : 'chicken', 2 : 'number7', 3 : 'sidestep', 4 : 'turnclap', 5 : 'wipers',
               6 : 'numbersix', 7 : 'salute', 8 : 'mermaid', 9 : 'swing', 10 : 'cowboy', 11 : 'finisher'}
    
    result = mapping[prediction_mlp[0]]
    return [voltage, current, power, energy, result]


