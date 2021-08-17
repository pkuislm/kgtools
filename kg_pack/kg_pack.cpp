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

//��ѹ�����ݵĻ���������2��alpha��alpha_count������󸽼� n * 3 ��RGB��ֵ��
//���ȼ��㳤�Ȳ�����ռ䡣
//��ȡһ�����ء���¼�׸�����alpha�����Һ�һ������alpha���Ƚ��Ƿ���ͬ����ͬ��alpha_count++�����������ҡ�
//�����е�n�����ص�alpha��ǰ�涼��һ��ʱ��ֹͣ���ҡ�����д��[alpha][alpha_count][ÿһ����RGBֵ]
//�ر�ע�⵱����alphaΪ0ʱ����Ҫд��RGB���ݣ���alpha_count���Ϊ256,д������Ϊ0x00
//���沽���ظ�������ȫ���ж���ϡ�
//offset_table���������ӣ�ÿԪ����ֵ���ڵ�ǰ�����ѹ��ǰ��size��

void ReadPng(FILE* pngfile,const char*lpFileName)
{
	unit64 pixelR, pixelG, pixelB, pixelA;
	png_bytep row_buf;	//��ŵ�ǰ�еĻ�����
	unit8* pSrc;		//����ָ��row_buf��ָ��
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
	
	//������ļ�
	std::ofstream out;
	out.open(lpFileName, std::ios::binary);

	kgheader k;
	//memcpy(k.magic, magic, 4);
	k.width = info_ptr->width;
	k.height = info_ptr->height;
	//unit32 size = 0;//��С����

	//���λ�õ���bitmap_data��
	char cc[1] = { 0xcc };
	for (int i = 0; i < sizeof(k) + 4 * k.height; i++) {
		out.write(cc,1);
	}
	//out.flush();
	//ÿһ�еģ�pSrc
	int p = 0;
	//��һ��offset���
	int pprev =0;

	//���ÿ��������ʼ��ַ��table
	unit32* offset_table = new unit32[info_ptr->height];

	//Ϊÿһ�е����ط���ռ�
	row_buf = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));
	for (int i = 0; i < info_ptr->height; i++) {
		p = 0;
		
		png_read_rows(png_ptr, (png_bytepp)&row_buf, NULL, 1);
		//printf("Rows:%d:\n", i);
		//��¼��ǰ���ڵ�����λ��
		int pixpos = 1;
		//��������ڵ�table
		offset_table[i] = pprev;
	seg1:
		//alpha������1��ʼ
		int count = 1;
		//��ȡ��һ������alpha
		int alpha = row_buf[p+3];
		p += 4;

		//row_buf:[RR][GG][BB][AA]|[RR][GG][BB][AA]|[RR][GG][BB][AA]
		//		   ��	        	|______ |______
		//        pSrc					  | 	  |
		//compressed:[AA][CC][RR][GG][BB][RR][GG][BB]

		//�����һ������alpha��֮ǰ����ͬ�ͼ�����ֱ������ͬ
		//ͬʱҲ�Ǽ�����ѹ���󳤶�
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
			
			//���ͬһ�����ع��࣬����256��
			if (count == 256) {
				break;
				//break֮�������������½����жϣ�����ǰ�������Ƿ�������ͬ
			}
		}
		//printf("Alpha:%02X,Count:%04d,Pixpos:%04d\n", alpha,count,pixpos);
		//��͸���Ȳ�Ϊ0ʱ������ɫʱ��
		if (alpha != 0) {
			pprev += 2 + count * 3;
			//Ԥ�����С
			unit8* compressed = new unit8[2 + count * 3];
			//�������
			compressed[0] = alpha;
			//����256��������0����
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
				//        ��  ��  ��              ��        ��   ��
				//alpha+count rgb  offset      offset     index  rgb
				o += 4;
				px += 3;
			}
			//д��
			out.write((const char*)compressed, count * 3 + 2);
			//out.flush();
			//�ͷ�ѹ�����ݿռ�
			delete[] compressed;
		}
		//û��ɫ��ֱ��дalpha��count��
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
		//�����һ�л�û����ͼ�����
		if (pixpos != info_ptr->width)
			goto seg1;

	}

	//д���ļ�ͷ�Լ�offset_table
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
	//FILE* fp = fopen("E:\\GalGames_Work\\OnWork\\����԰���������\\chip_unpack\\testbench\\CFGChip.png","rb");
	//ReadPng(fp, (char*)"E:\\GalGames_Work\\OnWork\\����԰���������\\chip_unpack\\testbench\\0001.kg");
}