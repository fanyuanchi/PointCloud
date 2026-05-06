## 📦 RLR-Tree (Libtorch Reimplementation)

The original RLR-Tree paper did not release its official implementation.  
An internal version of the code was later shared by members of the same research group (link provided below).

To enable fair comparison with other C++-based baselines in PC-PS,  
we reimplemented RLR-Tree using Libtorch.

- Original paper: [link]
- Reference implementation: [link]

---

## 📁 Code Structure

- `dataLoader/`  
  Data loading module for training.

- `index/`  
  Implementation of the learning components, including `InsertLearner` and `SplitLearner`.

- `model/`  
  Pretrained models.

- `util/`  
  Utility functions (e.g., time measurement).

---

## 🚀 How to Run

### 1. Prepare Data

Download the raw point-cloud datasets from the link provided in the root README.

Then, use the preprocessing scripts in `PointCloud_RawDataPreProcessing/` to clean and prepare the data.  
Make sure to update the dataset path in the main file accordingly.

---

### 2. Install Libtorch

Download the appropriate Libtorch version from:  
https://pytorch.org/get-started/locally/

Extract it into the project directory and configure the path in `CMakeLists.txt`.

---

### 3. Run Training

In `main.cpp`, configure the parameters in the `runTraining` function:

- **TrainType**: type of training task  
- **Dataset**: dataset to use

Then compile and run the program to start training.
