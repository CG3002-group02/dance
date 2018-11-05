
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
    filename_svm = "/home/pi/3002/ml/SVM.sav"
    filename_rd = "/home/pi/3002/ml/RD.sav"
    data = pd.read_csv('/home/pi/3002/ml/extracted_data.csv')
#     data = pd.read_csv(r'C:\Users\User\Desktop\Study\Year 3\Sem 1\CG3002\DanceProject\Machine Learning\extracted data\test_dataset.csv')
#     data = pd.read_csv(r'C:\Users\User\Desktop\chicken_extracted_dataset.csv')#
    mlp_model = pickle.load(open(filename_mlp, 'rb'))
    svm_model = pickle.load(open(filename_svm, 'rb'))
    rd_model = pickle.load(open(filename_rd, 'rb'))
    
    while(True):
        prediction_mlp = mlp_model.predict(data)
        prediction_svm = svm_model.predict(data)
        prediction_rd = rd_model.predict(data)
		print(prediction_mlp, prediction_rd, prediction_svm)
        if ((prediction_mlp == prediction_svm == prediction_rd) & (prediction_mlp != 0)):
            break
        data_processing(0.2)
		data = pd.read_csv('/home/pi/3002/ml/extracted_data.csv')
        
    mapping = {1 : 'chicken', 2 : 'number7', 3 : 'sidestep', 4 : 'turnclap', 5 : 'wipers',
               6 : 'numbersix', 7 : 'salute', 8 : 'mermaid', 9 : 'swing', 10 : 'cowboy', 11 : 'finisher'}
    return mapping[prediction_mlp[0]]


