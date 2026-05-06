## 📁 Code Structure

The `PCPS/` directory is organized into the following components:

- `core/`  
  Core implementation of PC-PS, including the worker logic and main processing pipeline.

- `index/`  
  Index implementations used in PC-PS, including R-BVH and all baseline methods.

- `logger/`  
  Experimental logging module.  
  Logs are organized by index type, experiment type, dataset, and result configuration.

- `util/`  
  Utility functions, including memory monitoring and time measurement.

## 🚀 How to Run

To reproduce the main results, please follow the steps below:

### 1. Prepare Datasets

Download the raw point-cloud datasets from the link provided in the root README.

Then, use the preprocessing scripts in `PointCloud_RawDataPreProcessing/` to clean and prepare the data.  
Make sure to update the dataset path in the main file accordingly.

---

### 2. Install Libtorch (for RLR-Tree)

Download the appropriate Libtorch version for your system from [the official website](https://pytorch.org/get-started/locally/).  
Extract it into the project directory and update the path in `CMakeLists.txt`.

---

### 3. Configure Experiment Settings

In the main file, modify the parameters in the `runExperiment` function:

- **PointSet**: dataset  
  (`WHU-TLS-M` / `WHU-TLS-R`)

- **QuerySet**: average selectivity of CR queries  
  (`VerySmall`: 1e-6, `Small`: 1e-5, `Medium`: 1e-4, `Large`: 1e-3)

- **IndexType**: index method to evaluate  
  (R-BVH, R-Tree, R*-Tree, RR*-Tree, RLR-Tree, 3DGrid, Z-Grid).  
  The configuration parameters of each index are defined in the main function.

- **TestType**: experiment type  
  (e.g., system throughput, baseline comparison)

> Note: The evaluation of R-BVH query registration, deletion, and publishing latency  
> is currently not enabled in this module.  
> These functionalities are implemented, but are separated to obtain more precise operation-level measurements.  
> Please refer to `PCPS_R-BVH(Only)/` for these specific tests.
