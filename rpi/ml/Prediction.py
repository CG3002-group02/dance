# coding: utf-8

# In[1]:


# Importation of  model
import pickle

# data analysis
import pandas as pd

import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning)


def predict():
    filename_mlp = "/home/pi/3002/ml/Neural_Network_Model.sav"
    filename_svm = "/home/pi/3002/ml/SVM.sav"
    filename_rd = "/home/pi/3002/ml/RD.sav"
#     data = pd.read_csv(r'/home/pi/Desktop/test_dataset.csv')
#     data = pd.read_csv(r'C:\Users\User\Desktop\Study\Year 3\Sem 1\CG3002\DanceProject\Machine Learning\extracted data\test_dataset.csv')
    data = pd.read_csv('/home/pi/3002/ml/extracted_data.csv')
    mlp_model = pickle.load(open(filename_mlp, 'rb'))
    svm_model = pickle.load(open(filename_svm, 'rb'))
    rd_model = pickle.load(open(filename_rd, 'rb'))

    prediction_mlp = mlp_model.predict(data).tolist()
    prediction_svm = svm_model.predict(data).tolist()
    prediction_rd = rd_model.predict(data).tolist()
    predictions = prediction_mlp + prediction_svm + prediction_rd

    mapping = { 1 : 'chicken', 2 : 'number7', 3 : 'sidestep', 4 : 'turnclap', 5 : 'wiper'}
    output = pd.DataFrame({'activity' : predictions})
    output = output.replace({'activity': mapping})

    b = output.activity.mode()
    return b[0]

