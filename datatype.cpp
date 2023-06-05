#include "datatype.h"
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <fstream>
#include <gdal_priv.h>

namespace fs = std::filesystem;

void strSplit(std::string input, std::vector<std::string>& output, std::string split, bool clearVector)
{
    if(clearVector)
        output.clear();
    std::string::size_type pos1, pos2;
    pos1 = input.find_first_not_of(split);
    pos2 = input.find(split,pos1);

    if (pos1 == std::string::npos) {
        return;
    }
    if (pos2 == std::string::npos) {
        output.push_back(input.substr(pos1));
        return;
    }
    output.push_back(input.substr(pos1, pos2 - pos1));
    strSplit(input.substr(pos2 + 1), output, split,false);
    
}

double spend_time(decltype (std::chrono::system_clock::now()) start)
{
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double second = double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
    return second;
}

color_map_v2::color_map_v2(const char* color_map_filepath)
{
    fs::path p(color_map_filepath);

    if(p.extension().string() == ".cm"){
        load_cm(color_map_filepath);
    }
    else if(p.extension().string() == ".cpt"){
        load_cpt(color_map_filepath);
    }
    else{
        return;
    }
}

color_map_v2::~color_map_v2()
{
    if (node != nullptr) {
        delete[] node;
    }

    if (color != nullptr) {
        delete[] color;
    }
}

funcrst color_map_v2::is_opened()
{
    std::string error_explain = "open failed, cause: ";
    bool return_bool = true;
    if (node == nullptr) {
        error_explain += "node is nullptr. ";
        return_bool = false;
    }

    if (color == nullptr) {
        error_explain += "color is nullptr. ";
        return_bool = false;
    }

    int node_size = int(_msize(node) / sizeof(*node));
    int color_size = int(_msize(color) / sizeof(*color));
    if (node_size + 1 != color_size) {
        error_explain += fmt::format("node.size({})+1 != color.size({})", node_size, color_size);
        return_bool = false;
    }

    return funcrst(return_bool, return_bool ? "is opened" : error_explain);
}

funcrst color_map_v2::mapping(float node_min, float node_max)
{
    funcrst rst = is_opened();
    if (!rst){
        return funcrst(false, fmt::format("mapping failed, cause there is not open, '{}'.",rst.explain));
    }

    int node_size = _msize(node) / sizeof(float);
    if (node_size < 2) {
        return funcrst(false, "node.size < 2, mapping failed.");
    }

    float* temp_node = new float[node_size];

    temp_node[0] = node_min;
    temp_node[node_size-1] = node_min;

    for (int i = 1; i < node_size; i++) {
        temp_node[i] = (node[i] - node[0]) / (node[node_size - 1] - node[0]) * (node_max - node_min) + temp_node[0];
    }

    for (int i = 0; i < node_size; i++) {
        node[i] = temp_node[i];
    }

    delete[] temp_node;
    return funcrst(true,"mapping success.");
}

rgba color_map_v2::mapping_color(float value)
{
    int node_size = _msize(node) / sizeof(float);
    if (value <= node[0])return color[0];
    if (value > node[node_size - 1])return color[node_size];
    for (int i = 0; i < node_size - 1; i++) {
        if (value > node[i] && value <= node[i + 1]) {
            return color[i + 1];
        }
    }
    return rgba(0, 0, 0, 0);
}

void color_map_v2::print_colormap()
{
    int node_size = _msize(node) / sizeof(float);
    std::cout << "node:\n";
    for (int i = 0; i < node_size; i++) {
        std::cout<<" node["<<i<<"]:" << node[i] << "\n";
    }
    std::cout << "\ncolor:\n";
    for (int i = 0; i < node_size+1; i++) {
        std::cout << " color[" << i << "]:" << color[i].red<<"," << color[i].green << "," << color[i].blue << "," << color[i].alpha  << "\n";
    }
}

void color_map_v2::load_cm(const char* cm_path)
{
    /// type:
    /// r g b
    /// r g b
    /// ...

    std::ifstream ifs(cm_path);
    if (!ifs.is_open()) {
        return ;
    }
    std::vector<std::string> texts;
    std::string str;
    while (std::getline(ifs, str)) {
        if (str[0] == '#')
            continue;
        texts.push_back(str);
    }
    ifs.close();

    if (texts.size() == 0) {
        return;
    }

    node = new float[texts.size()];
    color = new rgba[texts.size() + 1];

    for (int i = 0; i < texts.size(); i++) {
        std::vector<std::string> vec_splited;
        strSplit(texts[i], vec_splited, " ");
        if (vec_splited.size() < 3)
            continue;

        node[i] = i;
        color[i].red = stoi(vec_splited[0]);
        color[i].green = stoi(vec_splited[1]);
        color[i].blue = stoi(vec_splited[2]);
        color[i].alpha = 255;
    }
    color[texts.size()] = color[texts.size() - 1];

    return;
}
void color_map_v2::load_cpt(const char* cpt_path)
{
    /// type:
    /// #.....
    /// value r g b value r g b
    /// ...
    
    std::ifstream ifs(cpt_path);
    if (!ifs.is_open()) {
        return;
    }
    std::string str;
    std::vector<std::tuple<float, rgba>> temp;
    while (std::getline(ifs, str)) {
        if (str[0] == '#')
            continue;
        std::vector<std::string> vec_splited;
        strSplit(str, vec_splited, " ");
        if (vec_splited.size() < 4) {
            continue;
        }
        else if (vec_splited.size() < 8) {
            temp.push_back(std::make_tuple(stof(vec_splited[0]), rgba(stoi(vec_splited[1]), stoi(vec_splited[2]), stoi(vec_splited[3]))));
        }
        else {
            temp.push_back(std::make_tuple(stof(vec_splited[0]), rgba(stoi(vec_splited[1]), stoi(vec_splited[2]), stoi(vec_splited[3]))));
            temp.push_back(std::make_tuple(stof(vec_splited[4]), rgba(stoi(vec_splited[5]), stoi(vec_splited[6]), stoi(vec_splited[7]))));
        }
    }
    ifs.close();

    if (temp.size() == 0) {
        return;
    }

    node = new float[temp.size()];
    color = new rgba[temp.size() + 1];

    

    for (size_t i = 0; i < temp.size(); i++) {
        node[i] = std::get<0>(temp[i]);
        color[i] = std::get<1>(temp[i]);
    }
    color[temp.size()] = color[temp.size() - 1];
}


funcrst cal_stretched_minmax(GDALRasterBand* rb, int histogram_size, double stretch_rate, double& min, double& max)
{
	CPLErr cplerr;
	double minmax[2];
	cplerr = rb->ComputeRasterMinMax(false, minmax);
	if (cplerr == CE_Failure) {
		return funcrst(false, "rb.ComputeRasterMinMax failed.");
	}

	GUIntBig* histgram_result = new GUIntBig[histogram_size];
	cplerr = rb->GetHistogram(minmax[0], minmax[1], histogram_size, histgram_result, FALSE, FALSE, GDALDummyProgress, nullptr);
	if (cplerr == CE_Failure) {
		delete[] histgram_result;
		return funcrst(false, "rb.GetHistogram failed.");
	}

	GUIntBig* histgram_accumulate = new GUIntBig[histogram_size];
	histgram_accumulate[0] = histgram_result[0];
	for (int i = 1; i < histogram_size; i++) {
		histgram_accumulate[i] = histgram_accumulate[i - 1] + histgram_result[i];
	}
	/// 换算成百分比
	double* histgram_accumulate_percent = new double[histogram_size];
	bool update_min{ false }, update_max{ false };
    int index_min{-1}, index_max{-1};
	for (int i = 0; i < histogram_size; i++) {
		histgram_accumulate_percent[i] = 1. * histgram_accumulate[i] / histgram_accumulate[histogram_size - 1];
		if (i == 0)continue;
		if ((histgram_accumulate_percent[i - 1] <= stretch_rate || i == 1) && histgram_accumulate_percent[i] >= stretch_rate) {
			min = minmax[0] + (minmax[1] - minmax[0]) / histogram_size * (i - 1);
            index_min = i;
			update_min = true;
		}
		if (histgram_accumulate_percent[i - 1] <= 1 - stretch_rate && histgram_accumulate_percent[i] >= 1 - stretch_rate) {
			max = minmax[0] + (minmax[1] - minmax[0]) / histogram_size * i;
            index_max = i;
			update_max = true;
		}
	}

	delete[] histgram_result;
	delete[] histgram_accumulate;

	if (!update_min) {
		return funcrst(false, "min is not be updated.");
	}

	if (!update_max) {
		return funcrst(false, "max is not be updated.");
	}

	return funcrst(true, "cal_stretched_minmax success.");
}

/// egm2008

egm2008::egm2008(/* args */)
{
}

egm2008::~egm2008()
{
    if(arr == nullptr)
        delete[] arr;
}

funcrst egm2008::init(std::string path)
{
    ifstream ifs(path, ifstream::binary);
    if(!ifs.is_open()){
        lastError = "init, ifs.is_open return false.";
        return funcrst(false, lastError);
    }

    auto start = std::chrono::system_clock::now();

    std::string str;
    width = 0;
    height = 0;
    float value;
    zero_number = 0;
    bool stat_width = false;   
    while (ifs.read((char*)&value, 4)) { //一直读到文件结束
        if(value == 0){
            ++zero_number;
            if(zero_number == 1){
                stat_width = true;
            }
            if(zero_number == 2){
                stat_width = false;
            }
        }
        else{
            if(stat_width){
                ++width;
            }
        }
    }
    ifs.seekg(0,ios::beg);
    
    cout<<"zero_number:"<<zero_number<<endl;

    double seconds = spend_time(start);
    cout<<"init, spend_time: "<<seconds<<"s."<<endl;

    height = zero_number / 2;
    if(height != (width / 2 + 1)){
        lastError = fmt::format("init, height({}) != width({}) / 2 + 1\n",height, width);
        return funcrst(false, lastError);
    }

    spacing = double(360) / width; 

    /// 计算完height 和 width 后, 可以定义数组长度

    arr = new float[height * width];
    int num = 0;
    while (ifs.read((char*)&value, 4)){ //一直读到文件结束
        if(value == 0){
            continue;
        }
        else{
            if(num >= height * width){
                lastError = fmt::format("init, the number({}) of non-zero is more than height({}) * width({})", num, height, width);
                return funcrst(false,lastError);
            }
            arr[num] = value;
            ++num;
        }
    }

    ifs.close();
    return funcrst(true, "init, success.");
}

long egm2008::cal_off(size_t row, size_t col)
{
    if(row > height || col > width){
        return -1;
    }
    return row * (width + 2) + width + 1;
}