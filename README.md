# blosc2-bench
Data and environmental setup to run benchmarks against blosc2

## Initial setup
First, install GDAL to parse the input files.
```
sudo dnf install gdal-devel
```
Grab the input files from the respective agencies.
Run the processing scripts to produce the datasets.
```
create_rea6 TOT_PRECIP.2D.201512.grb
create_era5 data.grib
```

The `REA6_precip` dataset is taken from [Breaking Down Memory Walls](https://www.blosc.org/docs/Breaking-Down-Memory-Walls.pdf). The code they used is [here](https://github.com/Blosc/c-blosc2/blob/main/bench/read-grid-150x150.py). Instead of only evaluating over the small 20KiB section of one sample, we evaluate over the entire dataset of 744 samples from the COSMO-REA6 precipitation dataset.

The other datasets are taken from the [bytedelta analysis](https://www.blosc.org/posts/bytedelta-enhance-compression-toolset/). Here the [code](https://github.com/Blosc/python-blosc2/blob/main/bench/ndarray/download_data.py) they used no longer works. They don't publish the English names of the datasets they are using, so we can only guess to our best ability which datasets they have pulled. We use the following sample from the ERA5 reanalysis (shortname names):
- 10 metre u wind component (ERA5_wind)
- Mean sea level pressure (ERA5_pressure)
- Total precipitation (ERA5_precip)
- Downward UV radiation at the surface (ERA5_flux)
- Snow density (ERA5_snow)

### Some stats on the corpora
| File prefix | Size | X | Y |
| --- | --- | - | - |
| REA6_precip | 5590016 | 848 | 824 |
| ERA5_* | 8305920 | 1440 | 721 |

## How to run benchmarks
First, create a python virtual environment.
Then, download blosc-btune
```
pip install blosc2-btune
```
