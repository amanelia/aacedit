#define FRAME_BYTE_LIMIT	4096
#define MAX_INPUT_FILE		16
#define BUFFER_SIZE		32768
#define INDEX_CHUNK_SIZE	4096

typedef struct _aacdata {
	unsigned char *data;
	size_t size;
	unsigned int *index;
	unsigned int framecnt;
	struct _aacheader *header;
} AACDATA;

typedef struct _aacheader {
	unsigned int frame;
	unsigned int version;
	unsigned int profile;
	unsigned int sampling_rate;
	unsigned int channel;
	struct _aacheader *next;
} AACHEADER;

typedef struct _options {
	char *inputpath[MAX_INPUT_FILE];
	int inputfile;
	char *outputpath;
	char *avspath;
	int videoframerate;
	int aacframeset;
	int editmode;
	int delay;
	int auto_cut ;
} OPTIONS;

typedef struct _editinfo {
	unsigned int startframe;
	unsigned int endframe;
	struct _editinfo *next;
} EDITINFO;
