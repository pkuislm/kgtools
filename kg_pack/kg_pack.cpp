#include <png.h>
#include <stdlib.h>
#include <string.h>
#include "pngstruct.h"
#include "pnginfo.h"
#include <fstream>

typedef unsigned char  unit8;
typedef unsigned short unit16;
typedef unsigned int   unit32;
typedef unsigned __int64 unit64;

struct kgheader {
	char magic[4];
	unit16 width;
	unit16 height;
	unit32 image_size;
};

const char magic[] = { 0x47, 0x43, 0x47, 0x4B };

//被压缩数据的基本长度是2（alpha和alpha_count）。随后附加 n * 3 的RGB数值。
//首先计算长度并分配空间。
//读取一行像素。记录首个像素alpha。查找后一个像素alpha，比较是否相同。相同则alpha_count++，继续向后查找。
//当这行第n个像素的alpha与前面都不一样时，停止查找。依次写入[alpha][alpha_count][每一像素RGB值]
//特别注意当像素alpha为0时不需要写入RGB数据，且alpha_count最大为256,写入数据为0x00
//上面步骤重复到该行全部判断完毕。
//offset_table随行数增加，每元素数值等于当前行完成压缩前的size。

void ReadPng(FILE* pngfile,const char*lpFileName)
{
	unit64 pixelR, pixelG, pixelB, pixelA;
	png_bytep row_buf;	//存放当前行的缓冲区
	unit8* pSrc;		//用于指向row_buf的指针
	png_structp png_ptr;
	png_infop info_ptr;


	png_byte sig[8];
	fread(sig, 1, 8, pngfile);
	if (png_sig_cmp(sig, 0, 8)) {
		fclose(pngfile);
		return;
	}


	png_ptr = png_create_read_struct(png_get_libpng_ver(NULL), NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);
	setjmp(png_jmpbuf(png_ptr));
	png_init_io(png_ptr, pngfile);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	printf("%s(%s)PNG INFO:\n", __FILE__, __FUNCTION__);
	printf("pixel_depth = %d\n", info_ptr->pixel_depth);
	printf("bit_depth = %d\n", info_ptr->bit_depth);
	printf("width = %d\n", info_ptr->width);
	printf("height = %d\n", info_ptr->height);
	
	//打开输出文件
	std::ofstream out;
	out.open(lpFileName, std::ios::binary);

	kgheader k;
	//memcpy(k.magic, magic, 4);
	k.width = info_ptr->width;
	k.height = info_ptr->height;
	//unit32 size = 0;//大小计数

	//填充位置到达bitmap_data处
	char cc[1] = { 0xcc };
	for (int i = 0; i < sizeof(k) + 4 * k.height; i++) {
		out.write(cc,1);
	}
	//out.flush();
	//每一行的，pSrc
	int p = 0;
	//上一个offset入口
	int pprev =0;

	//存放每行像素起始地址的table
	unit32* offset_table = new unit32[info_ptr->height];

	//为每一行的像素分配空间
	row_buf = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));
	for (int i = 0; i < info_ptr->height; i++) {
		p = 0;
		
		png_read_rows(png_ptr, (png_bytepp)&row_buf, NULL, 1);
		//printf("Rows:%d:\n", i);
		//记录当前所在的像素位置
		int pixpos = 1;
		//保存下入口到table
		offset_table[i] = pprev;
	seg1:
		//alpha计数从1开始
		int count = 1;
		//获取第一个像素alpha
		int alpha = row_buf[p+3];
		p += 4;

		//row_buf:[RR][GG][BB][AA]|[RR][GG][BB][AA]|[RR][GG][BB][AA]
		//		   ↑	        	|______ |______
		//        pSrc					  | 	  |
		//compressed:[AA][CC][RR][GG][BB][RR][GG][BB]

		//如果后一个像素alpha与之前的相同就计数，直到不相同
		//同时也是计算下压缩后长度
		while (pixpos < info_ptr->width) {
			if (row_buf[p + 3] == alpha) {
				p += 4;
				count++;
				pixpos++;
			}
			else {
				pixpos++;
				break;
			}
			
			//如果同一个像素过多，超过256个
			if (count == 256) {
				break;
				//break之后后面的像素重新进行判断，不管前面像素是否与其相同
			}
		}
		//printf("Alpha:%02X,Count:%04d,Pixpos:%04d\n", alpha,count,pixpos);
		//当透明度不为0时（有颜色时）
		if (alpha != 0) {
			pprev += 2 + count * 3;
			//预分配大小
			unit8* compressed = new unit8[2 + count * 3];
			//填充数据
			compressed[0] = alpha;
			//等于256的数字以0代替
			if (count == 256) {
				compressed[1] = 0;
			}
			else {
				compressed[1] = count;
			}
			int orgoffset = p - 4 * count;
			int px = 0;
			for (int o = 0; o < count*4;) {
				compressed[2 + 0 + px] = row_buf[orgoffset + o + 0];
				compressed[2 + 1 + px] = row_buf[orgoffset + o + 1];
				compressed[2 + 2 + px] = row_buf[orgoffset + o + 2];
				//        ↑  ↑  ↑              ↑        ↑   ↑
				//alpha+count rgb  offset      offset     index  rgb
				o += 4;
				px += 3;
			}
			//写入
			out.write((const char*)compressed, count * 3 + 2);
			//out.flush();
			//释放压缩内容空间
			delete[] compressed;
		}
		//没颜色就直接写alpha和count了
		else {
			pprev += 2;
			out.write((const char*)&alpha, 1);
			if (count == 256){
				out.write("\0",1);
			}
			else{
				out.write((const char*)&count, 1);
			}
		}
		//out.flush();
		//如果这一行还没读完就继续读
		if (pixpos != info_ptr->width)
			goto seg1;

	}

	//写入文件头以及offset_table
	k.image_size = pprev;
	out.seekp(std::ios::beg);
	out.write(magic, 4);
	out.write((const char*)&k.width, 2);
	out.write((const char*)&k.height, 2);
	out.write((const char*)&k.image_size, 4);
	out.write((const char*)offset_table, info_ptr->height * sizeof(unit32));
	out.close();
	png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(row_buf);
}

int main(int argc,char**argv) {
	if (argc > 1) {
		std::string filename(argv[1]);
		std::string kgfilename = filename.substr(0, filename.find_last_of(".")) + ".kg";
		FILE* fp = fopen(filename.c_str(), "rb");
		ReadPng(fp, kgfilename.c_str());
	}
	else {
		printf("Usage: kg_depack.exe [kg_file]\n");
	}
	return 0;
	//FILE* fp = fopen("E:\\GalGames_Work\\OnWork\\游乐园里的撒娇鬼\\chip_unpack\\testbench\\CFGChip.png","rb");
	//ReadPng(fp, (char*)"E:\\GalGames_Work\\OnWork\\游乐园里的撒娇鬼\\chip_unpack\\testbench\\0001.kg");
}