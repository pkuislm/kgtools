#include <cstdio>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <png.h>
#include <fstream>
#include <string>
#include <iostream>

typedef unsigned char	 unit8;
typedef unsigned short	 unit16;
typedef unsigned int	 unit32;
typedef unsigned __int64 unit64;


struct kgheader {
	char magic[4];
	unit16 width;
	unit16 height;
	unit32 image_size;
};

const char magic[] = { 0x47, 0x43, 0x47, 0x4B };

void WritePng(FILE* Pngname, unit32 Width, unit32 Height, unit32 bpp, unit8* BitmapData)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unit32 i = 0;
	unit8 buff;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		printf("PNG信息创建失败!\n");
		exit(0);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		printf("info信息创建失败!\n");
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		exit(0);
	}
	png_init_io(png_ptr, Pngname);
	if (bpp == 32)
	{
		png_set_IHDR(png_ptr, info_ptr, Width, Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		for (i = 0; i < Width * Height; i++)
		{
			buff = BitmapData[i * 4 + 0];
			BitmapData[i * 4 + 0] = BitmapData[i * 4 + 2];
			BitmapData[i * 4 + 2] = buff;
		}
	}
	else if (bpp == 24)
	{
		png_set_IHDR(png_ptr, info_ptr, Width, Height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		for (i = 0; i < Width * Height; i++)
		{
			buff = BitmapData[i * 3 + 0];
			BitmapData[i * 3 + 0] = BitmapData[i * 3 + 2];
			BitmapData[i * 3 + 2] = buff;
		}
	}
	else
	{
		printf("不支持的bpp类型!bpp:%d\n", bpp);
		system("pause");
		exit(0);
	}
	png_write_info(png_ptr, info_ptr);

	for (i = 0; i < Height; i++)
		png_write_row(png_ptr, BitmapData + i * Width * bpp / 8);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
}



void process(std::string FileName, std::string PngName) {
	kgheader k;
	char buf[8];

	std::ifstream in;
	in.open(FileName.c_str(), std::ios::binary);
	in.read(k.magic, sizeof(k.magic));
	if (strncmp(k.magic, magic, 4) == 0) {
		in.read(buf, 8);
		memcpy((void*)&k.width, buf, 2);
		memcpy((void*)&k.height, buf + 2, 2);
		memcpy((void*)&k.image_size, buf + 4, 4);

	}
	unit8* compressed = new unit8[k.image_size];
	unit8* uncompressed = new unit8[4 * k.width * k.height];
	unit32 addr = 0;
	unit32 fpaddr = 0;
	printf("Width:%d\nHeight:%d\nImageSize:%04X\nRequested Uncompress Size:%04X\n", k.width, k.height, k.image_size, 4 * k.height * k.width);
	fpaddr = in.tellg();
	in.seekg(fpaddr + 4 * k.height, std::ios::beg);
	in.read((char*)compressed, k.image_size);
	in.seekg(fpaddr, std::ios::beg);
	unit8 alpha, count;
	unit32 pixel;
	unit32 udatap = 0;
	unit32 count32 = 0;
	for (int i = 0; i < k.height; i++) {
		in.read(buf, 4);
		memcpy((void*)&addr, buf, 4);
		unit32 cdatap = addr;
		udatap = 4 * k.width * i;
		for (int j = 0; j < k.width;) {

			//|alpha|num|...data*num|
			memcpy((void*)&alpha, compressed + cdatap, 1);
			memcpy((void*)&count, compressed + cdatap + 1, 1);
			cdatap += 2;

			//char类型会溢出
			if (count == 0) {
				count32 = 256;
			}
			else {
				count32 = count;
			}


			if (alpha != 0) {
				for (int i = 0; i < count32; i++) {
					//移位貌似行不通
					//pixel = (alpha << 18) | (compressed[cdatap] << 10) | (compressed[cdatap + 1] << 8) | compressed[cdatap + 2];
					//memcpy(uncompressed + udatap, &pixel, 4);

					uncompressed[udatap] = compressed[cdatap + 2];	//RB通道交换？
					uncompressed[udatap + 1] = compressed[cdatap + 1];
					uncompressed[udatap + 2] = compressed[cdatap];
					uncompressed[udatap + 3] = (char)alpha;
					//向前推进
					udatap += 4;
					cdatap += 3;
				}
			}
			else {
				pixel = 0;
				for (int i = 0; i < count32; i++) {
					memcpy(uncompressed + udatap, &pixel, 4);
					//填充0
					udatap += 4;
				}
			}
			j += count32;
		}

		printf("Line:%d,Offset:%04X,Compressed_pointer:%04X,Uncompressed_pointer:%04X\n", i + 1, addr, cdatap, udatap);
	}

	FILE* pngfp = fopen(PngName.c_str(), "wb");
	WritePng(pngfp, k.width, k.height, 32, uncompressed);
	in.close();
	fclose(pngfp);
	delete[] compressed;
	delete[] uncompressed;
}


int main(int argc, char** argv) {
	if (argc > 1) {
		std::string filename(argv[1]);
		std::string pngfilename = filename.substr(0, filename.find_last_of(".")) + ".png";
		process(filename, pngfilename);
	}
	else {
		printf("Usage: kg_depack.exe [kg_file]\n");
	}
	return 0;
}