#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <filesystem>
#include <complex>
#include <omp.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <gdal_priv.h>

#include "datatype.h"

#define EXE_NAME "rolling_guidance_filter"

using namespace std;
namespace fs = std::filesystem;

string EXE_PLUS_FILENAME(string extention){
    return string(EXE_NAME)+"."+ extention;
}


int main(int argc, char* argv[])
{

    auto start = chrono::system_clock::now();
    string msg;

    auto my_logger = spdlog::basic_logger_mt(EXE_NAME, EXE_PLUS_FILENAME("txt"));

    auto return_msg = [my_logger](int rtn, string msg){
        my_logger->info(msg);
        spdlog::info(msg);
        return rtn;
    };

    if(argc < 4){
        msg =   EXE_PLUS_FILENAME("exe\n");
        msg +=  " manual: " EXE_NAME " [input] [method] [output]\n" 
                " argv[0]: " EXE_NAME ",\n"
                " argv[1]: input, fcpx filepath.\n"
                " argv[2]: method, 1:average_5, 2:average_7.\n"
                " argv[3]: output, fcpx filepath.";
        return return_msg(-1,msg);
    }

    return_msg(0,EXE_NAME " start.");

    
    
    return return_msg(1, EXE_NAME " end.");
}


funcrst rolling_guidance(float* arr_in, int height, int width, float* arr_out, int step, int size, double delta_s, double delta_r)
{
	auto is_point_in_image = [width, height]( int i, int j){
		if(i < 0 || j < 0 || i > height - 1 || j > width - 1)
			return false;
		return true;
	};

	//Step1: Small Structure Removal
	float* jt0 = new float[height * width];

#pragma omp parallel for
	for(int i = 0; i < height; i++){
		for(int j = 0; j < width; j++){

			float G = 0, K = 0;
			for (int m = -size; m <= size; m++){
				for (int n = -size; n <= size; n++){
					if(!is_point_in_image(i+m, j+n))
						continue;
					float temp = (float)exp(-(m*m+n*n) / 2. / pow(delta_s, 2));
					K += temp;
					G += temp * arr_in[(i+m)*width + (j+n)];
				}
			}
			jt0[i * width + j] = (K == 0. ? 0 : G / K);
		}
	}

	//Step2: Edge Recovery (iteration)
	int iter_num = 0;
	do{
		/// init jt1
		float* jt1 = new float[height * width];

#pragma omp parallel for
		for(int i = 0; i < height; i++){
			for(int j = 0; j < width; j++){

				float J = 0, K = 0;
				for (int m = -size; m <= size; m++){
					for (int n = -size; n <= size; n++){
						if(!is_point_in_image(i+m, j+n))
							continue;
						float temp = (float)exp(-(m*m+n*n) / 2. / pow(delta_s, 2) - pow(jt0[(i+m)*width+(j+n)] - jt0[i*width+j], 2) / (2 * pow(delta_r, 2)) );
						K += temp;
						J += temp * arr_in[(i+m)*width + (j+n)];
					}
				}
				
				jt1[i * width + j] = (K == 0. ? 0 : J / K);
			}
		}

		for(size_t i=0; i < height*width; i++){
			jt0[i] = jt1[i];
		}
		delete[] jt1;

		iter_num++;
	} while (iter_num < step);

	if(arr_out == nullptr){
		arr_out = new float[height * width];
	}
	else if(dynamic_array_size(arr_out) != height * width){
		delete[] arr_out;
		arr_out = new float[height * width];
	}

	for(size_t i=0; i < height * width; i++){
		arr_out[i] = jt0[i];
	}

	delete[] jt0;
	
	return funcrst(true, "rolling_guidance finished.");
}