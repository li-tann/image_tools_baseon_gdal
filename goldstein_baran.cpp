#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <filesystem>
#include <complex>
#include <omp.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fmt/format.h>

#include <gdal_priv.h>

#include <fftw3.h>

#include "datatype.h"

#define EXE_NAME "baran"

// #define DEBUG

using namespace std;
namespace fs = std::filesystem;

string EXE_PLUS_FILENAME(string extention){
    return string(EXE_NAME)+"."+ extention;
}

funcrst baran(std::complex<float>* arr_in, float* cor, int height, int width, std::complex<float>* arr_out, float* arr_alpha_out );

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

    if(argc < 5){
        msg =   EXE_PLUS_FILENAME("exe\n");
        msg +=  " manual: " EXE_NAME " [input] [params] [output]\n" 
                " argv[0]: " EXE_NAME ",\n"
                " argv[1]: input, fcpx interf filepath.\n"
                " argv[2]: input, flt, correlation filepath.\n"
                " argv[3]: output, fcpx filtered interf filepath.\n"
				" argv[4]: output, flt alpha filepath.";
        return return_msg(-1,msg);
    }

    return_msg(0,EXE_NAME " start.");

	GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset* ds = (GDALDataset*)GDALOpen(argv[1], GA_ReadOnly);
    if(!ds){
        return return_msg(-2, "ds is nullptr.");
    }
    GDALRasterBand* rb = ds->GetRasterBand(1);

    int width = ds->GetRasterXSize();
    int height= ds->GetRasterYSize();
    GDALDataType datatype = rb->GetRasterDataType();

	if(datatype != GDT_CFloat32){
		return return_msg(-2, "datatype is diff with float(not support yet) or fcomplex.");
	}

	std::complex<float>* arr = new std::complex<float>[width * height];
	std::complex<float>* arr_out = new std::complex<float>[width * height];
	float* arr_alpha = new float[width * height];
	rb->RasterIO(GF_Read, 0, 0, width, height, arr, width, height, datatype, 0, 0);


    /// TODO: load correlation map
	float* arr_cor = new float[width * height];
	GDALDataset* ds_cor = (GDALDataset*)GDALOpen(argv[2], GA_ReadOnly);
	if(!ds_cor){
		GDALClose(ds);
		return return_msg(-2, "ds_cor is nullptr.");
	}
	GDALRasterBand* rb_cor = ds_cor->GetRasterBand(1);

	if(ds_cor->GetRasterXSize() != width){
		GDALClose(ds);
		GDALClose(ds_cor);
		return return_msg(-2, fmt::format("ds_cor.width({}) is diff with ds.width({}).",ds_cor->GetRasterXSize(), width));
	}

	if(ds_cor->GetRasterYSize() != height){
		GDALClose(ds);
		GDALClose(ds_cor);
		return return_msg(-2, fmt::format("ds_cor.height({}) is diff with ds.height({}).",ds_cor->GetRasterYSize(), height));
	}
	if(rb_cor->GetRasterDataType() != GDT_Float32){
		GDALClose(ds);
		GDALClose(ds_cor);
		return return_msg(-2, "datatype is not float.");
	}
	rb_cor->RasterIO(GF_Read, 0, 0, width, height, arr_cor, width, height, GDT_Float32, 0, 0);
	GDALClose(ds_cor);

	auto rst = baran(arr, arr_cor, height, width, arr_out, arr_alpha);
    return_msg(1,rst.explain); 

    delete[] arr;
	delete[] arr_cor;
	GDALClose(ds);

	GDALDriver* dv = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* ds_out = dv->Create(argv[3], width, height, 1, datatype, NULL);
    if(!ds_out){
		delete[] arr_out;
        return return_msg(-3, "ds_out create failed.");
    }
    GDALRasterBand* rb_out = ds_out->GetRasterBand(1);

	rb_out->RasterIO(GF_Write, 0, 0, width, height, arr_out, width, height, datatype, 0, 0);

	delete[] arr_out;
	GDALClose(ds_out);

	GDALDataset* ds_alpha_out = dv->Create(argv[4], width, height, 1, GDT_Float32, NULL);
    if(!ds_out){
		delete[] arr_alpha;
        return return_msg(-3, "ds_alpha_out create failed.");
    }
    GDALRasterBand* rb_alpha_out = ds_alpha_out->GetRasterBand(1);

	rb_alpha_out->RasterIO(GF_Write, 0, 0, width, height, arr_alpha, width, height, GDT_Float32, 0, 0);

	delete[] arr_alpha;
	GDALClose(ds_alpha_out);
    
    return return_msg(1, EXE_NAME " end.");
}

funcrst conv_2d(float* arr_in, int width, int height, float* arr_out, float* kernel, int size)
{
	if(arr_in == nullptr)
		return funcrst(false, "filter::conv_2d, arr_in is nullptr.");
	
	if(dynamic_array_size(arr_in) != width * height)
		return funcrst(false, fmt::format("filter::conv_2d, arr_in.size({}) is diff with width*height({}).",dynamic_array_size(arr_in),width * height));
	
	if(kernel == nullptr)
		return funcrst(false, "filter::conv_2d, kernel is nullptr.");

	if(dynamic_array_size(kernel) != size * size)
		return funcrst(false, fmt::format("filter::conv_2d, kernel.size({}) is diff with size^2({}).",dynamic_array_size(kernel),size*size));

	float* kernel_overturn = new float[size*size];
	for(int i=0; i<size*size; i++)
		kernel_overturn[i] = kernel[size*size-1-i];

	if(arr_out == nullptr){
		arr_out = new float[height * width];
	}
	else if(dynamic_array_size(arr_out) != height * width){
		delete[] arr_out;
		arr_out = new float[height * width];
	}

	for(int i = 0; i < height; i++){
		for(int j = 0; j < width; j++){
			/// 这种重复计算的方式肯定会多耗费一些时间, 如果使用同行向右滑动, 逐列增减数据的方式, 可以大大减少耗时
			float sum = 0;
			for(int m = 0; m< size; m++){
				for(int n = 0; n< size; n++){
					if(i-size/2+m < 0 || i-size/2+m > height-1 || j-size/2+n < 0 || j-size/2+n > width-1)
						continue;/// 超界
					sum +=  kernel_overturn[m*size+n] * arr_in[(i-size/2+m)*width+(j-size/2+n)];
				}
			}
			arr_out[i*width+j]=sum;
		}
	}

	delete[] kernel_overturn;
	return funcrst(true, "filter::conv_2d finished.");
}

funcrst baran(std::complex<float>* arr_in, float* cor, int height, int width, std::complex<float>* arr_out, float* arr_alpha_out)
{
	auto start_time = std::chrono::system_clock::now();

	int size = 32;
	int overlap = 24;
	int step = size - overlap;

	if(arr_out == nullptr){
		arr_out = new std::complex<float>[height * width];
	}
	else if(dynamic_array_size(arr_out) != height * width){
		delete[] arr_out;
		arr_out = new std::complex<float>[height * width];
	}

	int max_threads = omp_get_max_threads();

	fftwf_complex** spatial_arrs =  new fftwf_complex*[max_threads];
	fftwf_complex** frequency_arrs =  new fftwf_complex*[max_threads];
	fftwf_plan* forward_plans = new fftwf_plan[max_threads];
	fftwf_plan* backward_plans = new fftwf_plan[max_threads];
	for(int i=0; i < max_threads; i++){
		spatial_arrs[i] = fftwf_alloc_complex(size*size);
		frequency_arrs[i] = fftwf_alloc_complex(size*size);
		forward_plans[i] = fftwf_plan_dft_2d(size, size, spatial_arrs[i], frequency_arrs[i], FFTW_FORWARD, FFTW_ESTIMATE);
		backward_plans[i] = fftwf_plan_dft_2d(size, size, frequency_arrs[i], spatial_arrs[i], FFTW_BACKWARD, FFTW_ESTIMATE);
	}
	
#pragma omp parallel for
	for(int i=0; i < height; i+=step)
	{
		/// out_i_start, out_i_end, 控制block数组内需要赋值到arr_out的行数, 保证输出数据没有"黑框"
		int out_i_start, out_i_end;
		if(i == 0){
			out_i_start = 0; out_i_end = size - overlap / 2 - 1;
		}
		else if(i + step - 1 > height - 1){
			out_i_start = overlap / 2; out_i_end = height - 1 - i;
		}
		else{
			out_i_start = overlap / 2; out_i_end = size - overlap / 2 - 1;
		}
		
		int thread_idx = omp_get_thread_num();


		for(int j=0; j < width; j+=step)
		{
			/// out_j_start, out_j_end, 控制block数组内需要赋值到arr_out的列数, 保证输出数据没有"黑框"
			int out_j_start, out_j_end;
			if(j == 0){
				out_j_start = 0; out_j_end = size - overlap / 2 - 1;
			}
			else if(j + step - 1 > width - 1){
				out_j_start = overlap / 2; out_j_end = width - 1 - j;
			}
			else{
				out_j_start = overlap / 2; out_j_end = size - overlap / 2 - 1;
			}

			/// spatial_arr init & 
			/// calculate average correlation of overlap area to replace alpha in goldstein
			int num = 0;
			float alpha = 0;
			for(int k = 0; k< size*size; k++){
				int block_i = k / size + i;
				int block_j = k % size + j;
				if(block_j > width - 1 || block_i > height - 1){
					/// 说明超界, 需要补零
					spatial_arrs[thread_idx][k][0]=0;
					spatial_arrs[thread_idx][k][1]=0;
				}
				else{
					spatial_arrs[thread_idx][k][0]=arr_in[block_i * width + block_j].real();
					spatial_arrs[thread_idx][k][1]=arr_in[block_i * width + block_j].imag();
					
					alpha += cor[block_i * width + block_j];
					num++;
				}
			}

			alpha = (num==0 ? 0 : alpha / num);
			alpha = alpha > 1 ? 1 : (alpha < 0 ? 0 : alpha);
			alpha = 1 - alpha;
			for(int m = out_i_start; m <= out_i_end; m++){
				for(int n = out_j_start; n <= out_j_end; n++){
                    if(i + m < 0 || i + m > height - 1 || j + n < 0 || j + n > width - 1)
                        continue;
					arr_alpha_out[(i+m)*width+(j+n)] = alpha;
				}
			}

			///  fft
			fftwf_execute(forward_plans[thread_idx]);


			/// abs
			float* block_abs = new float[size*size];
			for(int k=0; k<size*size; k++){
				block_abs[k] = sqrtf(powf(frequency_arrs[thread_idx][k][0],2) + powf(frequency_arrs[thread_idx][k][1],2));
			}

			/// smooth
			float* smooth_spatial = new float[25];
			for(int k = 0; k<25; k++)
				smooth_spatial[k] = 0.04;
			float* block_smooth = new float[size*size];
			// conv_2d(block_abs, size, size, block_smooth, smooth_frequency, 5);
            auto rst = conv_2d(block_abs, size, size, block_smooth, smooth_spatial, 5);

			delete[] block_abs;
			delete[] smooth_spatial;


			/// block_smooth^alpha * spatial_arrs -> frequency_arrs
			for(int k=0; k< size*size; k++){
				frequency_arrs[thread_idx][k][0] = pow(block_smooth[k],alpha) * frequency_arrs[thread_idx][k][0];
				frequency_arrs[thread_idx][k][1] = pow(block_smooth[k],alpha) * frequency_arrs[thread_idx][k][1];
			}

			delete[] block_smooth;


			/// ifft
			fftwf_execute(backward_plans[thread_idx]);


			/// 赋值
			for(int m = out_i_start; m <= out_i_end; m++){
				for(int n = out_j_start; n <= out_j_end; n++){
                    if(i + m < 0 || i + m > height - 1 || j + n < 0 || j + n > width - 1)
                        continue;
					arr_out[(i+m)*width+(j+n)].real(spatial_arrs[thread_idx][m*size+n][0] / size / size);
					arr_out[(i+m)*width+(j+n)].imag(spatial_arrs[thread_idx][m*size+n][1] / size / size);
				}
			}


		}
		
	}

	for(int i=0; i< max_threads; i++){
		fftwf_destroy_plan(forward_plans[i]);
		fftwf_destroy_plan(backward_plans[i]);
		fftwf_free(spatial_arrs[i]);
		fftwf_free(frequency_arrs[i]);
	}
	

    double spend_sec = spend_time(start_time);
    cout<<"glodstein_single spend_time: "<<spend_sec<<endl;

	return funcrst(true, "filter::goldstein finished.");
}