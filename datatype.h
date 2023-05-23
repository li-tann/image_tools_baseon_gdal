#ifndef DATATYPE
#define DATATYPE

#include <vector>
#include <string>
#include <complex>

using namespace std;

struct funcrst{
    funcrst() {};
    funcrst(bool b, std::string s) { result = b; explain = s; }
    bool result{ false };
    std::string explain{ "" };
    operator bool() { return result; }
};


void strSplit(std::string input, std::vector<std::string>& output, std::string split, bool clearVector = true);
double spend_time(decltype (std::chrono::system_clock::now()) start);


template<typename _Ty>
inline size_t dynamic_array_size(_Ty* array)
{
    return _msize(array) / sizeof(*array);
}

/// @brief RGBA
/// r,g,b: 0~255, a:0~1(0是透明, 1是全实)
struct rgba {
	int red, green, blue, alpha;
	rgba():red(0), green(0), blue(0), alpha(0) {};
	rgba(int r, int g, int b, double a) :
		red(r > 255 ? 255 : (r < 0 ? 0 : r)), 
		green(g > 255 ? 255 : (g < 0 ? 0 : g)),
		blue(b > 255 ? 255 : (b < 0 ? 0 : b)),
		alpha(a > 255 ? 255 : (a < 0 ? 0 : a)) {}
	rgba(int r, int g, int b) :
		red(r > 255 ? 255 : (r < 0 ? 0 : r)),
		green(g > 255 ? 255 : (g < 0 ? 0 : g)),
		blue(b > 255 ? 255 : (b < 0 ? 0 : b)),
		alpha(255) {}
};

class color_map_v2
{
public:
	color_map_v2(const char* color_map_filepath);
	~color_map_v2();

	/// @brief  如果node, color 数组异常, 返回false, 否则为true
	/// @return 
	funcrst is_opened();

	/// @brief 等比例映射, 修改颜色表中的node, cm + mapping 可以约等于cpt
	funcrst mapping(float node_min, float node_max);

	/// @brief 输入value值, 匹配对应的rgba
	rgba mapping_color(float);

	float* node = nullptr;
	rgba* color = nullptr;

	/// rgba.size = node.size + 1, cause:
	///			  node[0]		node[1]			node[2]		...		node[n-1]		node[n]
	/// 	color[0]		color[1]		color[2]	...		color[n-1]		color[n]		color[n+1]
	/// color[0]: out_of_range(left) ; 
	/// color[n+1]: out_of_range(right)
	/// 向上兼容关系, 即 if( node[0] < value  && value <= node[1] ) color = color[1]
	 
	void print_colormap();

private:
	void load_cm(const char* cm_path);
	void load_cpt(const char* cpt_path);
};

/// @brief 通过创建直方图, 对影像进行百分比拉伸, 并获取拉伸后的最值
/// @param rb RasterBand, 图像的某个波段
/// @param histogram_size 直方图长度, 通常为100~256
/// @param stretch_rate 拉伸比例, 通常为 0.02
/// @param min 要输出的最小值
/// @param max 要输出的最大值
/// @return 
funcrst cal_stretched_minmax(GDALRasterBand* rb, int histogram_size, double stretch_rate, double& min, double& max);

#endif
