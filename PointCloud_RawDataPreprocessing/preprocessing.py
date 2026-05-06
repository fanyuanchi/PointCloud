import laspy
import pandas as pd
import numpy as np
from typing import Dict, Tuple


def load_las_to_dataframe(
    las_path: str,
    use_dims: tuple = ("x", "y", "z"),
    copy: bool = True
) -> pd.DataFrame:
    """
    Load raw point cloud from a .las/.laz file into a pandas DataFrame.

    Parameters
    ----------
    las_path : str
        Path to the .las or .laz file.
    use_dims : tuple
        Dimensions to load, default ("x", "y", "z").
    copy : bool
        Whether to copy data into a new numpy array (safer).

    Returns
    -------
    pd.DataFrame
        DataFrame with columns given by use_dims, dtype=float64.
    """
    # ---- Step 1: Read LAS file ----
    las = laspy.read(las_path)

    # ---- Step 2: Extract coordinates (already scaled & offset by laspy) ----
    data = {}
    for dim in use_dims:
        if not hasattr(las, dim):
            raise ValueError(f"LAS file does not contain dimension '{dim}'")
        arr = getattr(las, dim)
        # Ensure float64 for C++ double compatibility
        data[dim] = np.asarray(arr, dtype=np.float64, order="C")

    # ---- Step 3: Build DataFrame ----
    df = pd.DataFrame(data, copy=copy)

    return df
pass


def extract_scaled_dims(las, use_dims=("X", "Y", "Z")):
    """
    Extract scaled coordinates from LAS using raw integer dimensions.
    Returns dict of float64 numpy arrays.
    """
    data = {}

    for dim in use_dims:
        if not hasattr(las, dim):
            raise ValueError(f"LAS file does not contain dimension '{dim}'")

        raw = getattr(las, dim)

        scale = las.header.scales["XYZ".index(dim)]
        offset = las.header.offsets["XYZ".index(dim)]

        data[dim.lower()] = (
            np.asarray(raw, dtype=np.float64, order="C") * scale + offset
        )

    return data
pass

def load_las_list_to_dataframe(las_path_list):
    xs, ys, zs = [], [], []

    for path in las_path_list:
        las = laspy.read(path)

        coords = extract_scaled_dims(las, use_dims=("X", "Y", "Z"))

        xs.append(coords["x"])
        ys.append(coords["y"])
        zs.append(coords["z"])

    if not xs:
        return pd.DataFrame(columns=["x", "y", "z"])

    return pd.DataFrame({
        "x": np.concatenate(xs),
        "y": np.concatenate(ys),
        "z": np.concatenate(zs),
    })


def quantile_clip(
    df: pd.DataFrame,
    dims: tuple = ("x", "y", "z"),
    q_low: float = 0.01,
    q_high: float = 0.99,
    verbose: bool = True
) -> pd.DataFrame:
    """
    Clip point cloud using per-dimension quantiles.

    Parameters
    ----------
    df : pd.DataFrame
        Input point cloud DataFrame.
    dims : tuple
        Dimensions to clip, default ("x", "y", "z").
    q_low : float
        Lower quantile (e.g., 0.01).
    q_high : float
        Upper quantile (e.g., 0.99).
    verbose : bool
        Whether to print clipping statistics.

    Returns
    -------
    pd.DataFrame
        Clipped DataFrame with reset index.
    """
    assert 0.0 <= q_low < q_high <= 1.0, "Invalid quantile range"

    # ---- Step 1: Compute per-dimension quantiles (once) ----
    bounds = {}
    for d in dims:
        low = df[d].quantile(q_low)
        high = df[d].quantile(q_high)
        bounds[d] = (low, high)

    # ---- Step 2: Build global mask (AND over dimensions) ----
    mask = np.ones(len(df), dtype=bool)
    for d, (low, high) in bounds.items():
        mask &= (df[d] >= low) & (df[d] <= high)

    # ---- Step 3: Apply mask ----
    clipped_df = df.loc[mask].reset_index(drop=True)

    # ---- Step 4: Optional logging ----
    if verbose:
        original_n = len(df)
        clipped_n = len(clipped_df)
        ratio = clipped_n / original_n * 100.0
        print(
            f"[QuantileClip] "
            f"q=({q_low:.3f}, {q_high:.3f}) | "
            f"kept {clipped_n}/{original_n} "
            f"({ratio:.2f}%)"
        )
        for d, (low, high) in bounds.items():
            print(
                f"  {d}: [{low:.6e}, {high:.6e}]"
            )

    return clipped_df
pass



def grid_occupancy_analysis(df: pd.DataFrame, bins=100, columns=("x", "y", "z")):
    """
    Analyze point occupancy in a uniform 3D grid.

    Parameters
    ----------
    df : pandas.DataFrame
        DataFrame containing point cloud coordinates.
    bins : int
        Number of bins per dimension.
    columns : tuple
        Column names for (x, y, z).

    Returns
    -------
    stats : dict
        Summary statistics of grid occupancy.
    occupancy : np.ndarray
        Flattened array of non-zero voxel occupancies.
    """

    coords = df[list(columns)].to_numpy()

    # 3D histogram
    hist, edges = np.histogramdd(coords, bins=bins)

    # Only consider non-empty voxels
    nonzero = hist[hist > 0].astype(np.int64)

    total_voxels = bins ** 3
    occupied_voxels = nonzero.size
    total_points = coords.shape[0]

    stats = {
        "bins_per_dim": bins,
        "total_voxels": total_voxels,
        "occupied_voxels": occupied_voxels,
        "occupied_ratio": occupied_voxels / total_voxels,
        "total_points": total_points,
        "mean_points_per_occupied_voxel": nonzero.mean(),
        "max_points_in_voxel": nonzero.max(),
        "p50_points_per_voxel": np.percentile(nonzero, 50),
        "p90_points_per_voxel": np.percentile(nonzero, 90),
        "p99_points_per_voxel": np.percentile(nonzero, 99),
    }

    return stats, nonzero
pass



def uniform_downsample_preserve_order(df: pd.DataFrame, target_n: int) -> pd.DataFrame:
    """
    Uniformly downsample a DataFrame to target_n rows while preserving original order.

    Parameters
    ----------
    df : pd.DataFrame
        Input DataFrame (order matters).
    target_n : int
        Target number of rows after downsampling.

    Returns
    -------
    pd.DataFrame
        Downsampled DataFrame with original order preserved.
    """
    n = len(df)
    if target_n >= n:
        return df.copy()

    # Generate evenly spaced indices
    indices = np.linspace(0, n - 1, target_n, dtype=np.int64)

    # Use iloc to preserve order
    return df.iloc[indices].reset_index(drop=True)
pass


import pandas as pd

def write_dataframe_to_csv(
    df: pd.DataFrame,
    path: str,
    float_format: str = "%.10f"
):
    """
    Write DataFrame to CSV without header or index.

    Each row corresponds to one data point.
    """
    df.to_csv(
        path,
        header=False,
        index=False,
        float_format=float_format
    )
pass


def get_pointcloud_bounds(
    df: pd.DataFrame,
    cols=("x", "y", "z")
) -> Dict[str, Tuple[float, float]]:
    """
    Compute min/max bounds for a point cloud DataFrame.

    Parameters
    ----------
    df : pd.DataFrame
        Point cloud data.
    cols : tuple of str
        Column names corresponding to dimensions.

    Returns
    -------
    bounds : dict
        {
            "x": (min_x, max_x),
            "y": (min_y, max_y),
            "z": (min_z, max_z)
        }
    """
    bounds = {}
    for c in cols:
        col_min = df[c].min()
        col_max = df[c].max()
        bounds[c] = (float(col_min), float(col_max))
    return bounds
pass


def print_bounds_summary(low, top):
    extent = top - low
    for i, axis in enumerate(["x", "y", "z"]):
        print(f"{axis}: [{low[i]:.4f}, {top[i]:.4f}]  (len={extent[i]:.4f})")
pass


def check_df_within_bounds(df, bounds):
    """
    Check whether all points in df lie within given LOW / TOP bounds.

    Parameters
    ----------
    df : pandas.DataFrame
        Must contain columns corresponding to bounds keys, e.g. 'x', 'y', 'z'.
    bounds : dict
        {dim: (low, top)}, inclusive on both sides.

    Returns
    -------
    bool
        True if all values satisfy low <= v <= top for every dimension.
    """
    for dim, (low, top) in bounds.items():
        if dim not in df.columns:
            return False

        col = df[dim]
        if ((col >= low).all() and (col <= top).all()).empty:
            return False

    return True

las_list = ["D:\\PointCloudData\\3-Mountain\\1.las",
            "D:\\PointCloudData\\3-Mountain\\2.las",
            "D:\\PointCloudData\\3-Mountain\\3.las",
            "D:\\PointCloudData\\3-Mountain\\4.las"]
df = load_las_list_to_dataframe(las_list)
print(df.head())
print(df.describe(percentiles=[0.01, 0.05, 0.95, 0.99]))
print(len(df))
stats = grid_occupancy_analysis(df)
print(stats)
clipped_df = quantile_clip(df, dims=("x", "y", "z"), q_low = 0.01, q_high = 0.99)
downsample_df = uniform_downsample_preserve_order(clipped_df, target_n= 10000000)
stats = grid_occupancy_analysis(downsample_df)
print(stats)
bounds = get_pointcloud_bounds(downsample_df)
print(bounds)

bounds = {
    "x": (-60, 75.5),
    "y": (-73.5, 77.5),
    "z": (-2, 48.5),
}

ok = check_df_within_bounds(downsample_df, bounds)

# if not ok:
#     raise RuntimeError("Downsampled point cloud violates LOW/TOP bounds")
# write_dataframe_to_csv(downsample_df,
#                            path="D:\\PycharmPro\\PythonProject\\PointCloud\\PointCloudData\\Mountain.csv")

# df = load_las_to_dataframe("D:\\PointCloudData\\8-RiverBank\\01.las")
# clipped_df = quantile_clip(df, dims=("x", "y", "z"), q_low = 0.01, q_high = 0.99)
# downsample_df = uniform_downsample_preserve_order(clipped_df, target_n= 10000000)
# stats = grid_occupancy_analysis(downsample_df)
# print(stats)
# bounds = get_pointcloud_bounds(downsample_df)
# print(bounds)

# bounds = {
#     "x": (-340.0, -175.0),
#     "y": (  60.0,  220.0),
#     "z": (  -9.5,    6.5),
# }

# ok = check_df_within_bounds(downsample_df, bounds)
#
# if not ok:
#     raise RuntimeError("Downsampled point cloud violates LOW/TOP bounds")
# write_dataframe_to_csv(downsample_df,
#                            path="D:\\PycharmPro\\PythonProject\\PointCloud\\PointCloudData\\RiverBank.csv")

