/*
   Based heavily on miniunz.c - ( http://www.winimage.com/zLibDll/minizip.html )
*/

#ifndef _WIN32
        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifdef unix
# include <unistd.h>
# include <utime.h>
#else
# include <direct.h>
# include <io.h>
#endif

#include "unzip.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

unsigned char * LoadFirstFileInZip(const char* path,unsigned int *length)
{
    	unzFile uf=NULL;
	void* buf=NULL;

#        ifdef USEWIN32IOAPI
        zlib_filefunc64_def ffunc;
#        endif

#        ifdef USEWIN32IOAPI
        fill_win32_filefunc64A(&ffunc);
        uf = unzOpen2_64(path,&ffunc);
#        else
        uf = unzOpen64(path);
#        endif

	if (uf==NULL)
	{
		printf("Cannot open %s as zip\n",path);
		return 0;
	}

	// Was do Extract- -
	{
		uLong i;
		unz_global_info64 gi;
		int err;

		err = unzGetGlobalInfo64(uf,&gi);
		if (err!=UNZ_OK || gi.number_entry<1)
		{
			printf("error %d with zipfile in unzGetGlobalInfo \n",err);
			return 0;
		}

		{
			char filename_inzip[256];
			char* filename_withoutpath;
			char* p;
			int err=UNZ_OK;
			FILE *fout=NULL;
			uInt size_buf;

			unz_file_info64 file_info;
			uLong ratio=0;
			err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

			if (err!=UNZ_OK)
			{
				printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
				return 0;
			}

			size_buf=file_info.uncompressed_size;
			buf = malloc(file_info.uncompressed_size);
			if (buf==NULL)
			{
				printf("Error allocating memory\n");
				return 0;
			}

			err = unzOpenCurrentFilePassword(uf,NULL);
			if (err!=UNZ_OK)
			{
				printf("error %d with zipfile in unzOpenCurrentFilePassword\n",err);
				return 0;
			}

			do
			{
				err = unzReadCurrentFile(uf,buf,size_buf);
				if (err<0)
				{
					printf("error %d with zipfile in unzReadCurrentFile\n",err);
					return 0;
				}
			}
			while (err>0);

			if (err==UNZ_OK)
			{
				err = unzCloseCurrentFile (uf);
				if (err!=UNZ_OK)
				{
					printf("error %d with zipfile in unzCloseCurrentFile\n",err);
					return 0;
				}
			}
    			*length=size_buf;
		}
	}

    unzClose(uf);

    return buf;
}

