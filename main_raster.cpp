/**
 * @file main_raster.cpp
 * @author li-tann (li-tann@github.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

#include <gdal_priv.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <argparse/argparse.hpp>

#include "raster_include.h"
#include "datatype.h"

namespace fs = std::filesystem;
using std::cout, std::cin, std::endl, std::string, std::vector, std::map;

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("gdal_tool_raster","1.0");
    program.add_description("gdal_tools about raster data, ...");
    
    argparse::ArgumentParser sub_value_translate("value_translate");
    sub_value_translate.add_description("translate the source value in input image to  target value in output image.");
    {
        sub_value_translate.add_argument("input_imgpath")
            .help("the original image with the source value.");

        sub_value_translate.add_argument("output_imgpath")
            .help("the target image, 如果output_imgpath == input_imgpath, 则直接对源数据进行操作.");

        sub_value_translate.add_argument("source_value")
            .help("source_value")
            .scan<'g',double>();
        
        sub_value_translate.add_argument("target_value")
            .help("target_value")
            .scan<'g',double>();
    }

    argparse::ArgumentParser sub_set_nodata("set_nodata");
    sub_set_nodata.add_description("set 'nodata value' metadata within image.");
    {
        sub_set_nodata.add_argument("img_filepath")
            .help("raster image filepath.");

        sub_set_nodata.add_argument("nodata_value")
            .help("nodata value")
            .scan<'g',double>();
    }

    argparse::ArgumentParser sub_statistics("statistics");
    sub_statistics.add_description("statistics on anyone band of the image.");
    {
        sub_statistics.add_argument("img_filepath")
            .help("raster image filepath.");

        sub_statistics.add_argument("band")
            .help("band, default is 1.")
            .scan<'i',int>()
            .default_value("1");
    }

    argparse::ArgumentParser sub_histogram_stretch("histogram_stretch");
    sub_histogram_stretch.add_description("remove the extreme values at both ends of the image based on proportion.");
    {
        sub_histogram_stretch.add_argument("input_imgpath")
            .help("input image filepath, doesn't support complex datatype");

        sub_histogram_stretch.add_argument("output_imgpath")
            .help("output image filepath.");

        sub_histogram_stretch.add_argument("stretch_rate")
            .help("double, within (0,0.5], default is 0.02.")
            .scan<'g',double>()
            .default_value("0.2");    
    }

    argparse::ArgumentParser sub_histogram_statistics("histogram_statistics");
    sub_histogram_statistics.add_description("histogram statistics.");
    {
        sub_histogram_statistics.add_argument("input_imgpath")
            .help("input image filepath, doesn't support complex datatype");

        sub_histogram_statistics.add_argument("histogram_size")
            .help("int, histogram size, more than 1, default is 256.")
            .scan<'i',int>()
            .default_value("256");    
    }

    argparse::ArgumentParser sub_vrt_to_tif("vrt_to_tif");
    sub_vrt_to_tif.add_description("vrt image trans to tif");
    {
        sub_vrt_to_tif.add_argument("vrt")
            .help("input image filepath (*.vrt)");

        sub_vrt_to_tif.add_argument("tif")
            .help("output image filepath (*.tif)");    
    }

    argparse::ArgumentParser sub_tif_to_vrt("tif_to_vrt");
    sub_tif_to_vrt.add_description("tif image trans to vrt(binary), only trans the first band.");
    {
        sub_tif_to_vrt.add_argument("tif")
            .help("input image filepath");

        sub_tif_to_vrt.add_argument("binary")
            .help("output image filepath (binary, without extension)");
            
        sub_tif_to_vrt.add_argument("ByteOrder")
            .help("MSB(Most Significant Bit) or LSB(Least Significant Bit)")
            .default_value("MSB");
    }

    argparse::ArgumentParser sub_over_resample("over_resample");
    sub_over_resample.add_description("over resample by gdalwarp.");
    {
        sub_over_resample.add_argument("input_imgpath")
            .help("input image filepath");

        sub_over_resample.add_argument("output_imgpath")
            .help("output image filepath");
            
        sub_over_resample.add_argument("scale")
            .help("scale of over-resample.")
            .scan<'g',double>();

         sub_over_resample.add_argument("method")
            .help("over-resample method, use int to represent method : 0,nearst; 1,bilinear; 2,cubic; 3,cubicSpline; 4,lanczos(sinc).; 5,average.")
            .scan<'i',int>()
            .default_value("1");       
    }

    argparse::ArgumentParser sub_trans_geoinfo("trans_geoinfo");
    sub_trans_geoinfo.add_description("copy source's geoinformation to target");
    {
        sub_trans_geoinfo.add_argument("source")
            .help("source image");

        sub_trans_geoinfo.add_argument("target")
            .help("target image");    
    }

                    " argv[1]: input, image filepath .\n"
                " argv[2]: input, 4 int parameters splited by ',': start_x, start_y, width, height .\n"
                " argv[3]: output, cutted image filepath.";
    argparse::ArgumentParser sub_image_cut_pixel("image_cut_pixel");
    sub_image_cut_pixel.add_description("cut image by pixel");
    {
        sub_image_cut_pixel.add_argument("input_imgpath")
            .help("input image filepath");

        sub_image_cut_pixel.add_argument("output_imgpath")
            .help("output image filepath (*.tif)");   

        sub_image_cut_pixel.add_argument("pars")
            .help("4 pars with the order like: start_x, start_y, width, height")
            .scan<'i',int>()
            .nargs(4);        
    }


    std::map<argparse::ArgumentParser* , 
            std::function<int(argparse::ArgumentParser* args,std::shared_ptr<spdlog::logger>)>> 
    parser_map_func = {
        {&sub_value_translate,      value_translate},
        {&sub_set_nodata,           set_nodata_value},
        {&sub_statistics,           statistics},
        {&sub_histogram_stretch,    histogram_stretch},
        {&sub_histogram_statistics, histogram_statistics},
        {&sub_vrt_to_tif,           vrt_to_tif},
        {&sub_tif_to_vrt,           tif_to_vrt},
        {&sub_over_resample,        over_resample},
        {&sub_trans_geoinfo,        trans_geoinformation},
        {&sub_image_cut_pixel,      image_cut_by_pixel},
    };

    for(auto prog_map : parser_map_func){
        program.add_subparser(*(prog_map.first));
    }

    // std::initializer_list<argparse::ArgumentParser*> sub_programs{
    //     &sub_value_translate, 
    //     &sub_set_nodata, 
    //     &sub_statistics,
    //     &sub_histogram_stretch,
    // };

    // for(auto prog : sub_programs){
    //     program.add_subparser(*prog);
    // }


    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl<<std::endl;
        for(auto prog_map : parser_map_func){
            if(program.is_subcommand_used(*(prog_map.first))){
                std::cerr << *(prog_map.first) <<std::endl;
                return 1;
            }
        }
        std::cerr << program;
        return 1;
    }

    

    /// log
    char* pgmptr = 0;
    _get_pgmptr(&pgmptr);
    fs::path exe_root(fs::path(pgmptr).parent_path());
    fs::path log_path = exe_root / "gdal_tool_raster.log";
    auto my_logger = spdlog::basic_logger_mt("gdal_tool_raster", log_path.string());

    std::string config;
    for(int i=0; i<argc; i++){
        config += std::string(argv[i]) + " ";
    }
    PRINT_LOGGER(my_logger, info, "gdal_tool_raster start");
    PRINT_LOGGER(my_logger, info, fmt::format("config:[{}]",config));
    auto time_start = std::chrono::system_clock::now();

    for(auto& iter : parser_map_func){
        if(program.is_subcommand_used(*(iter.first))){
            return iter.second(iter.first, my_logger);
        } 
    }
    
    PRINT_LOGGER(my_logger, info, fmt::format("gdal_tool_raster end, spend time {}s",spend_time(time_start)));
    return 0;
}