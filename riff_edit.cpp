#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

#include "toojpeg.h"

const uint32_t MJPG = 'GPJM';
const uint32_t RIFF = 'FFIR';
const uint32_t LIST = 'TSIL';
const uint32_t JUNK = 'KNUJ';
const uint32_t INFO = 'OFNI';
const uint32_t VPRP = 'prpv';
const uint32_t MOVI = 'ivom';
const uint32_t IDX1 = '1xdi';
const uint32_t INDX = 'xdni';
const uint32_t ODML = 'lmdo';
const uint32_t AVIH = 'hiva';
const uint32_t STRN = 'nrts';
const uint32_t STRH = 'hrts';
const uint32_t STRF = 'frts';
const uint32_t DC00 = 'cd00';
const uint32_t WB01 = 'bw10';

typedef struct _avimainheader {
    //uint32_t fcc;
    //uint32_t  cb;
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} AVIMAINHEADER;

typedef struct _avistreamheader {
    //uint32_t fcc;
    //uint32_t cb;
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    struct {
        uint16_t left;
        uint16_t top;
        uint16_t right;
        uint16_t bottom;
    }  rcFrame;
} AVISTREAMHEADER;

typedef struct tagBITMAPINFOHEADER {
    uint32_t biSize;
    uint32_t biWidth;
    uint32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    uint32_t biXPelsPerMeter;
    uint32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
    uint32_t dwChunkId;
    uint32_t dwFlags;
    uint32_t dwOffset;
    uint32_t dwSize;
} AVIINDEX;

struct chunk_s
{
    uint32_t id=0;
    uint32_t size=0;
    uint32_t name=0;
    char* data=nullptr;

    std::vector<chunk_s> sub;
};

inline uint32_t compute_align(int32_t align, uint32_t offset)
{
    return (offset + (align - 1)) & -align;
}

std::vector<unsigned char> vid_data;
void store_byte(unsigned char b)
{
    vid_data.push_back(b);
}

std::vector<chunk_s> movi;
std::vector<AVIINDEX> idx;
uint32_t vid_frames = 0;
uint32_t offset = 4;
uint32_t read_chunks(std::ifstream& input, int32_t align, int32_t output_align, chunk_s& root)
{
    static bool sh_vid_found = false;
    static bool sf_vid_found = false;
    static uint32_t width = 0;
    static uint32_t height = 0;

    input.read((char*)&root.id, sizeof(root.id));
    input.read((char*)&root.size, sizeof(root.size));
    int ret_size = root.size;

    if (root.id == RIFF || root.id == LIST) {
        input.read((char*)&root.name, sizeof(root.name));
        for (uint32_t cnt = sizeof(root.name); cnt < root.size; ) {
            root.sub.emplace_back();
            cnt += read_chunks(input, align, output_align, root.sub.back());
        }
    }
    else {
        root.data = new char[root.size];
        input.read(root.data, root.size);

        if (!sh_vid_found && root.id == STRH) {
            sh_vid_found = true;
            AVISTREAMHEADER* ash = (AVISTREAMHEADER*)root.data;
            ash->fccHandler = MJPG;
            ash->dwLength = 0;
            ash->dwSuggestedBufferSize = 0;
            ash->dwQuality = 0;

            width = ash->rcFrame.right;
            height = ash->rcFrame.bottom;
        }
        else if (!sf_vid_found && root.id == STRF) {
            sf_vid_found = true;
            BITMAPINFOHEADER* asf = (BITMAPINFOHEADER*)root.data;
            asf->biWidth = width;
            asf->biHeight = height;
            asf->biCompression = MJPG;
        }
        else if (root.id == DC00) {
            vid_frames++;
            vid_data.clear();
            TooJpeg::writeJpeg(store_byte, root.data, width, height, true, 50);
            delete[] root.data;

            root.data = new char[vid_data.size()];
            root.size = (uint32_t)vid_data.size();
            std::copy(vid_data.begin(), vid_data.end(), root.data);

            movi.push_back(root);
            idx.emplace_back(root.id, 1, offset, compute_align(output_align, root.size));
            offset += idx.back().dwSize + 8;
        }
        else if (root.id == WB01) {
            movi.push_back(root);
            idx.emplace_back(root.id, 1, offset, compute_align(output_align, root.size));
            offset += idx.back().dwSize + 8;
        }
    }

    char padding[3];
    uint32_t padding_size = compute_align(align, ret_size) - ret_size;
    input.read(padding, padding_size);

    return ret_size + 8 + padding_size;
}

void prune_chunks(int32_t align, chunk_s& root)
{
    uint32_t new_size = (root.id == LIST || root.id == RIFF) ? sizeof(root.name) : 0;
    std::erase_if(root.sub,
        [&](chunk_s& x) {
            if (x.id == JUNK || x.id == VPRP || x.id == INDX || x.id == STRN || x.name == ODML) {
                delete[] x.data;
                return true;
            }
            if (x.id == LIST && x.name == INFO) {
                for (auto& s : x.sub) {
                    if (s.data) {
                        delete[] s.data;
                    }
                }
                return true;
            }
            if (x.id == AVIH) {
                AVIMAINHEADER* ah = (AVIMAINHEADER*)x.data;
                ah->dwTotalFrames = vid_frames;
            }
            if (x.id == IDX1) {
                delete[] x.data;
                x.data = new char[idx.size() * sizeof(AVIINDEX)];
                char* ptr = x.data;
                if (idx[0].dwChunkId == WB01) {
                    std::swap(idx[0], idx[1]);
                    idx[0].dwOffset = 4;
                    idx[1].dwOffset = 4 + compute_align(align, idx[0].dwSize);
                }
                for (auto& ai : idx) {
                    std::copy((char*)&ai, ((char*)&ai) + sizeof(ai), ptr);
                    ptr += sizeof(ai);
                }
                x.size = (uint32_t)idx.size() * sizeof(AVIINDEX);
            }
            if (x.id == LIST || x.id == RIFF) {
                if (x.name == MOVI) {
                    x.sub = movi;
                    if (x.sub[0].id == WB01) {
                        std::swap(x.sub[0], x.sub[1]);
                    }
                }
                prune_chunks(align, x);
            }
            new_size += x.size + 8 + (compute_align(align, x.size) - x.size);
            return false;
        }
    );
    root.size = new_size;
}

void write_chunks(std::ofstream& output, int32_t align, chunk_s& root)
{
    output.write((char*)&root.id, sizeof(root.id));
    output.write((char*)&root.size, sizeof(root.size));
    if (root.id == RIFF || root.id == LIST) {
        output.write((char*)&root.name, sizeof(root.name));
    }
    else {
        output.write(root.data, root.size);
        char padding[3] = {0};
        output.write(padding, compute_align(align, root.size) - root.size);
    }
    for (auto& x : root.sub) {
        write_chunks(output, align, x);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cout << "Usage: riff_edit input.avi output.avi 2 4\n";
        return -1;
    }

    std::ifstream input(argv[1], std::ios::binary);
    std::ofstream output(argv[2], std::ios::binary);

    int32_t input_align = 2;
    int32_t output_align = 4;
    if (argc == 5) {
        input_align = argv[3][0] - '0';
        output_align = argv[4][0] - '0';
    }

    chunk_s root {};
    read_chunks(input, input_align, output_align, root);
    chunk_s avix{};
    do {
        avix.id = 0;
        read_chunks(input, input_align, output_align, avix);
    } while (avix.id);
    prune_chunks(output_align, root);
    write_chunks(output, output_align, root);

    return 0;
}
