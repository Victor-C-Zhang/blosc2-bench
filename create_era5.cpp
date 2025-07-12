#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gdal_priv.h"

int main(int argc, char** argv) {
	std::string filename{argv[1]};
	GDALAllRegister();
    const GDALAccess eAccess = GA_ReadOnly;
    auto dataset = GDALDataset::FromHandle(GDALOpen( filename.c_str(), eAccess ));
    const auto numBands = dataset->GetRasterCount();
    int xsize = dataset->GetRasterXSize();
    int ysize = dataset->GetRasterYSize();
    {
        // sanity check
        auto band = dataset->GetRasterBand(1);
        std::cout << "x " << band->GetXSize() << " y " << band->GetYSize() << std::endl;
        std::cout << "x " << xsize << " y " << ysize << std::endl;
        std::cout << "nbands " << numBands << std::endl;
    }
    std::filesystem::path filenamePath(filename);
    std::array<std::string, 5> kindNames {
        "ERA5_wind",
        "ERA5_pressure",
        "ERA5_precip",
        "ERA5_flux",
        "ERA5_snow",
    };
    std::array<std::filesystem::path, 5> dirPaths {
        filenamePath.parent_path() / kindNames[0],
        filenamePath.parent_path() / kindNames[1],
        filenamePath.parent_path() / kindNames[2],
        filenamePath.parent_path() / kindNames[3],
        filenamePath.parent_path() / kindNames[4],
    };
    for (const auto& dirPath : dirPaths) {
        std::filesystem::create_directory(dirPath);
    }
    for (int i = 1; i <= numBands; ++i) {
        auto band = dataset->GetRasterBand(i);
        auto dt = band->GetRasterDataType();
        assert(dt == GDALDataType::GDT_Float64);
        assert(band->GetXSize() == xsize);
        assert(band->GetYSize() == ysize);

        if (1) {
            std::vector<double> vec(xsize * ysize);
            if (band->RasterIO(GF_Read, 0, 0, xsize, ysize, vec.data(), xsize, ysize, GDT_Float64, 0, 0) != CE_None) {
                std::cout << "OOPSZ " << i << std::endl;
                return 1;
            }
            // the bands loop through the 5 types round-robin style
            size_t whichKindIdx = (i - 1) % 5;
            auto dirPath = dirPaths[whichKindIdx];
            auto outPath = dirPath / (kindNames[whichKindIdx] + "_" + std::to_string(i) + ".bin");
            std::cout << "writing " << outPath << std::endl;
            std::ofstream out{outPath};
            out.write((const char*)vec.data(), xsize * ysize * sizeof(vec[0]));
            out.close();
        }
    }
    return 0;
}
