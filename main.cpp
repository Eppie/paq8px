#ifndef DEFAULT_OPTION
#define DEFAULT_OPTION 5
#endif
#define PROGNAME "paq8px"  // Please change this if you change the program.
//#define NDEBUG

#ifndef NDEBUG
#include <iostream>
#define DEBUG_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define DEBUG_MSG(str) do { } while ( false )
#endif

// 8, 16, 32 bit unsigned types (adjust as appropriate)
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

typedef enum {
    DEFAULT, JPEG, HDR, IMAGE1, IMAGE8, IMAGE8GRAY, IMAGE24, IMAGE32, AUDIO, EXE, CD, ZLIB, BASE64, GIF
} Filetype;
int level = DEFAULT_OPTION;  // Compression level 0 to 8
#define MEM (0x10000<<level)
int y = 0;  // Last bit, 0 or 1, set by encoder

// Global context set by Predictor and available to all models.

int c0 = 1; // Last 0-7 bits of the partial byte with a leading 1 bit (1-255)
U32 c4 = 0, f4 = 0, b2 = 0; // Last 4 whole bytes, packed.  Last byte is bits 0-7.
int bpos = 0; // bits in c0 (0 to 7)
int blpos = 0; // Relative position in block
static int dt[1024];  // i -> 16K/(i+3)

#include <cstdio>
#include <cassert>
#include <cstring>
#include "Buf.h"

Buf buf;  // Rotating input queue set by Predictor
#include "String.h"
#include "Encoder.h"
#include "user_interface.h"

#include "filters.h"

void direct_encode_block(Filetype type, FILE *in, int len, Encoder &en, int info = -1) {
    en.compress(type);
    en.compress(len >> 24);
    en.compress(len >> 16);
    en.compress(len >> 8);
    en.compress(len);
    if(info != -1) {
        en.compress(info >> 24);
        en.compress(info >> 16);
        en.compress(info >> 8);
        en.compress(info);
    }
    printf("Compressing... ");
    for(int j = 0; j < len; ++j) {
        if(!(j & 0xfff)) { en.print_status(j, len); }
        en.compress(getc(in));
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

void compressRecursive(FILE *in, long n, Encoder &en, char *blstr, int it = 0, float p1 = 0.0, float p2 = 1.0);

void
transform_encode_block(Filetype type, FILE *in, int len, Encoder &en, int info, char *blstr, int it, float p1, float p2,
                       long begin) {
    if(type == EXE || type == CD || type == IMAGE24 || type == ZLIB || type == BASE64 || type == GIF) {
        FILE *tmp = tmpfile();  // temporary encoded file
        if(!tmp) { perror("tmpfile"), quit(); }
        int diffFound = 0;
        if(type == IMAGE24) { encode_bmp(in, tmp, len, info); }
        else if(type == EXE) { encode_exe(in, tmp, len, begin); }
        else if(type == CD) { encode_cd(in, tmp, len, info); }
        else if(type == ZLIB) { diffFound = encode_zlib(in, tmp, len) ? 0 : 1; }
        else if(type == BASE64) { encode_base64(in, tmp, len); }
        else if(type == GIF) { diffFound = encode_gif(in, tmp, len) ? 0 : 1; }
        const long tmpsize = ftell(tmp);
        fseek(tmp, tmpsize, SEEK_SET);
        if(!diffFound) {
            rewind(tmp);
            en.setFile(tmp);
            fseek(in, begin, SEEK_SET);
            if(type == IMAGE24) { decode_bmp(en, tmpsize, info, in, FCOMPARE, diffFound); }
            else if(type == EXE) { decode_exe(en, tmpsize, in, FCOMPARE, diffFound); }
            else if(type == CD) { decode_cd(tmp, tmpsize, in, FCOMPARE, diffFound); }
            else if(type == ZLIB) { decode_zlib(tmp, tmpsize, in, FCOMPARE, diffFound); }
            else if(type == BASE64) { decode_base64(tmp, in, FCOMPARE, diffFound); }
            else if(type == GIF) { decode_gif(tmp, tmpsize, in, FCOMPARE, diffFound); }
        }
        // Test fails, compress without transform
        if(diffFound || fgetc(tmp) != EOF) {
            printf("Transform fails at %d, skipping...\n", diffFound - 1);
            fseek(in, begin, SEEK_SET);
            direct_encode_block(DEFAULT, in, len, en);
        } else {
            rewind(tmp);
            if(type == CD || type == ZLIB || type == BASE64 || type == GIF) {
                en.compress(type), en.compress(tmpsize >> 24), en.compress(tmpsize >> 16);
                en.compress(tmpsize >> 8), en.compress(tmpsize);
                if(type == ZLIB && info > 0) {// PDF image
                    Filetype type2 = (info >> 24) == 24 ? IMAGE24 : IMAGE8;
                    int hdrsize = 7 + 5 * fgetc(tmp);
                    rewind(tmp);
                    direct_encode_block(HDR, tmp, hdrsize, en);
                    transform_encode_block(type2, tmp, tmpsize - hdrsize, en, info & 0xffffff, blstr, it, p1, p2,
                                           hdrsize);
                } else if(type == GIF) {
                    int hdrsize = fgetc(tmp);
                    hdrsize = (hdrsize << 8) + fgetc(tmp);
                    rewind(tmp);
                    direct_encode_block(HDR, tmp, hdrsize, en);
                    direct_encode_block(IMAGE8, tmp, tmpsize - hdrsize, en, info);
                } else {
                    compressRecursive(tmp, tmpsize, en, blstr, it + 1, p1, p2);
                }
            } else if(type == EXE) {
                direct_encode_block(type, tmp, tmpsize, en);
            } else if(type == IMAGE24) {
                direct_encode_block(type, tmp, tmpsize, en, info);
            }
        }
        fclose(tmp);  // deletes
    } else {
        const int i1 = (type == IMAGE1 || type == IMAGE8 || type == IMAGE8GRAY || type == IMAGE32 || type == AUDIO)
                       ? info : -1;
        direct_encode_block(type, in, len, en, i1);
    }
}

void compressRecursive(FILE *in, long n, Encoder &en, char *blstr, int it, float p1, float p2) {
    static const char *typenames[14] = {"default", "jpeg", "hdr",
                                        "1b-image", "8b-image", "8b-img-grayscale", "24b-image", "32b-image", "audio",
                                        "exe", "cd", "zlib", "base64", "gif"};
    static const char *audiotypes[4] = {"8b mono", "8b stereo", "16b mono",
                                        "16b stereo"};
    Filetype type = DEFAULT;
    int blnum = 0, info;  // image width or audio type
    long begin = ftell(in), end0 = begin + n;
    char b2[32];
    strcpy(b2, blstr);
    if(b2[0]) { strcat(b2, "-"); }
    if(it == 5) {
        direct_encode_block(DEFAULT, in, n, en);
        return;
    }
    float pscale = n > 0 ? (p2 - p1) / n : 0;

    // Transform and test in blocks
    while(n > 0) {
        Filetype nextType = detect(in, n, type, info);
        long end = ftell(in);
        fseek(in, begin, SEEK_SET);
        if(end > end0) {  // if some detection reports longer then actual size file is
            end = begin + 1;
            type = DEFAULT;
        }
        int len = int(end - begin);
        if(len > 0) {
            en.set_status_range(p1, p2 = p1 + pscale * len);
            sprintf(blstr, "%s%d", b2, blnum++);
            printf(" %-11s | %-16s |%10d bytes [%ld - %ld]", blstr, typenames[type], len, begin, end - 1);
            if(type == AUDIO) { printf(" (%s)", audiotypes[info % 4]); }
            else if(type == IMAGE1 || type == IMAGE8 || type == IMAGE8GRAY || type == IMAGE24 || type == IMAGE32) {
                printf(" (width: %d)", info);
            } else if(type == CD) { printf(" (m%d/f%d)", info == 1 ? 1 : 2, info != 3 ? 1 : 2); }
            else if(type == ZLIB && info > 0) { printf(" (image)"); }
            printf("\n");
            transform_encode_block(type, in, len, en, info, blstr, it, p1, p2, begin);
            p1 = p2;
            n -= len;
        }
        type = nextType;
        begin = end;
    }
}

// Compress a file. Split filesize bytes into blocks by type.
// For each block, output
// <type> <size> and call encode_X to convert to type X.
// Test transform and compress.
void compress(const char *filename, long filesize, Encoder &en) {
    assert(en.getMode() == COMPRESS);
    assert(filename && filename[0]);
    FILE *in = fopen(filename, "rb");
    if(!in) { perror(filename), quit(); }
    long start = en.size();
    printf("Block segmentation:\n");
    char blstr[32] = "";
    compressRecursive(in, filesize, en, blstr);
    if(in) { fclose(in); }
    printf("Compressed from %ld to %ld bytes.\n", filesize, en.size() - start);
}

int decompressRecursive(FILE *out, long n, Encoder &en, FMode mode, int it = 0) {
    Filetype type;
    long len, i = 0;
    int diffFound = 0, info;
    FILE *tmp;
    while(i < n) {
        type = (Filetype) en.decompress();
        len = en.decompress() << 24;
        len |= en.decompress() << 16;
        len |= en.decompress() << 8;
        len |= en.decompress();

        if(type == IMAGE1 || type == IMAGE8 || type == IMAGE8GRAY || type == IMAGE24 || type == IMAGE32 ||
           type == AUDIO) {
            info = 0;
            for(int i = 0; i < 4; ++i) {
                info <<= 8;
                info += en.decompress();
            }
        }
        if(type == IMAGE24) { len = decode_bmp(en, len, info, out, mode, diffFound); }
        else if(type == EXE) { len = decode_exe(en, len, out, mode, diffFound); }
        else if(type == CD || type == ZLIB || type == GIF || type == BASE64) {
            tmp = tmpfile();
            if(!tmp) { perror("tmpfile"), quit(); }
            decompressRecursive(tmp, len, en, FDECOMPRESS, it + 1);
            if(mode != FDISCARD) {
                rewind(tmp);
                if(type == CD) { len = decode_cd(tmp, len, out, mode, diffFound); }
                if(type == ZLIB) { len = decode_zlib(tmp, len, out, mode, diffFound); }
                if(type == BASE64) { len = decode_base64(tmp, out, mode, diffFound); }
                if(type == GIF) { len = decode_gif(tmp, len, out, mode, diffFound); }
            }
            fclose(tmp);
        } else {
            for(int j = 0; j < len; ++j) {
                if(!(j & 0xfff)) { en.print_status(); }
                if(mode == FDECOMPRESS) { putc(en.decompress(), out); }
                else if(mode == FCOMPARE) {
                    if(en.decompress() != fgetc(out) && !diffFound) {
                        mode = FDISCARD;
                        diffFound = i + j + 1;
                    }
                } else { en.decompress(); }
            }
        }
        i += len;
    }
    return diffFound;
}

#ifdef WINDOWS
#include <windows.h>
#endif

// Try to make a directory, return true if successful
bool makedir(const char *dir) {
#ifdef WINDOWS
    return CreateDirectory(dir, 0)==TRUE;
#else
#ifdef UNIX
    return mkdir(dir, 0777) == 0;
#else
    return false;
#endif
#endif
}

// Decompress a file
void decompress(const char *filename, long filesize, Encoder &en) {
    FMode mode = FDECOMPRESS;
    assert(en.getMode() == DECOMPRESS);
    assert(filename && filename[0]);

    // Test if output file exists.  If so, then compare.
    FILE *f = fopen(filename, "rb");
    if(f) { mode = FCOMPARE, printf("Comparing"); }
    else {
        // Create file
        f = fopen(filename, "wb");
        if(!f) {  // Try creating directories in path and try again
            String path(filename);
            for(int i = 0; path[i]; ++i) {
                if(path[i] == '/' || path[i] == '\\') {
                    char savechar = path[i];
                    path[i] = 0;
                    if(makedir(path.c_str())) {
                        printf("Created directory %s\n", path.c_str());
                    }
                    path[i] = savechar;
                }
            }
            f = fopen(filename, "wb");
        }
        if(!f) { mode = FDISCARD, printf("Skipping"); } else { printf("Extracting"); }
    }
    printf(" %s %ld -> ", filename, filesize);

    // Decompress/Compare
    int r = decompressRecursive(f, filesize, en, mode);
    if(mode == FCOMPARE && !r && getc(f) != EOF) { printf("file is longer\n"); }
    else if(mode == FCOMPARE && r) { printf("differ at %d\n", r - 1); }
    else if(mode == FCOMPARE) { printf("identical\n"); }
    else { printf("done   \n"); }
    if(f) { fclose(f); }
}

// To compress to file1.paq8px: paq8px [-n] file1 [file2...]
// To decompress: paq8px file1.paq8px [output_dir]
int main(int argc, char** argv) {
    bool pause=argc<=2;  // Pause when done?
    try {

        // Get option
        bool doExtract=false;  // -d option
        bool doList=false;  // -l option
        if (argc>1 && argv[1][0]=='-' && argv[1][1] && !argv[1][2]) {
            if (argv[1][1]>='0' && argv[1][1]<='8')
                level=argv[1][1]-'0';
            else if (argv[1][1]=='d')
                doExtract=true;
            else if (argv[1][1]=='l')
                doList=true;
            else
                quit("Valid options are -0 through -8, -d, -l\n");
            --argc;
            ++argv;
            pause=false;
        }

        // Print help message
        if (argc<2) {
            printf(PROGNAME " archiver (C) 2016, Matt Mahoney et al.\n"
                    "Free under GPL, http://www.gnu.org/licenses/gpl.txt\n\n"
#ifdef WINDOWS
            "To compress or extract, drop a file or folder on the "
        PROGNAME " icon.\n"
        "The output will be put in the same folder as the input.\n"
        "\n"
        "Or from a command window: "
#endif
                    "To compress:\n"
                    "  " PROGNAME " -level file               (compresses to file." PROGNAME ")\n"
                    "  " PROGNAME " -level archive files...   (creates archive." PROGNAME ")\n"
                    "  " PROGNAME " file                      (level -%d, pause when done)\n"
                    "level: -0 = store, -1 -2 -3 = faster (uses 35, 48, 59 MB)\n"
                    "-4 -5 -6 -7 -8 = smaller (uses 133, 233, 435, 837, 1643 MB)\n"
#if defined(WINDOWS) || defined (UNIX)
                    "You may also compress directories.\n"
#endif
                    "\n"
                    "To extract or compare:\n"
                    "  " PROGNAME " -d dir1/archive." PROGNAME "      (extract to dir1)\n"
                    "  " PROGNAME " -d dir1/archive." PROGNAME " dir2 (extract to dir2)\n"
                    "  " PROGNAME " archive." PROGNAME "              (extract, pause when done)\n"
                    "\n"
                    "To view contents: " PROGNAME " -l archive." PROGNAME "\n"
                    "\n",
                    DEFAULT_OPTION);
            quit();
        }

        FILE* archive=0;  // compressed file
        int files=0;  // number of files to compress/decompress
        Array<const char*> fname(1);  // file names (resized to files)
        Array<long> fsize(1);   // file lengths (resized to files)

        // Compress or decompress?  Get archive name
        Mode mode=COMPRESS;
        String archiveName(argv[1]);
        {
            const int prognamesize=strlen(PROGNAME);
            const int arg1size=strlen(argv[1]);
            if (arg1size>prognamesize+1 && argv[1][arg1size-prognamesize-1]=='.'
                && equals(PROGNAME, argv[1]+arg1size-prognamesize)) {
                mode=DECOMPRESS;
            }
            else if (doExtract || doList)
                mode=DECOMPRESS;
            else {
                archiveName+=".";
                archiveName+=PROGNAME;
            }
        }

        // Compress: write archive header, get file names and sizes
        String header_string;
        String filenames;
        if (mode==COMPRESS) {

            // Expand filenames to read later.  Write their base names and sizes
            // to archive.
            int i;
            for (i=1; i<argc; ++i) {
                String name(argv[i]);
                int len=name.size()-1;
                for (int j=0; j<=len; ++j)  // change \ to /
                    if (name[j]=='\\') name[j]='/';
                while (len>0 && name[len-1]=='/')  // remove trailing /
                    name[--len]=0;
                int base=len-1;
                while (base>=0 && name[base]!='/') --base;  // find last /
                ++base;
                if (base==0 && len>=2 && name[1]==':') base=2;  // chop "C:"
                int expanded=expand(header_string, filenames, name.c_str(), base);
                if (!expanded && (i>1||argc==2))
                    printf("%s: not found, skipping...\n", name.c_str());
                files+=expanded;
            }

            // If there is at least one file to compress
            // then create the archive header.
            if (files<1) quit("Nothing to compress\n");
            archive=fopen(archiveName.c_str(), "wb+");
            if (!archive) perror(archiveName.c_str()), quit();
            fprintf(archive, PROGNAME "%c%d", 0, level);
            printf("Creating archive %s with %d file(s)...\n",
                   archiveName.c_str(), files);
        }

        // Decompress: open archive for reading and store file names and sizes
        if (mode==DECOMPRESS) {
            archive=fopen(archiveName.c_str(), "rb+");
            if (!archive) perror(archiveName.c_str()), quit();

            // Check for proper format and get option
            String header;
            int len=strlen(PROGNAME)+2, c, i=0;
            header.resize(len+1);
            while (i<len && (c=getc(archive))!=EOF) {
                header[i]=c;
                i++;
            }
            header[i]=0;
            if (strncmp(header.c_str(), PROGNAME "\0", strlen(PROGNAME)+1))
            printf("%s: not a %s file\n", archiveName.c_str(), PROGNAME), quit();
            level=header[strlen(PROGNAME)+1]-'0';
            if (level<0||level>8) level=DEFAULT_OPTION;
        }

        // Set globals according to option
        assert(level>=0 && level<=8);
        buf.setsize(MEM*8);
        Encoder en(mode, archive, level);

        // Compress header
        if (mode==COMPRESS) {
            int len=header_string.size();
            printf("\nFile list (%d bytes)\n", len);
            assert(en.getMode()==COMPRESS);
            long start=en.size();
            en.compress(0); // block type 0
            en.compress(len>>24); en.compress(len>>16); en.compress(len>>8); en.compress(len); // block length
            for (int i=0; i<len; i++) en.compress(header_string[i]);
            printf("Compressed from %d to %ld bytes.\n",len,en.size()-start);
        }

        // Decompress header
        if (mode==DECOMPRESS) {
            if (en.decompress()!=0) printf("%s: header corrupted\n", archiveName.c_str()), quit();
            int len=0;
            len+=en.decompress()<<24;
            len+=en.decompress()<<16;
            len+=en.decompress()<<8;
            len+=en.decompress();
            header_string.resize(len);
            for (int i=0; i<len; i++) {
                header_string[i]=en.decompress();
                if (header_string[i]=='\n') files++;
            }
            if (doList) printf("File list of %s archive:\n%s", archiveName.c_str(), header_string.c_str());
        }

        // Fill fname[files], fsize[files] with input filenames and sizes
        fname.resize(files);
        fsize.resize(files);
        char *p=&header_string[0];
        char* q=&filenames[0];
        for (int i=0; i<files; ++i) {
            assert(p);
            fsize[i]=atol(p);
            assert(fsize[i]>=0);
            while (*p!='\t') ++p; *(p++)='\0';
            fname[i]=mode==COMPRESS?q:p;
            while (*p!='\n') ++p; *(p++)='\0';
            if (mode==COMPRESS) { while (*q!='\n') ++q; *(q++)='\0'; }
        }

        // Compress or decompress files
        assert(fname.size()==files);
        assert(fsize.size()==files);
        long total_size=0;  // sum of file sizes
        for (int i=0; i<files; ++i) total_size+=fsize[i];
        if (mode==COMPRESS) {
            for (int i=0; i<files; ++i) {
                printf("\n%d/%d  Filename: %s (%ld bytes)\n", i+1, files, fname[i], fsize[i]);
                compress(fname[i], fsize[i], en);
            }
            en.flush();
            printf("\nTotal %ld bytes compressed to %ld bytes.\n", total_size, en.size());
        }

            // Decompress files to dir2: paq8px -d dir1/archive.paq8px dir2
            // If there is no dir2, then extract to dir1
            // If there is no dir1, then extract to .
        else if (!doList) {
            assert(argc>=2);
            String dir(argc>2?argv[2]:argv[1]);
            if (argc==2) {  // chop "/archive.paq8px"
                int i;
                for (i=dir.size()-2; i>=0; --i) {
                    if (dir[i]=='/' || dir[i]=='\\') {
                        dir[i]=0;
                        break;
                    }
                    if (i==1 && dir[i]==':') {  // leave "C:"
                        dir[i+1]=0;
                        break;
                    }
                }
                if (i==-1) dir=".";  // "/" not found
            }
            dir=dir.c_str();
            if (dir[0] && (dir.size()!=3 || dir[1]!=':')) dir+="/";
            for (int i=0; i<files; ++i) {
                String out(dir.c_str());
                out+=fname[i];
                decompress(out.c_str(), fsize[i], en);
            }
        }
        fclose(archive);
//        if (!doList) programChecker.print(); // TODO
    }
    catch(const char* s) {
        if (s) printf("%s\n", s);
    }
    if (pause) {
        printf("\nClose this window or press ENTER to continue...\n");
        getchar();
    }
    return 0;
}
