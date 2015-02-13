#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "aacedit.h"
#define TRUE 1
#define FALSE 0
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define _MAX_PATH 260
typedef int HANDLE;
typedef unsigned long DWORD;

//#define CORRECTION 108 / 100
#define CORRECTION 1

static int getoptions(int argc, char *argv[]);
static int avsopen(const char *filepath);
static void usage(void);

int aacopen(const char *filepath, AACDATA *aacdata);
int trimanalyze(void);
int aacwrite(void);
void aacrelease(void);
unsigned long getallaacframe(void);
long videotoaacframe(long vframe);
void errorexit(const char *errorstr, int showusage);
unsigned long bitstoint(unsigned char *data, unsigned int shift, unsigned int n);

OPTIONS options;
HANDLE hWriteAACFile, hAvsFile;
AACDATA *aacdatalist[MAX_INPUT_FILE];
EDITINFO *editinfotop;

int main(int argc, char *argv[])
{
	AACDATA *aacdata;
	AACHEADER *aacheader, *aacheaderprev;
	unsigned int bitrate, second;
	int i, editok;

	printf("AAC ADTS Editor version 0.1 alpha 3 add 3\n");
	if (!getoptions(argc, argv))
		errorexit("�I�v�V����������������܂���B", TRUE);

	if (options.outputpath) {
		avsopen(options.avspath);
		hWriteAACFile = open(options.outputpath, (O_RDWR | O_CREAT), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
		if (hWriteAACFile < 0) {
			errorexit("�o�̓t�@�C���I�[�v���Ɏ��s���܂����B", FALSE);
			return 0;
		}
	}
	editok = TRUE;
	for (i = 0; i < options.inputfile; i++) {
		aacdata = aacdatalist[i] = (AACDATA *)calloc(1, sizeof(AACDATA));
		if (aacdata == NULL)
			errorexit("�������m�ۂɎ��s���܂����B", FALSE);
		if (aacopen(options.inputpath[i], aacdata) == 2) {
			printf("%s �̓t���[���h���b�v���Ă���\��������܂��B\n", options.inputpath[i]);
			editok = FALSE;
		}
		aacheader = aacdata->header;
		printf("%s �̏��:\n", options.inputpath[i]);
		printf("  �T�C�Y:       %ld �o�C�g\n", aacdata->size);
		printf("  �t���[��:     %u �t���[��\n", aacdata->framecnt);
		second = (unsigned int)((double)aacdata->framecnt / ((double)aacheader->sampling_rate / 1024));
		printf("  ����:         %02u:%02u:%02u\n", second / 3600, second / 60 % 60, second % 60);
		bitrate = (unsigned int)((double)aacdata->size * 8 * ((double)aacheader->sampling_rate / 1024) / aacdata->framecnt + 0.5);
		printf("  �r�b�g���[�g: %u.%u kbps\n", bitrate / 1000, bitrate % 1000);
		while (aacheader) {
			if (aacheader->frame == 0) {
				printf("  �o�[�W����:   %s\n", aacheader->version ? "MPEG-2 AAC" : "MPEG-4 AAC");
				if (aacheader->profile == 0) {
					printf("  �v���t�@�C��: Main\n");
					editok = FALSE;
				} else if (aacheader->profile == 1) {
					printf("  �v���t�@�C��: LC (Low Complexity)\n");
				} else if (aacheader->profile == 2) {
					printf("  �v���t�@�C��: SSR (Scalable Sampling Rate)\n");
					editok = FALSE;
				} else {
					printf("  �v���t�@�C��: �s��\n");
					editok = FALSE;
				}
				printf("  �T���v�����O���g��: %u Hz\n", aacheader->sampling_rate);
				if (aacheader->channel == 0)
					printf("  �`�����l��:   0 ch (��d����?)\n");
				else if (aacheader->channel >= 6)
					printf("  �`�����l��:   %u.1 ch\n", aacheader->channel - 1);
				else
					printf("  �`�����l��:   %u ch\n", aacheader->channel);
			} else {
				printf("%u�t���[���ȍ~:\n", aacheader->frame);
				if (aacheader->version != aacheaderprev->version)
					printf("  �o�[�W����:   %s\n", aacheader->version ? "MPEG-2 AAC" : "MPEG-4 AAC");
				if (aacheader->profile != aacheaderprev->profile) {
					if (aacheader->profile == 0) {
						printf("  �v���t�@�C��: Main\n");
						editok = FALSE;
					} else if (aacheader->profile == 1) {
						printf("  �v���t�@�C��: LC (Low Complexity)\n");
					} else if (aacheader->profile == 2) {
						printf("  �v���t�@�C��: SSR (Scalable Sampling Rate)\n");
						editok = FALSE;
					} else {
						printf("  �v���t�@�C��: �s��\n");
						editok = FALSE;
					}
				}
				if (aacheader->sampling_rate != aacheaderprev->sampling_rate)
					printf("  �T���v�����O���g��: %u Hz\n", aacheader->sampling_rate);
				if (aacheader->channel != aacheaderprev->channel) {
					if (aacheader->channel == 0)
						printf("  �`�����l��:   0 ch (��d����?)\n");
					else if (aacheader->channel >= 6)
						printf("  �`�����l��:   %u.1 ch\n", aacheader->channel - 1);
					else
						printf("  �`�����l��:   %u ch\n", aacheader->channel);
				}
			}
			if (aacheader->sampling_rate != 48000)
				editok = FALSE;
			aacheaderprev = aacheader;
			aacheader = aacheader->next;
		}
		putchar('\n');
	}

	if (!editok) {
		printf("�ҏW��Ή��t�@�C���ł��B\n");
	} else if (options.outputpath) {
		if (!trimanalyze()) {
			int delay, allaacframe;

			editinfotop = (EDITINFO *)calloc(1, sizeof(EDITINFO));
			if (editinfotop == NULL)
				errorexit("�������m�ۂɎ��s���܂����B", FALSE);
			//�����ӂ�`�F�b�N
			allaacframe = getallaacframe();
			delay = options.delay * 46875 / 1000000 * CORRECTION;
			if (delay > 0) {
				editinfotop->startframe = 0;
				editinfotop->endframe = max(allaacframe - delay, 0);
			} else {
				editinfotop->startframe = -delay;
				editinfotop->endframe = allaacframe;
			}
			if (editinfotop->startframe == editinfotop->endframe)
				editinfotop->startframe = editinfotop->endframe = 0xFFFFFFFF;
		}
		aacwrite();
	}
	aacrelease();
	return 0;
}

static int getoptions(int argc, char *argv[])
{
	int i, ret = 0;
	char *p, buf[256];

	memset(options.inputpath, 0, sizeof(options.inputpath));
	options.inputfile = 0;
	options.outputpath = NULL;
	options.avspath = NULL;
	options.videoframerate = 29970;
	options.aacframeset = FALSE;
	options.editmode = FALSE;
	options.delay = 0;
	options.auto_cut = 0 ;
	if (argc == 1) {
		usage();
		exit(0);
	}
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (tolower(argv[i][1])) {
			case 't':
				i++;
				if (i >= argc)
					return 0;
				if (options.avspath == NULL)
					options.avspath = argv[i];
				else
					return 0;
				break;
			case 'o':
				i++;
				if (i >= argc)
					return 0;
				if (options.outputpath == NULL)
					options.outputpath = argv[i];
				else
					return 0;
				break;
			case 'd':
				i++;
				if (i >= argc)
					return 0;
				p = argv[i];
				if (*p == '+' || *p == '-')
					p++;
				while (isdigit(*p) && p - argv[i] < 7) p++;
				if (*p != '\0')
					return 0;
				options.delay = atoi(argv[i]);
				break;
			case 'a':
				options.aacframeset = TRUE;
				break;
			case 'f':
				i++;
				if (i >= argc)
					return 0;
				//�����h�C�̂ŌŒ�`�F�b�N
				if (strncmp(argv[i], "29.97", 5) == 0)
					options.videoframerate = 29970;
				else if (strncmp(argv[i], "59.94", 5) == 0)
					options.videoframerate = 59940;
				else
					return 0;
				break;
			case '?':
			case 'h':
				usage();
				exit(0);
			case 'x':
				options.auto_cut = 1 ;
				break ;
			default:
				return 0;
			}
		}
		else {
			if (options.inputfile < MAX_INPUT_FILE) {
				options.inputpath[options.inputfile] = argv[i];
				options.inputfile++;
				ret = 1;
			} else {
				sprintf(buf, "���̓t�@�C���͍ő� %d �t�@�C���܂łł��B", MAX_INPUT_FILE);
				errorexit(buf, TRUE);
			}
		}
	}
	if ( options.auto_cut && options.inputfile ) {
		const char* pszFirstInput = options.inputpath[0] ;
		int nPosBegin = -1, nPosEnd, nDelay ;
		int nPos, nPosMinus,nMinus, nValue, nState = 0 ;
		static char szOut[_MAX_PATH] ;

		for ( nPos = 0 ; nPos < strlen( pszFirstInput ) ; nPos++ ) {
			char c = tolower( pszFirstInput[nPos] ) ;
			switch ( nState ) {
			case 0 :
				if ( c == '-' ){
					nMinus = 1;
					nValue = 0 ;
					nPosMinus = nPos ;
				}
				if(( c >= '0' ) & ( c <= '9' )){
					nMinus = 0;
					nValue = c - '0';
					nPosMinus = nPos ;
				}
				nState = 1 ;
				break ;
			case 1 :
				if ( c == 'm' ) nState = 2 ;
				else if (( c >= '0' ) & ( c <= '9' )) nValue = nValue * 10 + c - '0' ;
				else nState = 0 ;
				break ;
			case 2 :
				if ( c == 's' ) {
					nPosBegin = nPosMinus ;
					nPosEnd = nPos ;
					nDelay = nValue ;
					if(nMinus)
						nDelay = nDelay * -1;
				}
				nState = 0 ;
				break ;
			}
		}

		if ( nDelay == 0 ) {
			errorexit("���̓t�@�C���㏑���h�~�̈ג��~���܂�\n", FALSE);
		}
		if ( nPosBegin < 0 ) {
			errorexit("-x�I�v�V�������w�肳��܂������A�t�@�C������\"-**ms\"��������܂���B\n", FALSE);
		}
		strcpy( szOut, pszFirstInput ) ;
		szOut[nPosBegin] = '\0' ;
		strcat( szOut, "0ms" ) ;
		strcat( szOut, &pszFirstInput[nPosEnd+1] ) ;
		options.outputpath = szOut ;
		options.delay = nDelay ;
		printf( "-x�I�v�V�����ɂ��A�ȉ��̃I�v�V�����������I�ɒǉ�\n" ) ;
		printf( "    -d %d\n", nDelay ) ;
		printf( "    -o \"%s\"\n", szOut ) ;
	}
	return ret;
}

static void usage(void)
{
	printf(
	"Usage: aacedit [options] -t xxxx -o <outfile> <infiles>\n"
	"\n"
	"�I�v�V����:\n"
	"  -t xxxx      AviSynth �X�N���v�g�t�@�C������\n"
	"               Trim �Ŏn�܂��s���������ēǂݍ���\n"
	"               �܂��̓R�}���h���C������ Trim �������\n"
	"  -o <outfile> �o�̓t�@�C��\n"
	"  -d xxxx      �x���␳ (ms�P��)\n"
	"  -a           AAC �t���[���𒼐ڎw��\n"
	"  -f xxxx      ����̃t���[�����[�g���w�� (29.97 or 59.94)\n"
	"  -x           �ŏ���<infiles>�̃t�@�C��������-o/-d�I�v�V�����������w��\n"
	"  -h or -?     �w���v\n"
	);
	return;
}

static int avsopen(const char *filepath)
{
	size_t len;
	const char *p, *plast;
	char tmpstr[16], *ptmp;

	if (!filepath)
		return 0;
	len = strlen(filepath);
	if (len == 0)
		return 0;
	p = filepath;
	plast = p + min(len, 5);
	ptmp = tmpstr;
	while (p < plast) {
		*ptmp = tolower(*p);
		p++;
		ptmp++;
	}
	*ptmp = '\0';
	if (strcmp(tmpstr, "trim(") != 0) {
		hAvsFile = open(filepath, O_RDONLY);
		if (hAvsFile < 0) {
			errorexit("AviSynth �X�N���v�g�t�@�C���̓ǂݍ��݂Ɏ��s���܂����B", FALSE);
		}
	}
	return 1;
}

int aacopen(const char *filepath, AACDATA *aacdata)
{
	int ret = 1;
	unsigned int aacframecnt = 0, indexchunk = INDEX_CHUNK_SIZE, readbyte = 0;
	unsigned int channel = 0xFF, tmpchannel, aac_frame_length;
	unsigned char buffer[BUFFER_SIZE], *p, *plast, *ptmp;
	AACHEADER *aacheader = NULL, *aacheadertop, *tmpaacheader;
	DWORD filesize, readchunk = BUFFER_SIZE;
	HANDLE hFile;
	const int sampling_frequency_index[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};

	if (!aacdata || !filepath)
		return 0;
	hFile = open(filepath, O_RDONLY);
	if (hFile < 0) {
		errorexit("AAC �t�@�C���̓ǂݍ��݂Ɏ��s���܂����B", FALSE);
	}

	lseek(hFile, 0, SEEK_END);
	filesize = lseek(hFile, 0, SEEK_CUR);
	lseek(hFile, 0, SEEK_SET);
	aacdata->data = (unsigned char *)calloc(filesize, sizeof(char));
	aacdata->index = (unsigned int *)calloc(indexchunk, sizeof(int));
	if (aacdata->data == NULL || aacdata->index == NULL)
		errorexit("�������m�ۂɎ��s���܂����B", FALSE);
	readchunk = read(hFile, buffer, readchunk);
	while (readchunk > 0) {
		p = buffer;
		plast = p + readchunk - 7;
		while (p < plast) {
			if (*p == 0xFF && (*(p+1) & 0xF6) == 0xF0) {
				aacdata->index[aacframecnt] = readbyte;
				aac_frame_length = bitstoint(p + 3, 6, 13);
				if (readbyte + aac_frame_length > filesize) { //�I�[��length��菭�Ȃ����Ƃ�����
					aac_frame_length = filesize - readbyte;
				}
				tmpchannel = bitstoint(p, 23, 3);
				if (channel != tmpchannel) {
					tmpaacheader = (AACHEADER *)calloc(1, sizeof(AACHEADER));
					if (tmpaacheader == NULL)
						errorexit("�������m�ۂɎ��s���܂����B", FALSE);
					tmpaacheader->frame = aacframecnt;
					tmpaacheader->version = bitstoint(p, 12, 1);
					tmpaacheader->profile = bitstoint(p, 16, 2);
					tmpaacheader->sampling_rate = sampling_frequency_index[bitstoint(p, 18, 4)];
					tmpaacheader->channel = channel = tmpchannel;
					/*
					if (channel == 0) {
					}
					*/
					tmpaacheader->next = NULL;
					if (!aacheader) {
						aacheadertop = aacheader = tmpaacheader;
					} else {
						aacheader->next = tmpaacheader;
						aacheader = tmpaacheader;
					}
				}
				memcpy(aacdata->data + readbyte, p, aac_frame_length);
				aacframecnt++;
				if (aacframecnt >= indexchunk) {
					indexchunk += INDEX_CHUNK_SIZE;
					aacdata->index = (unsigned int *)realloc(aacdata->index, indexchunk * sizeof(unsigned int));
					if (aacdata->index == NULL) {
						errorexit("�������m�ۂɎ��s���܂����B", FALSE);
					}
				}
				readbyte += aac_frame_length;
				if (filesize <= readbyte)
					break;
				else if ((p + aac_frame_length > plast - FRAME_BYTE_LIMIT) && readchunk >= BUFFER_SIZE) {
					lseek(hFile, readbyte, 0);
					break;
				}
				p += aac_frame_length; //�t���[�����ړ�
			} else {
				if (aacframecnt != 0)
					printf("%u �t���[���ڂ̃w�b�_�[��񂪐���������܂���B\n", aacframecnt);
				ptmp = min(p + 1024, plast);
				while (p < ptmp && !(*p == 0xFF && (*(p+1) & 0xF6) == 0xF0)) p++;
				if (p == ptmp) {
					close(hFile);
					errorexit("AAC ADTS �t�@�C���ł͂���܂���B", FALSE);
				}
				ret = 2;
			}
		}
		readchunk = read(hFile, buffer, readchunk);
	}
	close(hFile);
	if (aacframecnt == 0) {
		errorexit("AAC ADTS �t�@�C���ł͂���܂���B", FALSE);
	}
	aacdata->index[aacframecnt] = filesize;
	aacdata->size = filesize;
	aacdata->framecnt = aacframecnt;
	aacdata->header = aacheadertop;
	return ret;
}

int trimanalyze(void)
{
	size_t len;
	int startnum, endnum, endnumzero, delay, allaacframe;
	char *allocbuf, *p, *ptmp, *plast, buffer[BUFFER_SIZE];
	EDITINFO *editinfo, *editip;
	DWORD filesize, readchunk;

	if (!options.avspath)
		return 0;
	if (!hAvsFile) {
		len = strlen(options.avspath);
		p = allocbuf = (char *)calloc(len + 1, sizeof(char));
		if (allocbuf == NULL)
			errorexit("�������m�ۂɎ��s���܂����B", FALSE);
		plast = options.avspath + len;
		for (ptmp = options.avspath; ptmp < plast; ptmp++) {
			if (*ptmp != ' ' && *ptmp != '\t') {
				*p = tolower(*ptmp);
				p++;
			}
		}
		*p = '\0';
		plast = p;
	} else {
		lseek(hAvsFile, 0, SEEK_END);
		filesize = lseek(hAvsFile, 0, SEEK_CUR);
		lseek(hAvsFile, 0, SEEK_SET);
		p = allocbuf = (char *)calloc(filesize + 1, sizeof(unsigned char));
		if (allocbuf == NULL)
			errorexit("�������m�ۂɎ��s���܂����B", FALSE);
		readchunk = read(hAvsFile, buffer, BUFFER_SIZE);
		while (readchunk > 0) {
			plast = buffer + readchunk;
			for (ptmp = buffer; ptmp < plast; ptmp++) {
				if (*ptmp == '\r' && *(ptmp + 1) == '\n') {
					*p = '\n';
					p++;
					ptmp++;
				} else if (*ptmp != ' ' && *ptmp != '\r' && *ptmp != '\t') {
					*p = tolower(*ptmp);
					p++;
				}
			}
			readchunk = read(hAvsFile, buffer, BUFFER_SIZE);
		}
		close(hAvsFile);
		hAvsFile = -1;
		plast = p;
	}
	p = allocbuf;
	allaacframe = getallaacframe();
	while (p < plast) {
		if (strncmp(p, "trim(", 5) == 0) {
			p += 5;
			ptmp = p;
			while (p < allocbuf + len && isdigit(*p)) p++;
			if (*p != ',' || p == ptmp || p - ptmp >= 8)
				break;
			strncpy(buffer, ptmp, p - ptmp);
			*(buffer + (p - ptmp)) = '\0';
			startnum = atoi(buffer);
			p++;
			ptmp = p;
			while (p < plast && (isdigit(*p) || *p == '-')) p++;
			if (p == ptmp || p - ptmp >= 8)
				break;
			strncpy(buffer, ptmp, p - ptmp);
			*(buffer + (p - ptmp)) = '\0';
			endnum = atoi(buffer);

			editip = (EDITINFO *)calloc(1, sizeof(EDITINFO));
			if (editip == NULL)
				errorexit("�������m�ۂɎ��s���܂����B", FALSE);
			endnumzero = (endnum == 0);
			if (!endnumzero) {
				if (endnum < 0)
					endnum = startnum - endnum;
				else
					endnum++;
				if (startnum >= endnum)
					endnum = startnum + 1;
			}
			startnum = videotoaacframe(startnum);
			endnum = videotoaacframe(endnum);
			delay = options.delay * 46875 / 1000000 * CORRECTION;
			startnum -= delay;
			//�����ӂ�`�F�b�N
			if (startnum >= allaacframe)
				editip->startframe = allaacframe - 1;
			else if (startnum < 0)
				editip->startframe = startnum = 0;
			else
				editip->startframe = startnum;
			if (endnumzero) {
				editip->endframe = allaacframe;
			} else {
				endnum -= delay;
				if (endnum > allaacframe)
					editip->endframe = allaacframe;
				else if (endnum > startnum)
					editip->endframe = endnum;
			 	else
					editip->startframe = editip->endframe = 0xFFFFFFFF; //��O
			}
			editip->next = NULL;
			if (!editinfotop) {
				editinfotop = editinfo = editip;
			} else {
				editinfo->next = editip;
				editinfo = editip;
			}
			while (p < plast && *p != ')') p++;
		} else if (*p == '\n' && *(p + 1) == '\\') { //������Trim������
			p++;
		} else if (*p != '+') {
			if (editinfotop)
				break;
			while (p < plast && *p != '\n') p++;
		}
		p++;
	}
	free(allocbuf);
	if (editinfotop == NULL)
		return 0;
	return 1;
}

int aacwrite(void)
{
	int i;
	unsigned int writebyte;
	unsigned char *writebuf;
	DWORD writechunk, framenum;
	AACDATA *aacdata;
	EDITINFO *editip;

	if (!editinfotop)
		return 0;

	printf("�o�͒�...\n");
	framenum = writebyte = 0;
	editip = editinfotop;
	while (editip) {
		unsigned int writeframe = 0, skip = 1, startframe, endframe;

		if (editip->startframe == 0xFFFFFFFF) {
			printf("(no output)");
			editip = editip->next;
			if (editip != NULL)
				putchar('+');
			continue;
		}
		printf("Trim(%u,%u)", editip->startframe, editip->endframe - 1);
		for (i = 0; i < options.inputfile; i++) {
			aacdata = aacdatalist[i];
			if (skip > 0) {
				if (aacdata->framecnt * skip <= editip->startframe) {
					skip++;
					continue;
				} else {
					startframe = editip->startframe - (aacdata->framecnt * (skip - 1));
					skip = 0;
				}
			}
			if (i == 0)
				endframe = min(editip->endframe, aacdata->framecnt);
			else
				endframe = min(editip->endframe - editip->startframe - writeframe, aacdata->framecnt);
			if (writeframe == 0) {
				writebuf = aacdata->data + aacdata->index[startframe];
				writechunk = aacdata->index[endframe] - aacdata->index[startframe];
				writeframe += endframe - startframe;
			} else {
				writebuf = aacdata->data;
				writechunk = aacdata->index[endframe];
				writeframe += endframe;
			}
			write(hWriteAACFile, writebuf, writechunk);
			writebyte += writechunk;
			if (writeframe >= editip->endframe - editip->startframe)
				break;
		}
		framenum += writeframe;
		editip = editip->next;
		if (editip != NULL)
			putchar('+');
	}
	close(hWriteAACFile);
	hWriteAACFile = -1;
	printf("\n�o�̓T�C�Y: %d �o�C�g (%ld�t���[��)\n", writebyte, framenum);
	return 1;
}

void aacrelease(void)
{
	int i;
	AACDATA *aacdata;
	AACHEADER *aacheader, *aacheadernext;
	EDITINFO *editinfo, *editinfonext;

	for (i = 0; i < options.inputfile; i++) {
		aacdata = aacdatalist[i];
		if (!aacdata)
			break;
		free(aacdata->data);
		free(aacdata->index);
		aacheader = aacdata->header;
		while (aacheader) {
			aacheadernext = aacheader->next;
			free(aacheader);
			aacheader = aacheadernext;
		}
		memset(aacdata, 0, sizeof(AACDATA));
	}
	editinfo = editinfotop;
	while (editinfo) {
		editinfonext = editinfo->next;
		free(editinfo);
		editinfo = editinfonext;
	}
	return;
}

unsigned long getallaacframe(void)
{
	int i;
	unsigned long ret = 0;
	AACDATA *aacdata;

	for (i = 0; i < options.inputfile; i++) {
		aacdata = aacdatalist[i];
		ret += aacdata->framecnt;
	}
	return ret;
}

//�r�f�I�t���[������AAC�t���[�����v�Z
long videotoaacframe(long vframe)
{
	double ret, m;
	long a;

	if (options.aacframeset || vframe == 0)
		return vframe;

	//(48000 / 1024) / (30000 / 1001) == 1.5640625
	//(48000 / 1024) / (60000 / 1001) == 0.78203125
	if (options.videoframerate == 59940) {
		m = 0.78203125;
		a = 282;
	} else {
		m = 1.5640625;
		a = 564;
	}
	ret = (double)vframe * m;
	a = (options.delay % 125) * a;
	a = (a + (a / 2000)) % 1000;
	ret -= (double)a / 1000;
	return (long)ret;
}

void errorexit(const char *errorstr, int showusage)
{
	fprintf(stderr, "�G���[: %s\n", errorstr);
	if (showusage) usage();
	aacrelease();
	if (hAvsFile) close(hAvsFile);
	if (hWriteAACFile) close(hWriteAACFile);
	exit(1);
}

//�r�b�g�ϊ��֐�
unsigned long bitstoint(unsigned char *data, unsigned int shift, unsigned int n)
{
	unsigned long ret;

	memcpy(&ret, data, sizeof(unsigned long));
	ret = ret << 24 | ((ret << 8) & 0x00FF0000) | ((ret >> 8) & 0x0000FF00) | ((ret >> 24) & 0x000000FF);
	ret = (ret >> (32 - shift - n));
	ret &= 0xFFFFFFFF & ((1 << n) - 1);
	return ret;
}
